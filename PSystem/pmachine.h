// the p-code interpreter
// (C) 2013 Frank Seide

// documentation:
//  - p machine:  http://miller.emu.id.au/pmiller/ucsd-psystem-um/reconstruct/03-04-pseudo-machine.html

#pragma once

#include "ptypes.h"
//#include "pglobals.h"   // SYSCOMREC is hard-coded into our memory
#include "iterminal.h"
#include <stdexcept>
#include <assert.h>
#include <memory>
#include <vector>
#include <stack>
#include <deque>

namespace psystem
{

struct pmemory
{
protected:  // not private since we need access for snapshot taking/resuming
    void validateaddr(size_t addr) const { assert(addr < _countof(words)); }
    BYTE evalstack[12*1024];    // 4k bytes of evaluation stack plus 2 x 4k guard pages, so we don't need to check overflow so often; UCSD Filer needs > 128 bytes at least
    WORD words[0x10000];        // 64k words of RAM
public:
    // direct access to RAM
    inline const WORD operator[] (size_t addr) const { validateaddr(addr); return words[addr]; }
    inline       WORD & operator[] (size_t addr)     { validateaddr(addr); return words[addr]; }
protected:
    void reset() { memset(&words[0], 0, sizeof(words)); }   // wipe the memory
    // convert from pointer to PTR, i.e. the index that the pointer refers to in this array
    template<typename T> PTR<T> addr(const T * p) const
    {
        if(1 & (intptr_t)p)
            throw std::out_of_range("pmemory: unaligned pointer"); 
        const size_t addr = ((WORD*)p) - &words[0]; validateaddr(addr); return *(PTR<T>*)&addr;
    }
public: // public because this is needed by UNIT implementation
    template<typename T> T * ptr(const PTR<T> & addr) const
    {
        validateaddr(addr.offset); return (T*) &words[addr.offset];
    }
protected:
    // known address locations
    void * NILPTR;
    void * rambottom;
    void * cpustackbottom;
    void * cpustacktop;
    void * heapbottom;
    void * framestacktop;
    void * ramtop;
    // note: strings cannot live at < 256 due to SAS instruction which treats those as single-char strings
    // TODO: move SYSCOM to addr 1 (under the CPU stack, or rather combine CPU stack with the frame stack)
    pmemory() :
        // eval stack is in a separate area (6502 stack too small for UCSD II.0 FILER)
        rambottom(&evalstack[_countof(evalstack)/3]),
        cpustackbottom(rambottom),
        cpustacktop(&evalstack[_countof(evalstack)/3*2]),
        // main data RAM
        NILPTR(&words[0]),              // addr == 0
        heapbottom(&words[128]),        // first 256 bytes blocked by 1-char string addresses; we could move SYSCOM here
        framestacktop(&words[0xffff] + 1),
        ramtop(framestacktop)
        // valid RAM includes evalstack[] and words[]
    {
        if (evalstack + _countof(evalstack) != (void*)words)
            throw std::logic_error("C++ compiler unexpectedly rearranged RAM blocks");
        // we assume the above for range checks
    }
    void suspendresume(class psnapshot *);       // snapshot p-machine itself for undo or suspend
};

class pcpu
{
protected:
    // registers
    // We encode pointers as actual WORD pointers instead of offsets, for efficiency.
    CODE * IP;              // instruction pointer
    void * PSP;             // CPU stack pointer

    // for debugging:
    struct codelocation
    {
        WORD relIP;
        BYTE segid;
        BYTE procid;
        // and in a format that can be seen in the debugger:
        const CODE * IP;
        const struct pproctable * SEG;
        const struct pprocdescriptor * PROC;
        BYTE lexlevel;
        codelocation() { memset(this, 0, sizeof(*this)); }
    };

    __declspec(noinline) void throwruntimeerror(XEQERRWD xeqerr) const;   // throw out of step()
    __declspec(noinline) void throwfatalerror(XEQERRWD xeqerr) const;

    void suspendresume(class psnapshot *);       // snapshot p-machine itself for undo or suspend
};

class pmachine;

class psegtable
{
protected:
    // one segment loaded in memory
    struct segtableentry
    {
        bool isSYSTEM;                          // true if this comes from SYSTEM.PASCAL at boot time
        size_t refcount;                        // how many references
        void * segend;                          // previous frame stack ptr, pop to this to unload
        const struct pproctable * proctable;    // pointer to segment descriptor; Apple data segment: nullptr
        WORD * variables;                       // Apple data segment: pointer to data array, indexed 1-based
        CODE * segbase;                         // load address of segment
        bool (pmachine::*unitemulator)(size_t procno);  // emulation function for units in assembly language (Apple compat)
        // debugging:
        std::string segname;                    // segment name if we know it (we may not have them for anything but the boot segment)
        std::string codefile;                   // name of the code file a segment was loaded from ("" if somehow undeterminable)
        size_t codeunit;                        // and unit it was loaded from

