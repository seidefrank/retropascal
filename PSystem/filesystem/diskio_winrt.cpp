// p-system file system emulator to allow direct access to a Windows folder
// well, within limits; the limits of the p-system
// Copyright (C) 2013 Frank Seide

// basic operation:
//  - we need to mitigate between UCSD's block-wise disk access and the Windows file system
//  - we operate in the UCSD block space
//  - we do not support block-level device access, such as binary disk copy
//     - blocks 0 and 1 are forbidden to hit, to prevent this
//  - each EmulatedPFSDisk instance holds its directory in Pascal format
//     - This is the ultimate master of all information.
//     - it fully defines the meanings of disk block numbers
//  - each dir entry in there has a backing file on the Windows side
//     - it may be lazily created and/or grown
//  - read/write accesses inside the block range of an entry goes to that backing file
//  - read/write accesses outside any block range and outside the directory are forbidden
//     - correct? How about extending a file? Does it reserve the blocks first?
//  - the assumed write sequence from UCSD Pascal is:
//     - for a new file, a dir entry is created and written back (cf. FOPEN)
//     - this entry reserves the maximum expected number of blocks (possibly "INFINITE"); this is noted in the dir entry that's written to disk, so we know it
//     - write accesses only happen within this declared and noted block range
//     - any time the dir is written back, we sync all information back to the backing file
//        - name change
//        - deletion (given block range no longer occupied)
//        - change from TMP to on-TMP status (also name change of backing file)
//        - TEXT files have their partner files sync'ed
//        - if the volume name has changed, the $VOLNAME.volumename file is renamed
//        - we read back the directory; newly created files are added, but files gone missing are kept in the dir
//     - if the dir entry changes its name, the backing file is renamed accordingly
//  - we assume that directories are always written in one write operation
//     - WRITEDIR: UNITWRITE(FUNIT,FDIR^, (DNUMFILES+1)*SIZEOF(DIRENTRY),DIRBLK);
//     - however, this module is called sector by sector (we should fix that! makes no longer sense)

// needed change:
//  - volume folders named after their drive unit number
//  - special file $VOLNAME.volumename is the volume name

// coming some day:
//  - subdirectories ("catalog files")
//  - can we support larger dir entries?

// earlier write-up, not sure if up-to-date
// redesign:
//  - files inside the p-system are uniquely identified by their block range; we map the Windows dir to fake block ranges
//  - block accesses fall into three categories:
//     - directory access
//     - file access by (fake) block number; emulator maps fake block numbers back to Windows files
//        - files that are written have a temp dir entry created for them by the Pascal system,
//          and could be identified this way
//        - files that are read cannot be identified other than tracking FOPEN/FCLOSE calls and the SYSCOM runtime segment table
//     - any other block range (access will cause IO errors)
//        - TODO: will this naturally block Filer's K(runch function?
//  - each time the directory blocks are requested to be read, we can update the fake block ranges in the directory,
//    and thus pick up changes in the Windows file system; EXCEPT:
//    TODO: verify this against Pascal source code
//  - files that are in-use (i.e. some part of the Pascal system holdson to the fake block numbers)
//    must keep their fake block number assignments upon reloading the dir. There are two kinds of this case:
//     - files in the runtime segment table
//     - files that are actually open in an FIB (we'd need to track FOPEN/FCLOSE to know their locations)
//    TODO: any more cases?
//  - each time a directory is written, we can 'diff' the two file sets to know what the changes were
//  - on-disk storage:
//     - known types are denoted by extension .TYPE, and if extension is missing or wrong, by appending [TYPE]
//     - .TEXT files are stored and synced in two versions: .TXT (Windows compatible) and .TEXT (Pascal-blocked version)
//     - temp files (during write); file names are derived from their block range
//  - allocation:
//     - Pascal creates a temp dir entry for newly written files until they are closed; with specified size:
//        - [n]: allocate this many (fake) blocks
//        - [0]: allocate the largest range  --this is a problem, e.g. cannot write compiler listing to same disk
//        - [*]: allocate max of second largest and half the largest (SCANTITLE returns FSEGS = -1 --> 'RT11ISH' flag in ENTERTEMP)
//     - we can fake the (fake) block range dir entry by limiting the size to the largest known UCSD system size (5 MB?), to avoid contention (assuming the total # blocks is large enough)
//     - we do so in our fake directory; FIB will still have MAXINT
//  - files with invalid SOS filenames are invisible

