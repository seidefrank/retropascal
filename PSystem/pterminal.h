// the terminal device in the UCSD system
// (C) 2013 Frank Seide

#include "pperipherals.h"

// interface to term_winrt.cpp  --these are used from pmachine.cpp directly
// TODO: move to iterminal
int term_width(void);
int term_height(void);
void term_confirm_size(size_t screenwidth, size_t screenheight, size_t WIDTH, size_t HEIGHT);
void term_desired_size(size_t & cols, size_t & rows, size_t & usableCols, size_t & usableRows);

class iterminal;

namespace psystem
{

class terminalperipheral : public pperipheral
{
    ::iterminal & term;
    virtual ~terminalperipheral() { }

    virtual IORSLTWD read(void * p, int offset, size_t size, int block, size_t mode);
    virtual IORSLTWD write(const void * p, int offset, size_t size, int block, size_t mode);
    virtual IORSLTWD stat(WORD & res);
    virtual IORSLTWD clear();
    virtual void suspendresume(class psnapshot *);

    // from iterminal:
public:
    terminalperipheral(::iterminal & term) : term(term) { }
    ::iterminal & getterminal() { return term; }
};

};
