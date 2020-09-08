// the terminal device in the UCSD system
// (C) 2013 Frank Seide

#include "iterminal.h"
#include "pterminal.h"
#include "psnapshot.h"
#include <memory>

// interface to term_winrt.cpp
// TODO: fix compat with C++/ZX and then include a proper header
void TermOpen(int, int);
void TermClose(void);
void TermWrite(char ch, unsigned short mode);
int TermStat(void);

using namespace std;

namespace psystem
{

// UNITCLEAR(1) is supposed to flush the input buffer
// TODO: except this breaks the ability to paste a whole string of commands for auto-processing.
IORSLTWD terminalperipheral::clear()
{
    term.flush();
    return INOERROR;
}

IORSLTWD terminalperipheral::read(void * p, int offset, size_t size, int block, size_t mode)
{
    // special mode to implement KEYPRESS through UNITREAD
    // (since UNITSTATUS is not part of the standard; we may add it, Apple Pascal 1.2 has it)
#define ASIMPLKEYPRESS 2205
    if (mode == ASIMPLKEYPRESS)
    {
        if (size != 2)
            return IBADMODE;
        *(INT *) (offset + (const char*)p) = (WORD) TermStat();
        return INOERROR;
    }
    for (int k = 0; k < (int) size; k++)
    {
#if 0
        bool emulateHomeEnd = (mode & iterminal::specialmodes::emulateHomeEnd) != 0;
        bool emulatePageUpDown = (mode & iterminal::specialmodes::emulatePageUpDown) != 0;
        bool syntaxHighlighting = (mode & iterminal::specialmodes::syntaxHighlighting) != 0;
        bool enableSuggestions = (mode & iterminal::specialmodes::enableSuggestions) != 0;
        bool enablePathnameCompletion = (mode & iterminal::specialmodes::enablePathnameCompletion) != 0;
#endif
        int ch = term.readchar((iterminal::specialmodes) mode);
        if (ch < 0)
            return ICALLAGAIN;      // special error code to indicate we are blocking
        ((char*)p)[offset + k] = (char)ch;
    }
    return INOERROR;
}

#define AUTOMATIONIMPLPUSH 201
IORSLTWD terminalperipheral::write(const void * vp, int offset, size_t size, int block, size_t mode)
{
    const char * p = (const char *) vp;
    // special mode to support 'K(eypress simulation' feature for automation --push keys into input FIFO
    if (mode == AUTOMATIONIMPLPUSH)
    {
        while (size > 0 && p[size-1] == 0)  // skip trailing NULs (may be part of a block copy)
            size--;
        term.pushkeys(p, size);
        return INOERROR;
    }
    for (int k = 0; k < (int) size; k++)
        TermWrite(p[offset + k], mode);
    return INOERROR;
}

IORSLTWD terminalperipheral::stat(WORD & res) { res = (WORD) TermStat(); return INOERROR; }

void terminalperipheral::suspendresume(class psnapshot * s)
{
    term.suspendresume(s);
}

pperipheral * createterminalperipheral(iterminal & term) { return new terminalperipheral(term); }

// non-echoing mirror of above

class echoingterminalperipheral : public pperipheral
{
    shared_ptr<pperipheral> base;            // underlying terminal

    virtual ~echoingterminalperipheral() { }

    virtual IORSLTWD read(void * p, int offset, size_t size, int block, size_t mode)
    {
        for (int k = 0; k < (int) size; k++)
        {
            auto ioResult = base->read(p, offset + k, 1, block, mode);  // this may throw
            if (ioResult != INOERROR)
                return ioResult;    // we will get ICALLAGAIN here when there is no input available
            base->write(p, offset + k, 1, block, mode); // echo it
        }
        return INOERROR;
    }
    virtual IORSLTWD write(const void * p, int offset, size_t size, int block, size_t mode)
    {
        return base->write(p, offset, size, block, mode);
    }
    virtual IORSLTWD stat(WORD & res) { return base->stat(res); }
    virtual IORSLTWD clear() { return base->clear(); }
public:
    echoingterminalperipheral(shared_ptr<pperipheral> basep) : base(basep) { }
};


pperipheral * createechoingterminalperipheral(shared_ptr<pperipheral> base)
{
    return new echoingterminalperipheral(base);
}

};
