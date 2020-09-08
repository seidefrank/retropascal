// all that lives on the frame stack
// (C) 2013 Frank Seide

#include "ptypes.h"

namespace psystem
{

/*
SEGBOT	.EQU	TPROC		; pointer to bottom of segment
RLBASE	.EQU	TPROC+2		; base relocation amount
REFP	.EQU	TPROC+4		; pointer to relevant refcount
PROCBOT .EQU	TPROC+6		; pc relative (proc) relocation amount
RLDELTA .EQU	TPROC+8		; the relocation abount for the relocation
				;   currently being done.
SEGNUM	.EQU	TPROC+10.	; segment # currently being called
SEGTP	.EQU	TPROC+12.	; ^segtable entry for segment
NEWSEG	.EQU	TPROC+14.	; new SEGP
NEWJTB	.EQU	TPROC+16.	; new JTAB pointer

; Seg table (part of syscom) format:
;		 00H		; unit number code for seg is on
;		+02H		; block # code for seg starts at
;		+04H		; segment length (in bytes)
 */

// a code segment descriptor record
/*
    ;Proc table (pointed to by msseg) format
    ;	.EQU	+01H		; number of procs in segment
    ;	.EQU	 00H		; seg_num
    ;-02H to -2*(number of procs)	self-relative pointers to each procs JTAB
 */
struct pproctable
{
    const struct pprocdescriptor & operator[](size_t n) const   // this array is at negative offsets from 'this'
    {
        const RELPTR<pprocdescriptor> * procdescoffsets = (const RELPTR<pprocdescriptor> *) this; // an array of self-rel-references to proc descriptors
        if (n < 1 || n > numprocedures)     // n is 1-based
            throw badprocedureid();
        return *procdescoffsets[-(int)n];  // the array is at negative offsets from 'this'
    }
    BYTE segmentid;
    BYTE numprocedures;
};

// a procedure descriptor record ("JTAB")
/*
    ; Jump table (JTAB) format
    ;	.EQU	+01H		; lex level of proc
    ;	.EQU	 00H		; proc-num
    ENTRIC	.EQU	-02H		; address of entry point (self-relative)
    EXITIC	.EQU	-04H		; address of exit code (self-relative)
    PARMSZ	.EQU	-06H		; number of bytes of parameters
    DATASZ	.EQU	-08H		; number of bytes of local data segment
    ; -0AH to -08H-2*(# of long jumps)	self-relative jump address
 */
struct pprocdescriptor
{
    BYTE procnum;                                                   // redundant?
    SBYTE lexlevel;                                                 // lexical level (PROGRAM PASCALSYSTEM=-1)
    bool isbaseproc() const { return lexlevel < 1; }                // -1=PROGRAM PASCALSYSTEM, 0=OS procs in GLOBALS.TEXT and related
    // these elements are stored with negative indices
    CODE * entryIP() const { return ((RELPTR<CODE>*)this)[-1]; }    // entry point
    CODE * exitIP() const { return ((RELPTR<CODE>*)this)[-2]; }     // entry point for Exit()
    size_t parmsize() const { return ((WORD*)this)[-3]; }           // number of bytes of parameters plus space for retval
    size_t datasize() const { return ((WORD*)this)[-4]; }           // number of bytes of local data segment
    CODE * jumpIP(SBYTE n) const
    {
        // -2 seems an optimization for loops that go back to the first instruction
        if (n != -2 && n > -10)
            throw invalidpcodearg();
        return *(RELPTR<CODE>*) (n + (char*)this);
    }
};

// a procedure stack frame (aka "Mark stack control word," MSCW)
/*
    ; Mark stack control word (MSCW) format:
    MSSP	.EQU	+0AH		; Caller's top of stack
    MSIPC	.EQU	+08H		; Caller's IPC (return address)
    MSSEG	.EQU	+06H		; Caller's segment (proc table) pointer
    MSJTAB	.EQU	+04H		; Caller's jtab pointer
    MSDYN	.EQU	+02H		; Dynamic link - pointer to caller's MSCW
    MSSTAT	.EQU	+00H		; Static link - pointer to parent's MSCW
    MSBASE	.EQU	-02H		; Base link (only if CBP) - pointer
 */
// Interesting: A stack frame does not know its own proc id or lex level. Note that lex level cannot be inferred from caller.
// Note: Apple III (and probably //e > 64k) copy string constants to the frame stack; needs an extra entry here called STR that is the list of currently cached objects.
// Note: LINKER source says: MSDELTA = 12;       { mark stack size for pub/priv fixup }
struct pstackframe
{
    PTR<pstackframe> & callerbaseframeaddr() { return ((PTR<pstackframe>*)this)[-1]; }   // (only if CBP) - pointer to base stack frame of caller
    PTR<pstackframe> outerframeaddr;        // Static link - pointer to parent's MSCW (one lex level up), used to access intermediate-level variables
    PTR<pstackframe> callerframeaddr;       // Dynamic link - pointer to caller's MSCW, used when returning
    PTR<pprocdescriptor> procdescraddr;     // Caller's jtab pointer (restore upon return)
    PTR<pproctable> proctableaddr;          // Caller's segment (proc table) pointer (restore upon return)
    WORD IPoffset;                          // Caller's IPC (return address), as byte offset from PROC->entryIP
    WORD SPoffset;                          // Caller's top of stack (eval stack), relative to stackbottom
    //BYTE locals[];                        // variables start here (note: 1-based)
    template<typename T> T & locals(size_t wordindex1based) const
    {
        WORD * endptr = (WORD *) (sizeof(*this) + (char*)this); // local variables follow right after this
        WORD * varptr = &endptr[wordindex1based - 1];           // (debug here)
        return *(T*) varptr;
    }
};

};