        segtableentry() { memset(this, 0, sizeof(*this)); }
    };

    // segment procedures loaded in memory
    // This matches with SYSCOMREC.SEGTABLE, which contains the static disk-location information
    static const size_t MAX_RESIDENT_SEG = 31;      // MAX CODE SEGMENT NUMBER we can load into SYSCOM
    // TODO: this seems to be 16, not 32; cf. pglobals.h
    segtableentry segtable[MAX_RESIDENT_SEG+1];

    void suspendresume(class psnapshot *);       // snapshot p-machine itself for undo or suspend
};

class pperipherals
{
protected:
    static const size_t MAXUNIT = 12;      // MAXIMUM PHYSICAL UNIT # FOR UREAD
    std::shared_ptr<class pperipheral> devices[MAXUNIT+1];

    pperipheral * getunit(size_t u) { return devices[u].get(); }
    class diskperipheral * getdiskunit(size_t u);

    void suspendresume(class psnapshot *);       // snapshot p-machine itself for undo or suspend
};

class pmachine : public pcpu, public pmemory, public psegtable, public pperipherals
{
    // additional globals that we include here for convenience
    void * NP;                              // NEW pointer, heap grows from bottom
    void * SP;                              // frame and code stack, grows from top
    // globals from GLOBALS.TEXT
    // TODO: define a proper data structure for this
    struct SYSCOMREC * syscom;              // our syscom record (it lives somewhere inside the emulated memory; this is for direct access)
    pfixedstring<8> * DKVID, * SYVID;
    struct DATEREC * thedate;               // address of THEDATE variable; word 67

    // frame stack and code segments
    const struct pproctable * SEG;          // current segment we are executing in
    const struct pprocdescriptor * PROC;    // current procedure descriptor ('JTAB')
    struct pstackframe * BP;                // current frame pointer (Z80: MP; we call it base pointer like in Intel)
    struct pstackframe * GBP;               // quick access to (innermost) outer frame pointer of lex level 1; program VARs live here
    struct pstackframe * GBP0;              // quick access to outer frame pointer of lex level -1 (PASCALSYSTEM); system VARs in GLOBALS.TEXT live here

    // debugging
    WORD curRelIP;                          // set for each statement, so we can inspect it
    const struct pproctable * curSEG;
    const struct pprocdescriptor * curPROC;
    //std::stack<void*> statementPSPs;        // top is current statement-level PSP, we push/pop in enterproc/exitproc
    struct pstackframe * curBP;
    struct pstackframe * curBPUp1;          // going up 1 and 2 steps
    struct pstackframe * curBPUp2;
    const WORD * curLocals;
    const WORD * curLocalsUp1;
    const WORD * curLocalsUp2;
    const WORD * curSystemGlobals;
    std::vector<codelocation> breakpoints;
    void debugstatement();
    void assertatstatement() const;
    void validatepcode(bool) const;               // check a condition about the correctness of p-code itself and the program (it's a runtime error w.r.t. the interpreter)
    void validatepcodelogic(bool) const;          // code itself is proper, but there is a logic error, e.g. calling a segment that was not loaded
    template<typename T> void validateptr(const T * p) const;
    template<typename T> void validateptr(const PTR<T> & addr) const;
    void validateptr(const BYTEPTR & byteptr) const { validateptr(byteptr.addr);/*we could also check .byte*/ }
    void validatetosptr() const { validateptr(tos<PTR<void>>()); }
    FILE * tracefile;
    void settracefile(const wchar_t *);
    void tracesegchanged(size_t s);
    void traceinst();
    void tracesource();
    void traceIORSLT();
    void traceenterexitproc(size_t s, size_t p, bool entering);