// supporting SOS pathname conventions:
//  - SOS conventions:
//     - device name: .DEVNAME instead of DEVNAME: (15 characters)
//     - volume name: /VOLNAME instead of VOLNAME: (15 characters)
//     - filenames can have subdirectories
//     - % denotes directory of current executable file (where SYSCOM runtime segment #1 lives)
//  - current conventions as implemented in SCANTITLE:
//     - * denotes boot disk (existing Pascal convention implemented in SCANTITLE)
//     - : denotes current prefix (existing Pascal convention implemented in SCANTITLE)
//  - we can even support Windows convention (backslash; with filename restrictions)
//  - FOPEN sequence:
//     - SCANTITLE -> LVID, LTID (also LSEGS, that is [] how-to-allocate spec if given)
//     - VOLSEARCH(LVID) -> LUNIT, LDIR
//     - DIRSEARCH(LTID, LDIR)
//     - temp entry created upon open-for-write
//        - ENTERTEMP (operates on LDIR in RAM; uses final filename but with YEAR=100 to denote temp entry)
//        - WRITEDIR
//     - dir entry -> saved in FIB
//  - FGET, FPUT, RESETER:
//     - direct write or read by (fake) block index in saved dir entry in FIB without further checks
//  - FCLOSE sequence:
//     - RESETER (flush buffers)
//     - VOLSEARCH(LVID from FIB) -> must return same as FUNIT in FIB (else ILOSTUNIT err)
//     - looks for precise match of block ranges against saved entry FIB (else ILOSTFILE err)
//     - CPURGE: delete file from LDIR in RAM
//     - writing: turn temp into regular file entry (possibly deleting existing one with same name)
//     - WRITEDIR
//  - VOLSEARCH updates SYSCOM.UNITABLE which keeps vol name for each unit
//  - possible way of faking it:
//     - intercept SCANTITLE -> special LVID that is an index into a sub-dir path table in emulator
//        - special LVID is entered in a table, and is also used for looking up files by (fake) block index
//        - special syntax, e.g. ~xxx or ending in /, to make sure we won't confuse it with an actual volume
//     - intercept VOLSEARCH
//        - match special LVID, return a faked LDIR for sub-directory, and LUNIT=physical unit
//        - do not update UNITABLE with faked LDIR entries, i.e. we keep the disk name there
//     - original DIRSEARCH will now search the sub-directory
//     - original FCLOSE will have remembered the special LVID and call VOLSEARCH (-> LDIR) and WRITEDIR with it
//     - life time of special LVID entry:
//        - for each file: until FCLOSE
//        - we may use the special LVID as DKVID; thus it must be kept around as well as long as it is in there
//        - otherwise until... TODO: next SCANTITLE? next dir read?
//     - TODO: do we still fake multiple volumes? or rather fake C:, D: (USB drive), ...??
//        - we could make SYSTEM: a special LVID for the root folder inside the C drive; and otherwise operate on the full filesystem!
//        - all on a single virtual disk volume #4
//  - uses of pathnames in SYSCOM:
//     - INFOREC (VID and TID) of (WORK, SYM, CODE)
//     - current prefix: DKVID
//     - system volume: SYVID
//        - this is OK--system volume is a root dir
//  - non-FOPEN uses of those functions:
//     - ASSOCIATE sets up SYSCOM runtime segtable by full pathname
//        - uses FOPEN (FIB is USERINFO.CODEFIB)
//           - then uses FIB info directly
//           - reads seg table, fails if unlinked (triggering Linker)
//           - copy segments 1 and 7+ to SYSCOM runtime table
//           - FCLOSE
//        - used here:
//           - X(ecute) command passes full path to ASSOCIATE
//           - R(un) command passes CODEVID:CODETID (INFOREC)
//              - can INFOREC paths be limited to root folders? Then no problem
//           - "*SYSTEM.STARTUP" at startup
//           - from SYS_ASSOCIATE
//     - SYS_ASSOCIATE sets up SYSCOM runtime segtable for system tools by their filename (e.g. SYSTEM.EDITOR)
//        - first tries ASSOCIATE; if fails:
//        - SCANTITLE to break path
//        - read all volume dirs one by one using FETCHDIR (--> root dir); try ASSOCIATE(VID:TITLE) with VID from fetched dir
//     - FILER:
//        - SCANTITLE, VOLSEARCH used by SCANINPUT to parse
//        - uses VOLSEARCH(,TRUE,) to update the SYSCOM.UNITTABLE
//        - TODO: ...to be continued!
//     - verified that it is not used by: COMPILER, LINKER, EDITOR

// design of clipboard device:
//  - REMIN:, REMOUT:
//  - maybe rename one into CLIPBRD:
//  - when reading, always provide .TEXT format (header; multiple of 1024; return 0 blocks for last read; cf. FGET)
//     - Apple editor allows that; does the Pascal II one? Not according to source code (editor will append .TEXT; FOPEN will fail on non-block device with a pathname)
//  - when writing, assume characters; end is detected by
//     - a read access (using the clipboard, or reading from the device)
//     - reset of block number (which must be in increasing order, we will keep track)

// TODOs:  --these are OLD!
//  - honor read-only mode  --currently it's all writable (we will not support forget mode)
//  - testing e.g. of random-access record writing from a Pascal program
//  - deadling with file dates --I just ignored it for now
//  - handling of volume-name changes
//  - what is Ex)amine bad blocks? "0..6 endangered, repair?" --will it damage it? Or just rewrite the boot blocks?
//  - can we pick up new files in the Windows system? E.g. upon call to UNITCLEAR--is it actually called any other time than at INITIALIZE?

#define _CRT_SECURE_NO_WARNINGS 1
#include "diskio_winrt.h"
#include "ptypes.h"     // for IO error codes (IORSLTWD)
#include "psnapshot.h"
#include "converttext.h"
#include <string>
#include <set>
#include <vector>
#include <map>
#include <io.h>         // for _chsize_s()
#include <errno.h>
#include <fcntl.h>      // for _O_ flags
#include <time.h>
using namespace std;
using namespace psystem;

static const size_t SECPERBLK = PFSBLKSIZE / PFSSECSIZE;
#define BLOCKEXT ".BLOCKFMT$$"    // extension for temporary converted TEXT files

// a little helper
template<class T> static void zerostruct(T & t)
{
    const size_t sz = sizeof(t);
    memset(&t, 0, sz);
}

// this does not exist properly on Windows... WORKAROUND
static errno_t truncate(const char * path, size_t size)
{
    errno_t ret = 0;
    int fd = _open(path, _O_BINARY | _O_WRONLY);
    if (fd < 0)
        ret = errno;
    if (ret == 0)
        ret = _chsize_s(fd, size);
    if (fd >= 0)
        _close(fd);
    return ret;
}
// is there a proper CRT/Windows equivalent?
static errno_t createfile(const char * path)
{
    errno_t ret = 0;
    FILE * f = fopen(path, "r+b");  // open existing (need read/write, "a" will kill fseek())
    if (f == NULL && errno == ENOENT)
        f = fopen(path, "wb");      // not there: create it
    if (f == NULL)
        ret = errno;
    else
        fclose(f);
    return ret;
}

