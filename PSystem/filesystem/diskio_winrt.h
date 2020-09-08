// p-system file system emulator to allow direct access to a Windows folder
// well, within limits; the limits of the p-system
// Copyright (C) 2013 Frank Seide

#pragma once

#include <ctype.h>      // for toupper()
#include <string>

// directory structure
static const size_t PFSBLKSIZE = 512;       // number of bytes in a single block
static const size_t PFSSECSIZE = 256;       // we are called with Apple-size sectors instead of blocks

#pragma pack (push)
#pragma pack (1)

enum PFSFileKind
{
     UNTYPEDFILE, XDSKFILE, CODEFILE, TEXTFILE, INFOFILE, DATAFILE, GRAFFILE, FOTOFILE, SECUREDIR
};

// date/time
struct PFSDateRec
{
    unsigned __int16 month : 4;
    unsigned __int16 day : 5;
    unsigned __int16 year : 7;    // "100 is temp disk flag"
    PFSDateRec() : month(3), day(15), year(68) { }  // (some initializer)
    PFSDateRec(const __time64_t & date);
};
static_assert(sizeof PFSDateRec == 2, "PFSDateRec size must be 2 bytes large precisely");

template<size_t length>
struct PStringStore
{
    unsigned char numChars;
    unsigned char chars[length];
    //size_t size() { return numChars; }
    //std::string string() { return std::string(&chars[0], &chars[numChars]); }   // return as std::string
    // std::string like interface
    static size_t capacity() { return length; }
    size_t size() const { return numChars; }
    const char * begin() const { return (const char *) &chars[0]; }
    const char * end() const { return (const char *) &chars[0] + size(); }
    size_t slen(const char * s) { return strlen(s); }
    size_t slen(const wchar_t * s) { return wcslen(s); }
    int strcmp(const PStringStore & other)  const   // return this <=> other
    {
        for (size_t i = 0; i < numChars && i < other.numChars; i++)
        {
            int diff = (int) chars[i] - (int) other.chars[i];
            if (diff != 0)
                return diff;
        }
        // same prefix--how about the length?
        return (int) numChars - (int) other.numChars;
    }
    bool begins_with(const PStringStore & prefix)  const   // for filename completion
    {
        for (size_t i = 0; i < prefix.numChars; i++)
            if (i >= numChars || chars[i] != prefix.chars[i])
                return false;
        return true;
    }
    template<typename CHAR> void assign(const CHAR * s)
    {
        size_t n = slen (s);
        // how to do this without including STL? STL is incompatible with the C sources...
        //if (n > length)
        //    throw std::invalid_argument("assign: string too long");
        for (size_t i = 0; i < length; i++)
            chars[i] = i < n ? toupper(s[i]) : 0;    // or ' '?
        numChars = (unsigned char) n;
    }
    std::string tostring() const
    {
        return string(begin(), end());
    }
};

typedef PStringStore<7> PFSVolumeName;
typedef PStringStore<15> PFSFileName;

struct PFSDirEntry
{
    unsigned __int16 beginBlock;        // first block of file
    unsigned __int16 endBlock;          // last block +1
    unsigned __int16/*PFSFileKind*/ fileKind : 4;           // PFSFileKind as 8 bit number
    unsigned __int16 padding : 12;
    PFSFileName fileName;               // file name
    unsigned __int16 bytesInLastBlock;  // # bytes in last block (min 1)
    PFSDateRec lastModificationTime;    // last modification date
    size_t size() const { return (endBlock - beginBlock - 1) * 512 + bytesInLastBlock; }    // get the size of the file
    bool istemp() const { return lastModificationTime.year == 100; }  // year '100' indicates a temp file
    std::string todebugstring() const   // for debug messages
    {
        std::string s = fileName.tostring();
        char buf[100];
        sprintf_s(buf, " (%d,%d:%d)", (int) beginBlock, (int) size(), (int) fileKind);
        s += buf;
        if (istemp())
            s += " [temp]";
        return s;
    }
};
static_assert(sizeof PFSDirEntry == 26, "PFSDirEntry size must be 24 bytes large precisely");

// the directory is 1024 bytes long and is stored in blocks 2 and 3
struct PFSDirectory
{
    // directory header
    static const size_t dirBeginBlock = 2;
    unsigned __int16 beginBlock;            // first block of file  --what's this for a directory? Whole disk?
    unsigned __int16 endBlock;              // 6 or 10 --10 means duplicate directory backup
    unsigned __int16 fileTypeForDirHeader;
    PFSVolumeName volumeName;
    unsigned __int16 numBlocksOnVolume;     // e.g. 280 for Apple disks
    unsigned __int16 numFiles;              // note: limited to maximally _countof(files); TODO: remove this limit
    unsigned __int16 timeOfLastAccess;      // when was directory last loaded from the disk to (Pascal) RAM?
    PFSDateRec lastBootDate;                // MOST RECENT DATE SETTING --read at boot time as THEDATE (in case no built-in clock?)
    char padding[4];
    // directory entries  --77 fit into 1024 bytes
    // TODO: make this a variable thing; SYSTEM treats it as such
    PFSDirEntry files[77];
    char padding2[20];                      // total size == 2048
    size_t size() const                     // size of directory in bytes
    { 
        const auto * dirEnd = &files[0] + numFiles;
        return (size_t) ((const char *) dirEnd - (const char *) this);
    }
    PFSDirectory(size_t totalSizeInBytes);
};
#pragma pack (pop)

static_assert(sizeof PFSDirectory == 2048, "PFSDiretory size must be 4 disk blocks, i.e. 2048 bytes precisely");

class EmulatedPFSDisk
{
    class PFSDiskEmulator * inner;
public:
    EmulatedPFSDisk(const char * folder, size_t size);
    EmulatedPFSDisk(const wchar_t * folder, size_t size);
    ~EmulatedPFSDisk();
    int WriteSectors(size_t sector, const char * buf, size_t bytes);
    int ReadSectors(size_t sector, char * buf, size_t bytes);
    std::string FindFileByBlock(size_t block) const;
    std::string DiskPathForFile(const std::string & title) const;
    void SuspendResume(class psnapshot * s);
};