    // stack
    template<typename T> void push(const T & val)
    {
        auto cSP = (BYTE*)PSP;
        const size_t size = sizeof(val);    // (debug here)
#ifdef _DEBUG
        if (cSP < size + (BYTE*)cpustackbottom)
            throwstackoverflow();
#endif
        cSP -= size;
        PSP = cSP;
        *(T*) cSP = val;
    }
    template<typename T> void push(const T * p, size_t n)
    {
        auto cSP = (BYTE*)PSP;
        const size_t size = sizeof(T) * n;  // (debug here)
        if (cSP < size + (BYTE*)cpustackbottom)
            throwstackoverflow();
        cSP -= size;
        PSP = cSP;
        memcpy(cSP, p, size);
    }
    template<typename T> void pushzeroes(size_t n)
    {
        auto cSP = (BYTE*)PSP;
        const size_t size = sizeof(T) * n;  // (debug here)
        if (cSP < size + (BYTE*)cpustackbottom)
            throwstackoverflow();
        cSP -= size;
        PSP = cSP;
        memset(cSP, 0, size);
    }
    void pushzeroset(size_t maxindex)
    {
        checkboundscondition(maxindex < 255 * 16);
        size_t words = maxindex / 16 + 1;       // number of words in the set
        pushzeroes<WORD>(words);
        pushw(words);                           // first data element contains the size
    }
    template<typename T> T pop()
    {
        auto cSP = (BYTE*)PSP;
#ifdef _DEBUG
        if (cSP + sizeof(T) > (BYTE*)cpustacktop)
            throwstackunderflow();
#endif
        const size_t size = sizeof(T);      // (debug here)
        T val = *(T*) cSP;                  // (debug here)
        PSP = cSP + size;
        return val;
    }
    template<typename T> T &       tos(size_t i = 0)       { return ((T*)PSP)[i]; }
    template<typename T> const T & tos(size_t i = 0) const { return ((T*)PSP)[i]; }
    WORD & tosw(size_t i = 0) { return tos<WORD>(i); }
    INT & tosi(size_t i = 0) { return tos<INT>(i); }
    REAL & tosr(size_t i = 0) { return tos<REAL>(i); }
    void popbytes(void * towhere, size_t size)   // TODO: use template, and multiply size?
    {
        auto cSP = (BYTE*)PSP;
        if (cSP + size > (BYTE*)cpustacktop)
            throwstackunderflow();
        memcpy(towhere, PSP, size);
        PSP = cSP + size;
    }
    void dropwords(size_t nwords)               // pop and ignore N words
    {
        auto size = nwords * sizeof(WORD);
        auto cSP = (BYTE*)PSP;
        if (cSP + size > (BYTE*)cpustacktop)
            throwstackunderflow();
        PSP = cSP + size;
    }
    void pushw(const WORD & val) { push(val); }
    WORD popw() { return pop<WORD>(); }
    void pushi(const INT & val) { push(val); }
    INT popi() { return pop<INT>(); }
    REAL popr() { return pop<REAL>(); }
    void popretspace() { popi(); popi(); }      // pop the two dummy words pushed for a function call
    void push(bool b) { push((WORD) (b ? 1 : 0)); }
    template<> bool pop<bool>() { WORD b = popw(); return (b & 1) != 0; }   // bool is popped by testing the bit
    template<class T> T* popptr() { return ptr(pop<PTR<T>>()); }            // pointers are popped as actual pointers
    template<> pstring * pop<pstring *>() { validatetosptr(); return popptr<pstring>(); };
    template<> BYTE * pop<BYTE *>() { validatetosptr(); return popptr<BYTE>(); }
    template<> WORD * pop<WORD *>() { validatetosptr(); return popptr<WORD>(); }
    const pstring * popconststringptr(pfixedstring<2> & singletonbuf);      // including 1-character strings (special pointer)
    void pushd(const pwordstruct & s, bool pushnumwords)
    {
        if (pushnumwords)
            push((const WORD *) &s, s.numwords + 1);
        else
            push((const WORD *) s.data(), s.numwords);
    }
    template<class T>
    void popd(T & s) { size_t n = 1 + tosw(); validatepcodelogic(n * 2 <= sizeof(s)); popbytes(static_cast<pwordstruct*>(&s)/*enforce the type*/, n * 2); }
    // TODO: check this static cast

    // interpreter
    enum compareop { EQ=13, GE=10, GT=11, LE=9, LT=8, NE=12 };  // numbered after dec ops DECCMP; used internally as well
    void compareinstrpush(int compareresult, int op);
    template<int op> void compareinstrpush(int result);
    template<typename T, int op> void compareinstr(size_t sizearg = 0);
    template<int op> void compareinstrstr();
    template<int op> void compareinstrset();
    template<int op> void complexcompareinstr();
    void condbranchinstr(bool);
    WORD bigparam();
    void alignwordparam();
    INT intparam();
    WORD & segmentvarparam();
    BYTE * byteptr(const BYTEPTR & byteptr) const
    {
        validateptr(byteptr);
        // note: byte offsets are signed; we saw negative offsets in SYSTEM.LINKER
        return (ptr(byteptr.addr) + byteptr.byte);
    }

    // special procedures
    void csp();
    void decops();
    struct idsearchargs { INT offset; INT/*enum SYMBOL*/ sy; INT/*enum OPERATOR*/ op; ALPHA alpha; };
    void idsearch(idsearchargs & args, const CHAR * bytestream);
    struct treesearchnode { ALPHA name; PTR<treesearchnode> right; PTR<treesearchnode> left; };
    int treesearch (PTR<treesearchnode> root, PTR<treesearchnode> & node, ALPHA & nameid);
    void flushGDIRP();