// Windows file dates are not reliable, so we fake them ourselves
class FakeFileDates
{
    map<string,size_t> genmap;  // [path] -> generation index
    size_t genid;
    void deletefile(const string & file)
    {
        auto findfile = genmap.find(file);
        if (findfile != genmap.end())
        {
            genmap.erase(findfile);
            flushsuspendedgenmap();
        }
    }
    void renamefile(const string & from, const string & to)
    {
        copydate(from, to);
        deletefile(from);
    }
public:
    FakeFileDates() : genid(0) { }
    void touch(const string & path)
    {
        dprintf("touch: %s = %d\n", path.c_str(), (int) genid);
        genmap[path] = genid++;
        flushsuspendedgenmap();
    }
    void copydate(const string & from, const string & to)
    {
        auto findfrom = genmap.find(from);
        if (findfrom == genmap.end())
            throw invalid_argument("copydate: called for non-existent file");
        size_t fromgen = findfrom->second;
        genmap[to] = fromgen;
        flushsuspendedgenmap();
    }
    bool needtomake(const string & target, const string & dependency) const
    {
        bool result = false;
        auto findwhich = genmap.find(target);
        auto findwrt = genmap.find(dependency);
        if (findwrt == genmap.end())            // dependency does not exist: we are up-to-date, nothing to do
            result = false;
        else if (findwhich == genmap.end())     // target does not exist: must make it
            result = true;
        else
            result = findwrt->second > findwhich->second;
        dprintf("needtomake: %s %s from %s\n", result ? "YES" : "no", target.c_str(), dependency.c_str());
        return result;
    }
    // actual file operations that do keep track
    int unlink(const string & path)
    {
        dprintf("unlink: %s\n", path.c_str());
        int ret = _unlink(path.c_str());
        if (ret == 0 || errno == ENOENT)
            deletefile(path);
        return ret;
    }
    int rename(const string & from, const string & to)
    {
        dprintf("rename: %s to %s\n", from.c_str(), to.c_str());
        int ret = ::rename(from.c_str(), to.c_str());
        if (ret == 0)
            renamefile(from, to);
        return ret;
    }
    errno_t truncate(const char * path, size_t size)
    {
        dprintf("truncate: %s to %d bytes\n", path, (int) size);
        errno_t e = ::truncate(path, size);
        if (e == 0)
            touch(path);
        return e;
    }
    errno_t createfile(const char * path)
    {
        dprintf("createfile: %s\n", path);
        errno_t ret = 0;
        FILE * f = fopen(path, "r+b");      // open existing (need read/write, "a" will kill fseek())
        if (f == NULL && errno == ENOENT)   // not there
        {
            f = fopen(path, "wb");
            if (f)
                dprintf("   -> created\n");
            if (f)
                touch(path);                // created--keep track
        }
        if (f)
            fclose(f);
        return ret;
    }
    // suspend/resume
    // We cache the serialized generation map since it is expensive to serialize it upon every single character we type.
    std::shared_ptr<std::vector<char>> suspendedgenmap;         // as serialized
    void flushsuspendedgenmap() { suspendedgenmap.reset(); }    // something changed, suspendedgenmap out of date
    void suspendresume(psnapshot * s)
    {
        if (s->resuming)
        {
            s->block("genmap", suspendedgenmap, SIZE_MAX);
            vector<char> data = *suspendedgenmap.get();         // (make a copy since we are destroying it)
            genmap.clear();
            while (!data.empty())
            {
                size_t len = strnlen(data.data(), data.size());
                size_t second;
                if (len >= data.size() - sizeof(second))
                    throw std::runtime_error("strmap: not 0-terminated");
                auto first = std::string(data.data());  // 0-terminated string
                memcpy(&second, data.data() + len+1, sizeof(second));
                data.erase(data.begin(), data.begin() + strlen(data.data())+1+sizeof(second));
                genmap.insert(make_pair(first, second));
            }
        }
        else
        {
            if (!suspendedgenmap)  // we update lazily since this can be expensive
            {
                suspendedgenmap.reset(new std::vector<char>());
                for (auto iter = genmap.begin(); iter != genmap.end(); iter++)
                {
                    suspendedgenmap->insert(suspendedgenmap->end(), iter->first.c_str(), iter->first.c_str() + iter->first.size() + 1);
                    suspendedgenmap->insert(suspendedgenmap->end(), (char*) &iter->second, sizeof(iter->second) + (char*) &iter->second);
                }
            }
            // if no change, then this will result in a pointer comparison to previous snapshot (fast)
            s->block("genmap", suspendedgenmap, suspendedgenmap->size());
        }
    }
};

// this emulates a PFS disk on top of a Windows folder
// with limitations. Features:
//  - the file system has maximum size (32k blocks), but still only 77 files, 15 chars per filename
//  - discovering files at mount time, but files added to the Windows folder later are not discovered
//  - files may not be modified outside while the folder is mounted since length info is cached
//  - entire volume copies with Filer are allowed; we will auto-patch the volume to max size
//  - the system volume can be such a folder, i.e. no need for disk images anymore
//  - file types are detected by extension; all unknown extensions are undestood as DATA (including SYSTEM.xxx files)
class PFSDiskEmulator
{
    string path;                // path of folder that stores the disk content
    string currentVolName;      // current volume name (from $VOLNAME.volumename file)
    PFSDirectory directory;     // directory buffer as written to by Pascal system; upon write, this will be synced with cachedDirectory
    FakeFileDates dateFaker;

    int expectedRelDirSector;   // TODO: remove this
    std::map<__int16, PFSDirEntry> cachedDirectory; // copy of current known directory, indexed by start block

