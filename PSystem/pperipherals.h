// the peripherals of a UCSD system
// (C) 2013 Frank Seide

#pragma once

#include "ptypes.h"
#include "pglobals.h"       // for error codes
#include <memory>           // for shared_ptr

namespace psystem
{

class pperipheral
{
public:
    pperipheral() { }
    virtual ~pperipheral() { }

    enum specialmodes
    {
        inFBLOCKIO = 0x80000000
    };

    // TODO: eliminate 'offset' from this interface; just do it in the interpreter
    virtual IORSLTWD read(void * p, int offset, size_t size, int block, size_t mode) { return IBADUNIT; }
    virtual IORSLTWD write(const void * p, int offset, size_t size, int block, size_t mode) { return IBADUNIT; }
    virtual IORSLTWD stat(WORD &) { return INOUNIT; }
    virtual IORSLTWD clear() { return INOUNIT; }        // flush everything (incl. pending input)
    virtual IORSLTWD finalize() { return INOERROR; }    // optional for char devices: final write flush (commit)
    virtual void suspendresume(class psnapshot *) { }   // snapshot this device for undo/redo or suspend/resume
};

// factories for the various devices; these are implemented in different source files
static pperipheral * createnullperipheral() { return new pperipheral(); }
pperipheral * createterminalperipheral(class iterminal &);
pperipheral * createechoingterminalperipheral(std::shared_ptr<pperipheral>);
pperipheral * creategraphicsperipheral(class pmachine &);
pperipheral * creatediskperipheral();
pperipheral * createclipboardperipheral();
pperipheral * createaudioperipheral();

};