    // I/O helpers
    void centerstring(size_t row, const char * s);
    void bootscreen(bool firstbootmessage);
    int VOLSEARCH(const char * FVID);

    // memory allocations
    template<typename T> T * alloca(size_t size);
    void freea(size_t size);

    // frame stacks
    const pproctable & getcodesegment(size_t segid);
    void enterproc(const struct pproctable & proctable, size_t procid, pstackframe * outerframe);
    void exitproc(size_t retwords, bool isRBP);
    struct pstackframe * getouterstackframeforcallinglexlevel(int lexlevel);
    struct pstackframe * pmachine::getstackframenlexlevelsup(size_t n);
    void validatestaticlinks();

    // code swapping
    bool pushsegment(size_t s, size_t u, size_t beginblock, size_t size);
    void pushsegment(size_t s);
    void popsegment(size_t s);
    void refsegment(size_t s);
    void derefsegment(size_t s);
    struct SEGTBL * lastsegtable;    // (debugging)

    // halt
    void enterEXECERROR(const xeqerror & e);    // set up call into EXECERROR from the xeqerr exception object

    // execution
    volatile int yieldrequest;  // yield request
    void step();                // execute one instruction

    // SYS emulation
    int systemproccallstate[systemprocedures::syspnnum];
    bool emulatesystem(size_t procno, bool entering);

    // state and variables for emulating specific system calls
    pstring * FREADSTRING_S;                                // S argument of current FREADSTRING
    bool completepathnames;                                 // pathname completion enabled? (toggle with Ctrl-F)
    std::vector<std::string> completionsuffixconstraints;   // only show what ends in this
    std::string currentpathnameprefix;                      // matchingpathnames is for this
    bool determinematchingpathnames(const std::string & prefix, std::vector<std::string> & matchingpathnames, bool aftercomma, bool wildcardbeforecomma);   // for pathname completion
    void resetpathnameprefix();
    int FCLOSE_unittofinalize;                              // closing this char unit

    // Apple unit emulation
    bool emulateturtlegraphics(size_t procno);
    bool emulateapplestuff(size_t procno);
    bool emulatelongintintrinsics(size_t procno);

    // units with system access
    class systemperipheral * createsystemperipheral(pmachine &);

    // other emulation
    char lastcharswrittentoconsole[3];      // for capturing prompt line
    std::string promptlineascaptured;       // captured prompt line (EDITOR does not use PL); first string written after GOTOXY(0,0)
    bool promptlineiscommandbar() const;

    // snapshots for Undo and suspend/resume
    void suspendresume(class psnapshot *);      // snapshot p-machine

    // snapshot queue for Undo
    std::deque<std::shared_ptr<class psnapshot>> pastsnapshotstack;     // Undo stack
    std::deque<std::shared_ptr<class psnapshot>> futuresnapshotstack;   // Redo stack
    void pushsnapshot();        // add a snapshot/undo state
    bool poppastsnapshot();     // recover VM from a snapshot/undo state
    bool popfuturesnapshot();   // recover VM from a snapshot/undo state
    bool haspastsnapshot();     // something on past snapshot stack? (e.g. can we undo?)
    bool hasfuturesnapshot();   // something on future snapshot stack? (e.g. can we redo?)
    void clearsnapshots();      // we crossed something undo-able (e.g. disk write): destroy all snapshots

    // other
    void updatedatetime();
    void resetterminal();
    void updateterminal(bool max84Cols);
    size_t getspecialterminalmodes() const;
    void forcekillmultimedia();
    mutable size_t lastxmodeprocno, lastxmodesegno, lastxmode;
    bool incodefile(const char * segname) const;
    bool insysfunction(size_t procno) const;
    bool infunction(size_t segno, size_t procno) const;
    CODE * lastjumpedtoIP;          // for detecting dead loops/wait loops
    size_t lastjumpedtoiterations;  // how many times did we jump there

public: // for UNIT implementations/devices
    void yield(unsigned int yieldms) { yieldrequest = yieldms; }        // interrupt (may be called on a different thread)
    IORSLTWD loadsystemfile(const char * name, std::vector<char> &);    // helper to get SYSTEM.CHARSET
    void beingpolled();
    PTR<void> NEWARRAY(int count, int bytesize);
    size_t RAMAVAIL();
public: // for callers
    pmachine(iterminal &);
    ~pmachine();
    bool mount(const wchar_t * diskPath);   // mount disk
    bool reset(bool firstbootmessage);      // reset, get boot code from first mounted disk (set it up for run() and return)
    int run();                              // run until interrupt (or HLT), returns -1 (interrupt), 0 (XIT), or halt code (on error)
    std::vector<unsigned char> savemachinestate();  // for suspend
    void restoremachinestate(const std::vector<unsigned char> & snapshot);  // for resume
};

};