    template<typename S,typename T>
    static bool isValidName(S name)
    {
        // a name is valid if it fits
        if (name.size() < 0 || name.size() > T::capacity())
            return false;
        // and if it has no non-ASCII characters
        // We also forbid [ to avoid confusion with our [XXXX] syntax.
        for (size_t i = 0; i < name.size(); i++)
            if (name[i] < 32 || name[i] > 127 || name[i] == '[')
                return false;
        // FIXME: there are probably other syntactic constraints
        return true;
    }
    int find_last_of(const string & s, const char * alternatives)
    {
        size_t n = string::npos;
        for (size_t i = 0; alternatives[i]; i++)
            if (n == string::npos)
                n = path.find_last_of(alternatives[i]);
        return (n != string::npos) ? (int) n : -1;
    }
    static bool ends_with(const string & s, const string & withwhat)
    {
        if (s.size() < withwhat.size())
            return false;
        return s.substr(s.size() - withwhat.size()) == withwhat;
    }
    // find sector
    struct FoundSector
    {
        string path;            // of the file that this belongs to
        size_t offset;          // byte offset of the sector within the file
        size_t bytesInSector;   // the last sector has less bytes
        bool usesConversion;    // TEXT files have a converted file alongside
        PFSFileKind fileKind;   // for debugging
        FoundSector(const string & p, size_t o, size_t size, bool c, PFSFileKind k)
            : path(p), offset(o), usesConversion(c), fileKind(k)
        {
            if (o >= size)
                bytesInSector = 0;  // second sector in a less-than-half-filled block
            else
                bytesInSector = size - o;
            if (bytesInSector < PFSSECSIZE)
                path += ""; // dummy just so we can set a breakpoint
            if (bytesInSector > PFSSECSIZE)
                bytesInSector = PFSSECSIZE;
        }
    };
    static string KindString(size_t kind)
    {
        static const char * kindNames[] = {   // this must match the order of PFSFileKind
            "UNTYPED", "XDSK", "CODE", "TEXT", "INFO", "DATA", "GRAF", "FOTO", "SECUREDIR"
        };
        if (kind >= _countof(kindNames))
            throw logic_error("KindString: invalid PFSFileKind");
        return kindNames[kind];
    }
    // the path where a file lives on the actual disk
    // If the extension is not right, we add another--e.g. [CODE]--that we parse here, and chop later
    template<class S1, class S2>
    static bool ends_with_uc(const S1 & s, const S2 & what)
    {
        // this is an insane function because C++ strings have no proper conversion between 8/16 and lower/upper
        if (what.size() > s.size())
            return false;
        for (size_t i = 0; i < what.size(); i++)
        {
            int sch = s[s.size() - what.size() + i];
            int whatch = what[i];
            if (sch >= 0) sch = toupper(sch);
            if (whatch >= 0) whatch = toupper(whatch);
            if (sch != whatch)
                return false;
        }
        return true;
    }
    template<class S>
    size_t /*PFSFileKind*/ DetectType(S & name, bool stripBrackets)
    {
        for (int kind = UNTYPEDFILE; kind < SECUREDIR; kind++)
        {
            string kindName = KindString(kind);
            string match1 = "." + kindName;
            string match2 = "[" + kindName + "]";
            S wmatch1 (match1.begin(), match1.end());
            S wmatch2 (match2.begin(), match2.end());
            if (ends_with_uc(name, wmatch2))
            {
                if (stripBrackets)
                    name = name.substr(0,name.size() - match2.size());
                return kind;
            }
            if (ends_with_uc(name, wmatch1))
                return kind;
        }
        return DATAFILE;        // this is for any old Windows file
    }
    string ToString(int i)
    {
        char buf[80];
        sprintf(buf, "%d", i);
        return buf;
    }
    string MakeDiskPath(const PFSDirEntry & e)
    {
        string name = string(e.fileName.begin(), e.fileName.end());
        size_t fileKind = DetectType(name, false);
        if (fileKind != e.fileKind) // if auto-detect fails then need to append the marker
            name += "[" + KindString(e.fileKind) + "]";
        if (e.istemp())
            name += ".tmp" + ToString(e.beginBlock) + "$$";
        return path + "\\" + name;
    }
    // on-the fly conversion
    // For converted files:
    //  - when mounting the disk, we always create a converted version
    //  - when creating a file, we create both (in case we don't write & thus don't detect a write)
    //  - when writing, we write into the converted file; this will make it newer than the original
    //  - when writing, at the final size change, we sync it back to the Windows file
    //  - i.e. they co-exist during the lifetime of the disk mount, except for newly created files
    static string MakeConvertedPath(string fn) { return fn + BLOCKEXT; }
    template<class S>
    static bool IsConvertedPath(const S & fn) { return ends_with_uc(fn, string (BLOCKEXT)); }
    static bool NeedsConversion(int/*PFSFIleKind*/ fileKind) { return fileKind == TEXTFILE; }
    int SyncConvertedFile(string diskPath, int direction)
    {
        string convertedDiskPath = MakeConvertedPath(diskPath);
        // direction: 1 = to UCSD, -1 = to Windows, 0 = both ways
        if (direction >= 0)
            if (dateFaker.needtomake(convertedDiskPath.c_str(), diskPath.c_str()))
            {
                dprintf("SyncConvertedFile: %s -> %s\n", diskPath.c_str(), convertedDiskPath.c_str());
                if (converttoTEXT(diskPath.c_str(), convertedDiskPath.c_str()))
                    dateFaker.copydate(diskPath, convertedDiskPath);
                else
                    return MapErrno(errno, "SyncConvertedFile to TEXT");
            }
        if (direction <= 0)
            if (dateFaker.needtomake(diskPath.c_str(), convertedDiskPath.c_str()))
            {
                dprintf("SyncConvertedFile: %s -> %s\n", convertedDiskPath.c_str(), diskPath.c_str());
                if (convertfromTEXT(convertedDiskPath.c_str(), diskPath.c_str()))
                    dateFaker.copydate(convertedDiskPath, diskPath);
                else
                    return MapErrno(errno, "SyncConvertedFile from TEXT");
            }
        return 0;
    }
    // find which disk file a sector belongs to, and which part of it
    FoundSector FindSector(size_t sector)
    {
        size_t block = sector / SECPERBLK;
        for (size_t i = 0; i < directory.numFiles; i++)
        {
            const PFSDirEntry & e = directory.files[i];
            if (block >= e.beginBlock && block < e.endBlock)
                return FoundSector (MakeDiskPath(e),
                                    (sector - e.beginBlock * SECPERBLK) * PFSSECSIZE,
                                    e.size(),
                                    NeedsConversion(e.fileKind),
                                    (PFSFileKind) e.fileKind);
        }
        return FoundSector("", 0, PFSSECSIZE, false, UNTYPEDFILE);  // none found
    }
    int WriteDirectorySectors(char * data, size_t size, size_t relativeSector, const char * buf, size_t bytes)
    {
        if (relativeSector * PFSSECSIZE + bytes > size)
            return IILLEGALBLOCKNO;  // out of bounds
        memcpy(data + relativeSector * PFSSECSIZE, buf, bytes);
        return 0;   // OK
    }
    int ReadDirectorySectors(const char * data, size_t size, size_t relativeSector, char * buf, size_t bytes)
    {
        if (relativeSector * PFSSECSIZE + bytes > size)
            return IILLEGALBLOCKNO;  // out of bounds
        memcpy(buf, data + relativeSector * PFSSECSIZE, bytes);
        return 0;   // OK
    }
    static void reportWeirdness(const char * what)
    {
#ifdef _DEBUG
        dprintf("%s\n", what);
#endif
    }
    static int MapErrno(errno_t eno, const char * what)
    {
#ifdef _DEBUG
        char buf[1024];
        strerror_s(buf, errno);
        dprintf("%s[MapErrno]: errno %d '%s'\n", what, (int) eno, buf);
#endif
        switch (eno)
        {
        case ENOENT      : return INOFILE;
        case EIO         : return IBADBLOCK;
        case EBADF       : return ILOSTFILE;
        case ENOMEM      : return ISTRGOVFL;
        case EACCES      : return ILOSTUNIT;
        case EEXIST      : return IDUPFILE;
        case ENODEV      : return IBADUNIT;
        case ENOTDIR     : return INOFILE;
        case EISDIR      : return INOFILE;
        case ENFILE      : return INOROOM;
        case EMFILE      : return INOROOM;
        case EFBIG       : return INOROOM;
        case ENOSPC      : return INOROOM;
        case ENAMETOOLONG: return IBADTITLE;
        case ENOTEMPTY   : return IDUPFILE;
        default:
            return IBADMODE;        // unknown errors: 'illegal IO request' (need to map to *something*)
        }
    }
    // TODO: monitoring changes in the directory structure
    //  - writing the length of a file
    //  - deletion of a file
    // The directory seems to be written in sequence, up to number of sectors
    // needed for the given number of files.
    // This way we can detect when the last block was written.
    // If a file is written beyond its registered size in the directory,
    // we will clip the writing to the size.
    // TODO: This could cause issues with file extensions; we need to test whether
    // the directory is updated first; otherwise this logic will fail.
    int WriteSectorsToFile(const FoundSector & foundSector, const char * buf, size_t bytes)
    {
        if (foundSector.bytesInSector == 0) // writing an overflow sector (beyond end of file)
            return 0;
        int ret = 0;
        string filePath = foundSector.path;
        if (foundSector.usesConversion)
        {
            ret = SyncConvertedFile(filePath, 1);   // convert to UCSD in case it exists
            filePath = MakeConvertedPath(filePath); // and then operate on the converted file instead
            if (ret)
                return ret; // not much we can do if we cannot sync
        }
        dprintf("WriteSectorsToFile: writing %d bytes to offset %d in file %s\n", (int) bytes, (int) foundSector.offset, filePath.c_str());
        FILE * f = fopen(filePath.c_str(), "r+b");  // open existing (need read/write, "a" will kill fseek())
        if (f == NULL && errno == ENOENT)
            f = fopen(filePath.c_str(), "wb");      // not there: create it
        if (f == NULL)
            ret = MapErrno(errno, "WriteSectorsToFile fopen");
        if (ret == 0)
            if (fseek(f, foundSector.offset, 0) != 0)
                ret = MapErrno(errno, "WriteSectorsToFile fseek");
        if (ret == 0)
            if (fwrite(&buf[0], foundSector.bytesInSector, 1, f) != 1)
                ret = MapErrno(errno, "WriteSectorsToFile fwrite");
        if (ret == 0)
            if (fflush(f) != 0)
                ret = MapErrno(errno, "WriteSectorsToFile fflush");
        if (f)
            fclose(f);
        if (ret == 0)
            dateFaker.touch(filePath);
        return ret;
    }
    int ReadSectorsFromFile(const FoundSector & foundSector, char * buf, size_t bytes)
    {
        if (foundSector.bytesInSector == 0) // reading an overflow sector
        {
            zerostruct(buf);
            return 0;
        }
        int ret = 0;
        string filePath = foundSector.path;
        if (foundSector.usesConversion)
        {
            // we do not sync here since the file should have been synced already when reading the directory at the start
            filePath = MakeConvertedPath(filePath); // we read from the converted file
            if (ret)
                return ret; // not much we can do if we cannot sync
        }
        dprintf("ReadSectorsFromFile: reading %d bytes from offset %d in file %s\n", (int) bytes, (int) foundSector.offset, filePath.c_str());
        FILE * f = fopen(filePath.c_str(), "rb");
        if (f == NULL)
            ret = MapErrno(errno, "ReadSectorsFromFile fopen");
        if (ret == 0)
            if (fseek(f, foundSector.offset, 0) != 0)
                ret = MapErrno(errno, "ReadSectorsFromFile fseek");
        if (ret == 0)
        {
            if (fread(&buf[0], foundSector.bytesInSector, 1, f) != 1)
                ret = MapErrno(errno, "ReadSectorsFromFile fread");
            for (size_t i = foundSector.bytesInSector; i < PFSSECSIZE; i++)
                buf[i] = 0; // pad if file is shorter
        }
        if (f)
            fclose(f);
        return ret;
    }
    bool fexists(const char * name)
    {
        FILE * f = fopen(name, "rb");
        if (f)
            fclose(f);
        return (f != NULL);
    }
public:
    PFSDiskEmulator(const char * folder, size_t totalSizeInBytes)
        : path(folder), directory(totalSizeInBytes)
    {
        // now populate the directory entries
        size_t curBlock = directory.endBlock;       // we allocate starting from here
        set<string> seenNames;
        wstring wfolder (path.begin(), path.end()); // FIXME: proper Unicode
        _wfinddatai64_t finddata;
        intptr_t h = _wfindfirsti64((wfolder + L"\\*").c_str(), &finddata);
        if (h >= 0)
        {
            bool done = false;
            for( ; !done; done = _wfindnexti64(h, &finddata) < 0)
            {
                // skip non-files
                if (finddata.attrib & (0x10/*_A_SUBDIR*/ | 2/*_A_HIDDEN*/ | 4/*_A_SYSTEM*/))
                    continue;
                wstring fname = finddata.name;
                // $VOLNAME.volumename file denotes the name of this file
                if (fname[0] == '$' && ends_with_uc(fname, string(".volumename")) && directory.volumeName.size() == 0)
                {
                    wstring volname = fname.substr(1, fname.size() - 12);
                    if (isValidName<wstring,PFSVolumeName>(volname))
                        directory.volumeName.assign(volname.c_str());
                    currentVolName = directory.volumeName.tostring();
                    // TODO: should we delete any secondary .volumename file? Or throw; volume is invalid.
                    continue;
                }
                // directory has limited number of entries
                if (directory.numFiles >= _countof(directory.files))
                    break;
                //  check if the file is presentable inside the UCSD world by various rules
                // it's a converted backing file --skip it, we will pick up the main file (if not, it's a stray)
                if (IsConvertedPath(fname))
                {
                    //continue;
                    // for now: keep it and convert it
                    // If the Windows file already exists, then we find it now and skip the actual one as a dup.
                    // If Windows file does not exist then we create it below in a two-way sync.
                    fname = fname.substr(0, fname.size() - strlen(BLOCKEXT));
                }
                // invalid name: skip
                size_t detectedFileKind = DetectType(fname, true);  // updates fname if it strips something
                if (!isValidName<wstring,PFSFileName>(fname))
                    continue;
                // prepare the direntry
                PFSDirEntry & e = directory.files[directory.numFiles];
                e.fileName.assign(fname.c_str());       // this upper-cases it
                e.beginBlock = curBlock;
                e.lastModificationTime = PFSDateRec(finddata.time_write);  // note: this makes it a non-temp file, needed by MakeDiskPath() below
                e.fileKind = detectedFileKind;
                size_t fileSize;
                string diskPath = MakeDiskPath(e);
                if (fexists(diskPath.c_str()))          // (we may have gotten here by discovering a BLOCKEXT file; existing file will override)
                    dateFaker.touch(diskPath);          // get the file into our fake date tracker (but not the BLOCKEXT file)
                else
                    dateFaker.touch(diskPath + BLOCKEXT);   // if we got here through a BLOCKEXT then this will get it converted
                if (NeedsConversion(e.fileKind))        // converted file: we report its UCSD size (post-conversion)
                {
                    // BUGBUG: This uses dateFaker which will not work for newly discovered files here, will it?
                    if (SyncConvertedFile(diskPath, 0/*1*/) != 0)   // for now: two-way sync for conversion
                        continue;           // conversion failed--we do not see this file from the UCSD side
                    fileSize = (size_t) ffilelength(MakeConvertedPath(diskPath).c_str());  // size of converted file (that's what UCSD cares about)
                }
                else        // no conversion: use file size directly
                    fileSize  = (size_t) finddata.size;
                size_t numBlocks = (fileSize + PFSBLKSIZE - 1) / PFSBLKSIZE;
                size_t endBlock = e.beginBlock + numBlocks;
                if (endBlock > directory.numBlocksOnVolume)  // disk capacity exceeded
                    break;
                e.endBlock = endBlock;
                e.bytesInLastBlock = PFSBLKSIZE - (numBlocks * PFSBLKSIZE - fileSize);
                auto modTime = finddata.time_write;
                // duplicate check --multiple files mapped to the same UCSD name
                string actualName(e.fileName.begin(), e.fileName.end());
                auto insertResult = seenNames.insert(actualName);
                if (!insertResult.second)    // aready there (after upper-casing) --skip
                    continue;
                // successfully entered
                curBlock += numBlocks;
                directory.numFiles++;
            }
            //_findclose(h);    // missing??
        }
        // set volume name from folder name if not given --this will go away
        // TODO: this gets overridden by $VOLNAME.volumename file; path will just be the unit number
        if (currentVolName.empty())
        {
            auto n = find_last_of(path, "/\\:");
            if (n < 0)
                throw invalid_argument("PFSDiskEmulator: no folder name in path?");
            currentVolName = path.substr(n+1);
            if (!isValidName<string,PFSVolumeName>(currentVolName))
                throw invalid_argument("PFSDiskEmulator: folder name must be a valid UCSD volume name");
            directory.volumeName.assign(currentVolName.c_str());
            FILE * f = fopen((path + "\\$" + currentVolName + ".volumename").c_str(), "wb");
            if (f)
                fclose(f);
        }
        // initialize directory-writing state
        expectedRelDirSector = 0;
        SyncDir();  // (this will only make a copy of the dir to detect changes)
    }
    ~PFSDiskEmulator()
    {
        // TODO: should we clean the converted files? But we never get here anyway...
    }
    // Directory blocks are written partially (only what changed).
    // We assume they are written in ascending direction (#files comes first).
    // file creation:
    //  - a new entry gets created with
    //     - end block = disk end (... or end of gap?)
    //     - date.year = 100   --temp
    //    => trigger on year 100, try to create the file then
    //  - data is written
    //  - file is finalized by setting the year to < 100
    // also need to detect file renaming and deletion
    // Use a hash table as a second copy for the dir, hashed by beginBlock.
private:
    // is there a proper CRT/Windows equivalent?
    long long ffilelength(const char * path)
    {
        long long size = -1;
        int fd = _open(path, _O_BINARY | _O_RDONLY);
        if (fd >= 0)
        {
            size = _filelengthi64(fd);
            _close(fd);
        }
        return size;
    }
    // index the 'directory' by start block into the passed map
    void MakeBlockIndexedDirectory(std::map<__int16, PFSDirEntry> & newCachedDirectory)
    {
        std::set<string> allNames;
        for (size_t i = 0; i < directory.numFiles; i++)
        {
            const PFSDirEntry & e = directory.files[i];
            newCachedDirectory[e.beginBlock] = e;       // we hash by start block (unique)
            string diskPath = MakeDiskPath(e);
            auto res = allNames.insert(diskPath);
            if (!res.second)
                reportWeirdness("duplicate pathname");
        }
    }
    // sync 'directory' into the block-indexed 'cachedDirectory'
    // Call this whenever a directory-writing operation has completed.
    // possibilites:
    //  - file appeared newly  --create it if it does not exist yet
    //  - file was renamed (incl. type change)  --replicate that on disk
    //  - file transitioned from temp to non-temp status  --rename it (temp files have a different name)
    //  - file disappeared  --remove it on disk
    int SyncDir()
    {
        int err = INOERROR;
        // sync volume name
        string newVolName = directory.volumeName.tostring();
        if (newVolName != currentVolName)
        {
            dprintf("SyncDir: renaming volume %s to %s\n", currentVolName.c_str(), newVolName.c_str());
            if (rename((path + "\\$" + currentVolName + ".volumename").c_str(),
                       (path + "\\$" + newVolName + ".volumename").c_str()) != 0)
                err = MapErrno(errno, "SyncDir rename (.volumename file)");
            currentVolName = newVolName;
        }
        // TODO: detect name change, update the .volumename file
        // sync files
        try
        {
            // transfer the dir into a map
            std::map<__int16, PFSDirEntry> newCachedDirectory; // full copy
            MakeBlockIndexedDirectory(newCachedDirectory);
            // process deleted items
            // We do those first because we may have replaced an old file with a temp
            for(auto iter = cachedDirectory.begin(); iter != cachedDirectory.end(); iter++)
            {
                const PFSDirEntry & e = iter->second;
                // find it in the new directory
                if (newCachedDirectory.find(e.beginBlock) == newCachedDirectory.end())
                {
                    dprintf("SyncDir: detected file being deleted: %s\n", e.fileName.tostring().c_str());
                    // file was deleted --delete it on disk as well
                    if (NeedsConversion(e.fileKind))
                    {
                        // we always operate on converted files, it must be there
                        if (dateFaker.unlink(MakeConvertedPath(MakeDiskPath(e)).c_str()) != 0 && err == INOERROR)
                            err = MapErrno(errno, "SyncDir unlink (converted)");
                        // Windows file may never have been created, so ENOENT is not an error
                        // (but we should check for others...)
                        dateFaker.unlink(MakeDiskPath(e).c_str());
                    }
                    else
                        if (dateFaker.unlink(MakeDiskPath(e).c_str()) != 0 && err == INOERROR)
                            err = MapErrno(errno, "SyncDir unlink");
                }
            }
            // now check for renamed or deleted items
            // We loop over the cached dir, since we don't need to do anything for new files.
            for(auto iter2 = newCachedDirectory.begin(); iter2 != newCachedDirectory.end(); iter2++)
            {
                const PFSDirEntry & e2 = iter2->second;
                const auto diskPath2 = MakeDiskPath(e2);
                // find it in the old directory
                auto iter = cachedDirectory.find(e2.beginBlock);
                if (iter != cachedDirectory.end())  // found
                {
                    const PFSDirEntry & e = iter->second;
                    const auto diskPath = MakeDiskPath(e);
                    if (diskPath2 != diskPath)  // (note: this will pick up type changes as well)
                    {
                        dprintf("SyncDir: detected file being renamed or type-changed: %s to %s\n", e.todebugstring().c_str(), e2.todebugstring().c_str());
                        // name changed --rename it on disk as well
                        // BUGBUG: we need to check both for NeedsConversion; if inconsistent then do something
                        if (NeedsConversion(e.fileKind) && NeedsConversion(e2.fileKind))    // if convertible file then rename both
                        {
                            // We always operate on converted files as the primary file.
                            if (dateFaker.rename(MakeConvertedPath(diskPath).c_str(), MakeConvertedPath(diskPath2).c_str()) != 0 && err == INOERROR)
                                err = MapErrno(errno, "SyncDir rename (converted)");
                            // The Windows file may not exist yet, if this is a new file; so no error check.
                            // (ahem... we rather should check for ENOENT and fail on all others...)
                            dateFaker.rename(diskPath.c_str(), diskPath2.c_str());
                        }
                        else if (!NeedsConversion(e.fileKind) && NeedsConversion(e2.fileKind))   // converting to a TEXT file
                        {
                            if (dateFaker.rename(diskPath.c_str(), MakeConvertedPath(diskPath2).c_str()) != 0 && err == INOERROR)
                                err = MapErrno(errno, "SyncDir rename (non-converted to converted)");
                            if (SyncConvertedFile(diskPath2, -1) != 0 && err == INOERROR)
                                err = MapErrno(errno, "SyncDir SyncConvertedFile");   // sync back to Windows
                        }
                        else if (NeedsConversion(e.fileKind) && !NeedsConversion(e2.fileKind))      // converting TEXT to DATA (keep block format)
                        {
                            if (SyncConvertedFile(diskPath.c_str(), 1) != 0 && err == INOERROR)     // convert to UCSD in case we have not yet
                                err = MapErrno(errno, "SyncDir SyncConvertedFile (convert before renaming)");
                            if (dateFaker.rename(MakeConvertedPath(diskPath).c_str(), diskPath2.c_str()) != 0 && err == INOERROR)
                                err = MapErrno(errno, "SyncDir rename (converted)");
                            dateFaker.unlink(diskPath.c_str()); // (if conversion failed, we may loose the file... meh!)
                            dateFaker.unlink(MakeConvertedPath(diskPath2).c_str()); // has no block format file
                        }
                        else
                            if (dateFaker.rename(diskPath.c_str(), diskPath2.c_str()) != 0 && err == INOERROR)
                                err = MapErrno(errno, "SyncDir unlink");
                    }
                    if (e.size() != e2.size())
                    {
                        dprintf("SyncDir: detected file being resized: %s %d to %d\n", e.todebugstring().c_str(), (int) e.size(), (int) e2.size());
                        // size has changed
                        // This happens while writing a file, which at the start occupies
                        // all the way until the end of the disk (or gap)
                        if (NeedsConversion(e2.fileKind))   // we wrote a converted file
                        {
                            // We always operate on converted files, and just sync them back if they have changed.
                            // File sizes are in the UCSD world.
                            if (dateFaker.truncate(MakeConvertedPath(diskPath2).c_str(), e2.size()) != 0 && err == INOERROR)
                                err = MapErrno(errno, "SyncDir truncate (converted)");
                            if (SyncConvertedFile(diskPath2, -1) != 0 && err == INOERROR)
                                err = MapErrno(errno, "SyncDir SyncConvertedFile");   // sync back to Windows
                        }
                        else
                            if (dateFaker.truncate(diskPath2.c_str(), e2.size()) != 0 && err == INOERROR)
                                err = MapErrno(errno, "SyncDir truncate");
                    }
                }
                // not found: create it if it doesn't exist yet
                else
                {
                    dprintf("SyncDir: detected file of kind %d being created: %s\n", e2.fileKind, e2.todebugstring().c_str());
                    // if it's a converted file, we create both
                    // Note: This is not a full rount-trip--a TEXT file is created empty,
                    // but that's never a valid UCSD file, and there is no way to encode that in the
                    // Windows file. If we convert back, it will be a valid TEXT file with no characters in it.
                    if (NeedsConversion(e2.fileKind))
                        createfile(MakeConvertedPath(diskPath2).c_str());
                    dateFaker.createfile(diskPath2.c_str());
                }
            }
            // done: swap them
            cachedDirectory.swap (newCachedDirectory);
        }
        catch(const std::exception & e)      // out of memory--just skip it
        {
            const char * what = e.what();
            reportWeirdness("SyncDir exception caught:");
            reportWeirdness(what);
            err = ISTRGOVFL;
        }
        return err;
    }
    // compute the relative director sector that holds the last file entry
    size_t RelDirSectorWithLastEntry() { return (directory.size() -1) / PFSSECSIZE; }
public:
    int WriteSectors(size_t sector, const char * buf, size_t bytes)
    {
        // sector = 1/SECPERBLK block; this is the unit of diskio.c to match Apple sector interleaving
        if (sector >= PFSDirectory::dirBeginBlock*SECPERBLK && sector < directory.endBlock*SECPERBLK)
        {
            // TODO: change so that it always writes entire directory in one go; then require that
            size_t relDirBlock = sector - PFSDirectory::dirBeginBlock*SECPERBLK;
            size_t relDirSectorWithLastEntry = RelDirSectorWithLastEntry();
            if (relDirBlock > relDirSectorWithLastEntry)
                reportWeirdness("directory writing beyond end");
            else if (relDirBlock != expectedRelDirSector)
                reportWeirdness("directory writing out of sequence");
            int err = WriteDirectorySectors((char*) &directory, sizeof(directory), relDirBlock, buf, bytes);
            if (err == 0)
            {
                size_t newRelDirSectorWithLastEntry = RelDirSectorWithLastEntry();  // may have been updated
                // if the last entry was written, we reset the state
                if (relDirBlock < newRelDirSectorWithLastEntry)
                    expectedRelDirSector++;
                else
                {
                    expectedRelDirSector = 0;
                    err = SyncDir();  // this syncs all changes to the dir to our back up and possibly to disk
                }
            }
            return err;
        }
        if (expectedRelDirSector > 0)
        {
            reportWeirdness("abandoned directory writing");
            expectedRelDirSector = 0;
            SyncDir();          // just in case; hoping no record got broken
        }
        // writing the last sector of the disk
        // This could be a whole-disk copy.
        if (sector == directory.numBlocksOnVolume * SECPERBLK-1)
        {
            // we extend the disk once it's been completely copied
            if (directory.numBlocksOnVolume < 32767)
                directory.numBlocksOnVolume = 32767;
            // TODO: what to do with endBlock==10? Is that a larger directory, or a backup?
            //if (directory.endBlock == 10)
            //    directory.endBlock == 6;
        }
        // find sector
        // TODO: What is the allocation if two files are open at once? Will they overlap?
        auto foundSector = FindSector(sector);
        if (foundSector.path.empty())
        {
            // invalid location: we assume that this is a block-level disk copy operation
            return 0;       // just ignore it
        }
        // and write data to the file
        return WriteSectorsToFile(foundSector, buf, bytes);
        // FIXME: what's missing is the update of the file length
    }
    int ReadSectors(size_t sector, char * buf, size_t bytes)
    {
        // sector = 1/2 block; this is the unit of diskio.c to match Apple sector interleaving
        if (sector >= PFSDirectory::dirBeginBlock*SECPERBLK && sector < directory.endBlock*SECPERBLK)
            return ReadDirectorySectors((char*) &directory, sizeof(directory), sector - PFSDirectory::dirBeginBlock*SECPERBLK, buf, bytes);
        // find sector
        auto foundSector = FindSector(sector);
        if (foundSector.path.empty())
        {
            // invalid location: we just return zeroes
            // This allows to copy this as a whole disk.
            // Note: diskperipheral currently forbids this (blocks access to block 0 and 1).
            zerostruct(buf);
            return 0;
        }
        // and read data from the actual file
        int res = ReadSectorsFromFile(foundSector, buf, bytes);
#ifdef _DEBUG
        //if (foundSector.fileKind == CODEFILE && foundSector.offset == 0)
        //    sin(1.0);
#endif
        return res;
    }
    string DiskPathForFile(const string & title)
    {
        for (size_t i = 0; i < directory.numFiles; i++)
        {
            const PFSDirEntry & e = directory.files[i];
            if (strncmp(e.fileName.begin(), title.c_str(), e.fileName.size()) == 0)
                return MakeDiskPath(e);
        }
        return "";  // not found
    }
    string FindFileByBlock(size_t block)
    {
        for (size_t i = 0; i < directory.numFiles; i++)
        {
            const PFSDirEntry & e = directory.files[i];
            if (block >= e.beginBlock && block < e.endBlock)
                return string(e.fileName.begin(), e.fileName.end());
        }
        return "";
    }
    void SuspendResume(psnapshot * s)
    {
        s->str("path", path);
        s->str("currentVolName", currentVolName);
        s->scalar("directory", directory);
        dateFaker.suspendresume(s);
        if (s->resuming)
        {
            expectedRelDirSector = 0;   // TODO: remove this
            MakeBlockIndexedDirectory(cachedDirectory);
        }
    }
};

