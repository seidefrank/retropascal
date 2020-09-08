// a disk in the UCSD system
// (C) 2013 Frank Seide

#include "diskio_winrt.h"
#include "pdisk.h"

using namespace std;

namespace psystem
{

IORSLTWD diskperipheral::readwrite(BYTE * p, int offset, size_t size, size_t block, bool write)
{
    if (!emulateddisk) return INOUNIT;
    if (block < 2) return IBADBLOCK;   // no block-level disk meddling!
    size_t sector = block * (PFSBLKSIZE / PFSSECSIZE);
    IORSLTWD res = INOERROR;
    while (size > 0)
    {
        char buf[PFSSECSIZE] = { 0 };  // init to 0 for write (yes, I know, we rather only reset the bytes we need, not important)
        size_t bytestocopy = min(sizeof(buf), size);
        if (write)
            memcpy(buf, p + offset, bytestocopy);
        if (write)
            res = (IORSLTWD) emulateddisk->WriteSectors(sector, buf, _countof(buf));
        else
            res = (IORSLTWD) emulateddisk->ReadSectors(sector, buf, _countof(buf));
        if (res != INOERROR)
            break;
        if (!write)
            memcpy(p + offset, buf, bytestocopy);
        offset += bytestocopy;
        size -= bytestocopy;
        sector++;
    }
    return res;
}

IORSLTWD diskperipheral::read(void * p, int offset, size_t size, int block, size_t mode)
{
    return readwrite((BYTE*)p, offset, size, (size_t) block, false);
}
IORSLTWD diskperipheral::write(const void * p, int offset, size_t size, int block, size_t mode)
{
    return readwrite(const_cast<BYTE*>((BYTE*)p), offset, size, (size_t) block, true);
}

void diskperipheral::suspendresume(class psnapshot * s)
{
    if (emulateddisk)
        emulateddisk->SuspendResume(s);
}

// disk-specific functions:
bool diskperipheral::mount(const wchar_t * diskpath)
{
    if (mounted()) return false;
    try
    {
        const size_t size = 32767 * 512;  // max # blocks possible in UCSD system
        emulateddisk.reset (new EmulatedPFSDisk(diskpath, size));
    }
    catch (...)
    {
        false;
    }
    return true;
}

// get name of volume in the drive
string diskperipheral::volname()
{
    char buf[256];
    if (!emulateddisk)
        return "";
    IORSLTWD ioResult = (IORSLTWD) emulateddisk->ReadSectors(4, buf, _countof(buf)); // volume name is within first 256 bytes
    // It seems overkill to read the disk each time,
    // but actually the emulateddisk just copies 256 bytes from its pre-cached directory,
    // so this is not outrageously cheap (although not maximally efficient either)
    if (ioResult)       // read error
        return "";
    const PFSDirectory & dir = (const PFSDirectory &) buf;
    return string(dir.volumeName.begin(), dir.volumeName.end());
}

// find all file names that begin with a given prefix (for file-name completion)
// TODO: too many dir reads happening with dup code; factor it out
vector<string> diskperipheral::completefilename(const char * name)
{
    vector<string> result;
    if (!emulateddisk)
        return result;
    PFSDirectory dir(0);
    char buf[256];
    for (size_t sector = 0; sector < sizeof(PFSDirectory) / sizeof(buf); sector++)
    {
        IORSLTWD ioResult = (IORSLTWD) emulateddisk->ReadSectors(sector + 4, buf, _countof(buf));
        if (ioResult)
            return result;
        memcpy(sector * sizeof(buf) + (char*)&dir, buf, sizeof(buf));
    }
    PFSFileName prefix;
    if (strlen(name) > prefix.capacity())
        return result;
    prefix.assign(name);
    for (size_t k = 0; k < dir.numFiles; k++)
        if (dir.files[k].fileName.begins_with(prefix))
            result.push_back(dir.files[k].fileName.tostring());
    return result;
}

// find a file on the drive (similar to DIRSEARCH)
IORSLTWD diskperipheral::findfile(const char * name, size_t & beginblock, size_t & size)
{
    if (!emulateddisk)
        return IBADUNIT;
    PFSDirectory dir(0);
    char buf[256];
    for (size_t sector = 0; sector < sizeof(PFSDirectory) / sizeof(buf); sector++)
    {
        IORSLTWD ioResult = (IORSLTWD) emulateddisk->ReadSectors(sector + 4, buf, _countof(buf));
        if (ioResult)
            return ioResult;
        memcpy(sector * sizeof(buf) + (char*)&dir, buf, sizeof(buf));
    }
    PFSFileName title;
    if (strlen(name) > title.capacity())
        return IBADTITLE;
    title.assign(name);
    for (size_t k = 0; k < dir.numFiles; k++)
    {
        if (dir.files[k].fileName.strcmp(title) == 0)
        {
            beginblock = dir.files[k].beginBlock;
            size = dir.files[k].size();
            return INOERROR;
        }
    }
    return INOFILE; // TODO: is this the right error code? Or IBADTITLE?
}

// find a file by block number
// We use this for special knowledge in the emulator if we know which program is running.
// 'dirp' points to the directory in RAM.
string diskperipheral::findfilebyblock(size_t block) const { return emulateddisk->FindFileByBlock(block); }

std::string diskperipheral::getdiskpath(const std::string & title) { return emulateddisk->DiskPathForFile(title); }

pperipheral * creatediskperipheral() { return new diskperipheral(); }

};
