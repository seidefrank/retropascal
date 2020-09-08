// a disk in the UCSD system
// (C) 2013 Frank Seide

#pragma once

#include "ptypes.h"
#include "pperipherals.h"
#include "pglobals.h"       // for error codes
#include <memory>
#include <string>
#include <vector>

namespace psystem
{

class diskperipheral : public pperipheral
{
    std::unique_ptr<class EmulatedPFSDisk> emulateddisk;
    virtual ~diskperipheral() { }

    IORSLTWD readwrite(BYTE * p, int offset, size_t size, size_t block, bool write);
    virtual IORSLTWD read(void * p, int offset, size_t size, int block, size_t mode);
    virtual IORSLTWD write(const void * p, int offset, size_t size, int block, size_t mode);
    virtual IORSLTWD stat(WORD & res) { return emulateddisk ? INOERROR : INOUNIT; } // TODO: do I need to fill in 'res'? how?
    virtual IORSLTWD clear() { return INOERROR; }
    virtual void suspendresume(class psnapshot *);
public:
    diskperipheral() { }

    // disk-specific functions:
    bool mount(const wchar_t * diskpath);
    bool mounted() const { return emulateddisk.get() != nullptr; }
    std::string volname();
    IORSLTWD findfile(const char * name, size_t & beginblock, size_t & numblocks);
    std::string findfilebyblock(size_t block) const;
    std::string getdiskpath(const std::string & title);
    std::vector<std::string> completefilename(const char * prefix);
};

};
