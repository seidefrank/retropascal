// p-code interpreter --code to handle snapshots
// (C) 2013 Frank Seide

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>

void dprintf(const char * format, ...);

class psnapshot
{
    // a snapshot is stored as a set of named blocks
    // To save RAM, we share identical blocks across snapshots.
    std::map<std::string,std::shared_ptr<std::vector<char>>> blocks;       // [name] -> byte array
    // args and state used during creation (avoid having to pass that through all the time)
    psnapshot * prev;               // previous snapshot to share memory with in case of identical blocks
    std::string prefix;             // current name prefix (units don't know their own number/name)
    size_t snapshotsize;            // bytes stored
    size_t snapshotnonsharedsize;   // (for debugging: actually non-shared size upon creation; becomes invalid if shared object gets destructed)
    const void * rambase;           // for ptr conversion
    const void * ramtop;
public:
    int id;                         // (for debugging)
    bool resuming;                  // direction: true=get, false=put
    psnapshot(psnapshot * prev, const void * rambase, const void * ramtop) :
        prev(prev), resuming(false/*assume we create it*/), rambase(rambase), ramtop(ramtop)
    {
        static int idcounter = 1;   // (for debugging)
        id = idcounter++;           // assing a unique id to the snapshot
        snapshotsize = snapshotnonsharedsize = 0;
        dprintf("psnapshot: id = %d\n",id);
    }
    void finalize()
    {
        dprintf("finalize: %d bytes newly allocated for %d total bytes captured\n", (int) snapshotnonsharedsize, (int) snapshotsize);
        prev = nullptr;             // this was temporary
        prefix.clear();
        resuming = true;            // from now on is used for resuming
    }
    // units don't know their own name, so generic snapshot loop sets the unit name for us
    void setprefix(const char * fmt, int iarg)
    {
        char buf[20];
        sprintf_s(buf, fmt, iarg);
        prefix = buf;
    }
    // add a block
    // If the content is identical to the same block in prev then just share it.
    void block(std::string name, std::shared_ptr<std::vector<char>> & data, size_t expectedsize/*resuming*/)
    {
        name.insert(name.begin(), prefix.begin(), prefix.end());       // insert prefix
        dprintf("block: %sing '%s', %d bytes\n", resuming ? "resum" : "suspend", name.c_str(), expectedsize);
        if (resuming)
        {
            auto iter = blocks.find(name);
            if (iter == blocks.end())
                throw std::runtime_error("block: trying to resume by non-existent block name");
            if (expectedsize != SIZE_MAX && iter->second->size() != expectedsize)   // possible cause: suspend from x64, resuming to Win32
                throw std::runtime_error("block: trying to resume a block with unexpected size");
            data = iter->second;
        }
        else
        {
            if (data->size() != expectedsize)   // (sanity check)
                throw std::runtime_error("block: trying to suspend a block with inconsistent size");
            snapshotsize += data->size();
            snapshotnonsharedsize += data->size();
            if (prev)
            {
                auto iter = prev->blocks.find(name);
                if (iter != prev->blocks.end())
                {
                    std::shared_ptr<std::vector<char>> & other = iter->second;
                    if (data.get() == other.get()           // (caller cached the data object)
                        || (data->size() == other->size() && memcmp(data->data(), other->data(), data->size()) == 0)) // binary the same
                    {
                        dprintf("block: sharing block %s\n", name.c_str());
                        snapshotnonsharedsize -= data->size();
                        data = other;   // delete our new copy, and just share the old copy instead
                        // note that the data arg is a &, so don't touch the data content after the call to block()
                    }
                }
            }
            // insert into map
            auto res = blocks.insert(make_pair(name, data));
            if (!res.second)
                throw std::logic_error("block: trying to add a block with the same name as a previous block");
        }
    }
    // add a contiguous area of RAM
    void ramblocks(const char * nameprefix, void * vbytes, size_t length, size_t blocksize)
    {
        char * bytes = (char *) vbytes;
        char namebuf[100];
        for (size_t offset = 0; offset < length; offset += blocksize)
        {
            size_t thisblocksize = std::min(blocksize, length - offset);
            sprintf_s(namebuf, "%s_%05x", nameprefix, offset);
            if (resuming)
            {
                std::shared_ptr<std::vector<char>> data;
                block(namebuf, data, thisblocksize);
                memcpy(bytes + offset, data->data(), thisblocksize);
            }
            else
            {
                block(namebuf, std::shared_ptr<std::vector<char>>(new std::vector<char>(bytes + offset, bytes + offset + thisblocksize)), thisblocksize);
            }
        }
    }
    // other types
    template<typename T>
    void ptr(const char * name, T * & p)
    {
        if (resuming)
        {
            ptrdiff_t rel = 0;   // (to keep compiler happy)
            scalar(name, rel);
            p = rel != -1 ? (T*) (rel + (char*) rambase) : nullptr;
        }
        else
        {
            // nullptr is represented as -1
            ptrdiff_t rel = p ? intptr_t(p) - intptr_t(rambase) : -1;
            scalar(name, rel);
        }
        if (p && (p < rambase || p >= ramtop))
            throw runtime_error("ptr: cannot (de-)serialize pointers that point outside of emulated RAM");
    }
    void str(const char * name, std::string & p)
    {
        if (resuming)
        {
            std::shared_ptr<std::vector<char>> data;
            block(name, data, SIZE_MAX);
            //p.assign(data->data(), data->data() + data->size());
            p = std::string(data->data(), data->data() + data->size());   // direct assign() fails with 'incompatible iterator'
        }
        else
        {
            block(name, std::shared_ptr<std::vector<char>>(new std::vector<char>(p.c_str(), p.c_str() + p.size())), p.size());
        }
    }
    template<typename ELEMTYPE>
    void vec(const char * name, std::vector<ELEMTYPE> & v)  // NOTE: poorly tested
    {
        if (resuming)
        {
            std::shared_ptr<std::vector<char>> data;
            block(name, data, SIZE_MAX);
            v.resize(data->size() / sizeof(ELEMTYPE));
            if (!data->empty())
                memcpy(v.data(), data->data(), data->size());
        }
        else
        {
            char * p = (char *) v.data();
            size_t n = v.size() * sizeof(ELEMTYPE);
            block(name, std::shared_ptr<std::vector<char>>(new std::vector<char>(p, p + n)), n);
        }
    }
    template<typename T>
    void scalar(const char * name, T & ival)      // note: also used for structs and flat arrays
    {
        if (resuming)
        {
            std::shared_ptr<std::vector<char>> data;
            block(name, data, sizeof(ival));
            memcpy(&ival, data->data(), sizeof(ival));
        }
        else
        {
            char * ip = (char *) &ival;
            block(name, std::shared_ptr<std::vector<char>>(new std::vector<char>(ip, ip + sizeof(ival))), sizeof(ival));
        }
    }
    void strarray(const char * name, std::vector<std::string> & p)
    {
        if (resuming)
        {
            std::shared_ptr<std::vector<char>> pdata;
            block(name, pdata, SIZE_MAX);
            std::vector<char> data = *pdata.get();  // make a copy since we destroy it here
            p.clear();
            while (!data.empty())
            {
                if (data.back() != 0)
                    throw std::runtime_error("strarray: not 0-terminated");
                p.push_back(data.data());  // 0-terminated string
                data.erase(data.begin(), data.begin() + strlen(data.data())+1);
            }
        }
        else
        {
            std::shared_ptr<std::vector<char>> data(new std::vector<char>());
            data->reserve(8000);
            for (size_t k = 0; k < p.size(); k++)
                data->insert(data->end(), p[k].c_str(), p[k].c_str() + p[k].size() + 1);
            block(name, data, data->size());
        }
    }
    // serialization/deserialization
    // serialized format:
    //  - magic 'BPKG'
    //  - #blocks, 4 bytes
    //  - each block:
    //     - magic 'BBLK'
    //     - name length, 1 byte
    //     - name bytes
    //     - block size, 4 bytes
    //     - block bytes
    //     - magic 'EBLK'
    //  - magic 'EPKG'
    // suspend: serialize to byte vector
private:
    static void pushint(std::vector<unsigned char> & pkg, int val)
    {
        const unsigned char * pval = (const unsigned char *) &val;
        pkg.insert (pkg.end(), pval, pval + sizeof (val));
    }
    static void canpop(size_t n, const std::vector<unsigned char>::const_iterator & p, const std::vector<unsigned char>::const_iterator & q)
    {
        if (p + n > q)
            throw std::runtime_error("canpop: invalid serialization package (unexpected end)");
    }
    static int popint(std::vector<unsigned char>::const_iterator & p, const std::vector<unsigned char>::const_iterator & q)
    {
        int val;
        canpop(sizeof(val), p, q);
        unsigned char * pval = (unsigned char *) &val;
        memcpy(&val, &*p, sizeof(val));
        p += sizeof(val);
        return val;
    }
    static void popmagic(int expectedmagic, std::vector<unsigned char>::const_iterator & p, const std::vector<unsigned char>::const_iterator & q)
    {
        int magic = popint(p, q);
        if (magic != expectedmagic)
            throw std::runtime_error("popmagic: invalid serialization package (expected magic number not found)");
    }
public:
    std::vector<unsigned char> serialize() const
    {
        std::vector<unsigned char> pkg;
        pkg.reserve(140000);
        pushint (pkg, 'BPKG');
        pushint (pkg, blocks.size());
        for (auto iter : blocks)
        {
            const std::string & name = iter.first;
            const auto & data = *iter.second.get();
            pushint (pkg, 'BBLK');
            // name
            if (name.size() > 255)
                throw std::runtime_error("serialize: block name too long");
            pkg.push_back ((unsigned char) name.size());
            const unsigned char * pname = (const unsigned char *) name.c_str();
            pkg.insert (pkg.end(), pname, pname + name.size());
            // block data
            if (data.size() > INT_MAX)
                throw std::runtime_error("serialize: block data too large");
            pushint (pkg, (int) data.size());
            const unsigned char * pdata = (const unsigned char *) data.data();
            pkg.insert (pkg.end(), pdata, pdata + data.size());
            pushint (pkg, 'EBLK');
        }
        pushint (pkg, 'EPKG');
        dprintf("serialize: snapshot created, %d bytes\n", (int) pkg.size());
        return pkg;
    }
    // resume: deserialize from byte vector
    psnapshot(const std::vector<unsigned char> & snapshotimage, const void * rambase, const void * ramtop) :
        prev(nullptr), resuming(true), rambase(rambase), ramtop(ramtop)
    {
        auto p = snapshotimage.begin();
        auto q = snapshotimage.end();
        popmagic('BPKG', p, q);
        size_t numblocks = popint(p, q);
        for (size_t block = 0; block < numblocks; block++)
        {
            popmagic('BBLK', p, q);
            size_t namelen = *p++;
            canpop(namelen, p, q);
            std::string name(p, p + namelen);
            p += namelen;
            size_t datalen = popint(p, q);
            canpop(datalen, p, q);
            std::shared_ptr<std::vector<char>> data(new std::vector<char>(p, p + datalen));
            p += datalen;
            auto kvp = std::make_pair(std::move(name), std::move(data));
            auto res = blocks.insert (kvp);    // (we could do better with insert() and std::move() I think)
            popmagic('EBLK', p, q);
        }
        popmagic('EPKG', p, q);
    }
};