PFSDirectory::PFSDirectory(size_t totalSizeInBytes/*0 is OK if it gets overwritten anyway*/)
{
    zerostruct(*this);
    size_t blocks = totalSizeInBytes / PFSBLKSIZE;
    if (blocks * PFSBLKSIZE != totalSizeInBytes)
        throw std::invalid_argument("PFSDirectory: disk size must be multiple of PFSBLKSIZE");
    if (blocks > 32767)
        throw std::invalid_argument("PFSDirectory: disk size must not exceed 32767 blocks");
    numBlocksOnVolume = blocks;
    endBlock = 6; // Apple format
    fileTypeForDirHeader = PFSFileKind::UNTYPEDFILE;   // header
}  // some initialization

PFSDateRec::PFSDateRec(const __time64_t & date)
{
    struct tm today;
    auto err = _localtime64_s (&today, &date);
    if (err)
        throw std::invalid_argument("PFSDateRec: failed to convert file time");
    month = today.tm_mon + 1;   // 0-based
    day = today.tm_mday;
    year = today.tm_year % 100; // Y2K--yay!
}


EmulatedPFSDisk::EmulatedPFSDisk(const char * folder, size_t size) : inner(new PFSDiskEmulator(folder, size)) { }
static string utf16to8(const wstring & s)
{
    // a very hacky version of this
    string res;
    for (size_t k = 0; k < s.size(); k++)
        res.push_back ((char) s[k]);
    return res;
}
EmulatedPFSDisk::EmulatedPFSDisk(const wchar_t * folder, size_t size) : inner(new PFSDiskEmulator(utf16to8(folder).c_str(), size)) { }
EmulatedPFSDisk::~EmulatedPFSDisk() { delete inner; }
int EmulatedPFSDisk::WriteSectors(size_t sector, const char * buf, size_t bytes) { return inner->WriteSectors(sector, buf, bytes); }
int EmulatedPFSDisk::ReadSectors(size_t sector, char * buf, size_t bytes) { return inner->ReadSectors(sector, buf, bytes); }
string EmulatedPFSDisk::FindFileByBlock(size_t block) const { return inner->FindFileByBlock(block); }
string EmulatedPFSDisk::DiskPathForFile(const string & title) const { return inner->DiskPathForFile(title); }
void EmulatedPFSDisk::SuspendResume(psnapshot * s) { inner->SuspendResume(s); }
