// reconstructed from ASM sources of 2006 distribution
// (C) 2013 Frank Seide

#pragma once

#define PCODE_II    // some opcodes differ, LDO moved!

// Anything I find interesting w.r.t. documentation:
/*
; Operator formats:
;   RBP,RNP: number of words to return (0..2)
;   CBP,CGP,CLP,CIP: proc_num
;   CXP: seg_num, proc_num

; Constants
NIL	.EQU	0001H		; value of NIL pointer
MAXSEG	.EQU	0FH		; max segment #
MSCWSIZE .EQU	0CH		; size of a mark stack control word
DISP0	.EQU	0AH		; Offset from MSSTAT of variable with offset
				; of 0

; Internal P-machine registers, widely used temporaries
	.ALIGN	2
NP	.WORD	0		; ^top_of_heap
MPD0	.WORD	0		; ^local var with offset of zero
BASED0	.WORD	0		; ^global var with offset of zero
IPCSAV	.WORD	0		; save IPC on complex ops, and for XEQERR
FPERROR .WORD	0		; fp error status
RETADR	.WORD	0  
NEWSP	.WORD	0  
LTSTRNG .BYTE	01H		; char to string conversion
	.BYTE	0  

; Internal segment table, contains refcounts and addr of each seg
	.ALIGN	2
INTSEGT .BLOCK	<MAXSEG+1>*4
 */

namespace psystem
{

enum opcodes
{
    // SLDCI: Short load constant word
    SLDCI_00, SLDCI_01, SLDCI_02, SLDCI_03, SLDCI_04, SLDCI_05, SLDCI_06, SLDCI_07, SLDCI_08, SLDCI_09, SLDCI_0a, SLDCI_0b, SLDCI_0c, SLDCI_0d, SLDCI_0e, SLDCI_0f,
    SLDCI_10, SLDCI_11, SLDCI_12, SLDCI_13, SLDCI_14, SLDCI_15, SLDCI_16, SLDCI_17, SLDCI_18, SLDCI_19, SLDCI_1a, SLDCI_1b, SLDCI_1c, SLDCI_1d, SLDCI_1e, SLDCI_1f,
    SLDCI_20, SLDCI_21, SLDCI_22, SLDCI_23, SLDCI_24, SLDCI_25, SLDCI_26, SLDCI_27, SLDCI_28, SLDCI_29, SLDCI_2a, SLDCI_2b, SLDCI_2c, SLDCI_2d, SLDCI_2e, SLDCI_2f,
    SLDCI_30, SLDCI_31, SLDCI_32, SLDCI_33, SLDCI_34, SLDCI_35, SLDCI_36, SLDCI_37, SLDCI_38, SLDCI_39, SLDCI_3a, SLDCI_3b, SLDCI_3c, SLDCI_3d, SLDCI_3e, SLDCI_3f,
    SLDCI_40, SLDCI_41, SLDCI_42, SLDCI_43, SLDCI_44, SLDCI_45, SLDCI_46, SLDCI_47, SLDCI_48, SLDCI_49, SLDCI_4a, SLDCI_4b, SLDCI_4c, SLDCI_4d, SLDCI_4e, SLDCI_4f,
    SLDCI_50, SLDCI_51, SLDCI_52, SLDCI_53, SLDCI_54, SLDCI_55, SLDCI_56, SLDCI_57, SLDCI_58, SLDCI_59, SLDCI_5a, SLDCI_5b, SLDCI_5c, SLDCI_5d, SLDCI_5e, SLDCI_5f,
    SLDCI_60, SLDCI_61, SLDCI_62, SLDCI_63, SLDCI_64, SLDCI_65, SLDCI_66, SLDCI_67, SLDCI_68, SLDCI_69, SLDCI_6a, SLDCI_6b, SLDCI_6c, SLDCI_6d, SLDCI_6e, SLDCI_6f,
    SLDCI_70, SLDCI_71, SLDCI_72, SLDCI_73, SLDCI_74, SLDCI_75, SLDCI_76, SLDCI_77, SLDCI_78, SLDCI_79, SLDCI_7a, SLDCI_7b, SLDCI_7c, SLDCI_7d, SLDCI_7e, SLDCI_7f,
    ABI,            // ABI: INTEGER ABSOLUTE VALUE
    ABR,            // ABR: REAL ABSOLUTE VALUE
    ADI,            // ADI: ADD INTEGER
    ADR,            // ADR: ADD REAL
    AND,            // AND: LOGICAL AND
    DIF,            // DIF: SET DIFFERENCE
    DVI,            // DVI: INTEGER DIVIDE
    DVR,            // DVR: REAL DIVIDE
    CHK,            // CHK: CHECK INDEX OR RANGE
    FLO,            // FLO: FLOAT NEXT TO TOP-OF-STACK
    FLT,            // FLT: FLOAT TOP-OF-STACK
    INN,            // INN: SET INCLUSION
    INTS,           // INT: SET INTERSECTION (renamed from INT since name conflicts with INT type)
    IOR,            // IOR: LOGICAL OR
    MOD,            // MOD: INTEGER REMAINDER DIVIDE
    MPI,            // MPI: INTEGER MULTIPLY
    MPR,            // MPR: REAL MULTIPLY
    NGI,            // NGI: INTEGER NEGATION
    NGR,            // NGR: REAL NEGATION
    NOT,            // NOT: LOGICAL NOT
    SRS,            // SRS: BUILD SUBRANGE SET
    SBI,            // SBI: INTEGER SUBTRACT
    SBR,            // SBR: REAL SUBTRACT
    SGS,            // SGS: MAKE SINGLETON SET
    SQI,            // SQI:  SQUARE INTEGER
    SQR,            // SQR: SQUARE REAL
    STO,            // STO: STORE INDIRECT
    IXS,            // IXS: STRING INDEX...DYNAMIC RANGE CHECK
    UNI,            // UNI: SET UNION
#ifdef PCODE_II     // version II
    LDE,            // load extended
#else               // Z80 code of 1.5
    S2P,            // S2P: STRING TO PACKED ARRAY CONVERT
#endif
    CSP,            // CSP: call special procedure
    LDCN,           // LDCN: LOAD CONSTANT NIL
    ADJ,            // ADJ: SET ADJUST
    FJP,            // FJP: BRANCH IF FALSE ON TOS
    INC,            // INC: INCREMENT TOS BY PARAM
    IND,            // IND: INDIRECT LOAD
    IXA,            // IXA: INDEX ARRAY
    LAO,            // LAO: LOAD GLOBAL ADDRESS
    LCA,            // LCA: LOAD CONSTANT (STRING) ADDRESS
#ifdef PCODE_II     // version II
    // version II has LDO on 169, not 167; 167 is something else; no MVB
    LAE,            // load address extended
    MOV,            // MOV: MOVE WORDS
    LDO,            // LDO: LOAD GLOBAL
#else               // Z80 code of 1.5
    LDO,            // LDO: LOAD GLOBAL
    MOV,            // MOV: MOVE WORDS
    MVB,            // MVB: MOVE BYTES
#endif
    SAS,            // SAS: STRING ASSIGNMENT
    SRO,            // SRO: STORE GLOBAL
    XJP,            // XJP: INDEX JUMP
    RNP,            // RNP: Return from normal procedure
    CIP,            // CIP: Call intermediate procedure
    COMPAR_EQ,      // COMPAR: ; COMPARE COMPLEX THINGS
    COMPAR_GE,      // COMPAR: ; COMPARE COMPLEX THINGS
    COMPAR_GT,      // COMPAR: ; COMPARE COMPLEX THINGS
    LDA,            // LDA: LOAD INTERMEDIATE ADDRESS
    LDC,            // LDC: LOAD MULTIWORD CONSTANT
    COMPAR_LE,      // COMPAR: ; COMPARE COMPLEX THINGS
    COMPAR_LT,      // COMPAR: ; COMPARE COMPLEX THINGS
    LOD,            // LOD: LOAD INTERMEDIATE VALUE
    COMPAR_NE,      // COMPAR: ; COMPARE COMPLEX THINGS
    STR,            // STR: STORE INTERMEDIATE VALUE
    UJP,            // UJP: BRANCH UNCONDITIONAL
    LDP,            // LDP: LOAD PACKED FIELD
    STP,            // STP: STORE PACKED FIELD
    LDM,            // LDM: LOAD MULTIPLE WORDS
    STM,            // STM: STORE MULTIPLE WORDS
    LDB,            // LDB: LOAD BYTE
    STB,            // STB: STORE BYTE
    IXP,            // IXP: INDEX PACKED ARRAY
    RBP,            // RBP: Return from base procedure
    CBP,            // CBP: Call base procedure
    EQUI,           // EQUI: INTEGER EQUAL COMPARE
    GEQI,           // GEQI: INTEGER GREATER OR EQUAL COMPARE
    GRTI,           // GRTI: INTEGER GREATER THAN COMPARE
    LLA,            // LLA: LOAD LOCAL ADDRESS
    LDCI,           // LDCI: LOAD LONG INTEGER CONSTANT
    LEQI,           // LEQI: INTEGER LESS THAN OR EQUAL COMPARE
    LESI,           // LESI: INTEGER LESS THAN COMPARE
    LDL,            // LDL: LOAD LOCAL
    NEQI,           // NEQI: INTEGER NOT EQUAL COMPARE
    STL,            // STL: STORE LOCAL
    CXP,            // CXP: Call external (different segment) procedure
    CLP,            // CLP: Call local procedure
    CGP,            // CGP: Call global procedure
#ifdef PCODE_II     // version II
    LPA,            // load packed array
    STE,            // store external
#else               // Z80 code of 1.5
    S1P,            // S1P: STRING TO PACKED ON TOS
    IXB,            // IXB: INDEX BYTE ARRAY
#endif
    BYT,            // BYT: CONVERT WORD TO BYTE ADDR --never generated by compiler (?), not implemented in the Miller system
    EFJ,            // EFJ: INTEGER = THEN FJP
    NFJ,            // NFJ: INTEGER <> THEN FJP
    BPT,            // BPT: CONDITIONAL HALT (BREAKPOINT)
    XIT,            // XIT: EXIT SYSTEM
    NOP,            // NOP: NO OPERATION  (Z80: 'BACK')
    // SLDL: Short load local word
    SLDL_01, SLDL_02, SLDL_03, SLDL_04, SLDL_05, SLDL_06, SLDL_07, SLDL_08, SLDL_09, SLDL_0a, SLDL_0b, SLDL_0c, SLDL_0d, SLDL_0e, SLDL_0f, SLDL_10,
    // SLDO: Short load global word - just like SLDL
    SLDO_01, SLDO_02, SLDO_03, SLDO_04, SLDO_05, SLDO_06, SLDO_07, SLDO_08, SLDO_09, SLDO_0a, SLDO_0b, SLDO_0c, SLDO_0d, SLDO_0e, SLDO_0f, SLDO_10,
    SIND_0,         // SIND0: Short index and load word, index=0 (load indirect)
    // SIND: Short static index and load word
    SINDS_1, SINDS_2, SINDS_3, SINDS_4, SINDS_5, SINDS_6, SINDS_7
    // how this was constructed:
    //  - get XFRTBL section from I.5-PDP-INTERP.TXT
    //  - convert using find-replace + manual editing for special cases;
    //  - extract a pattern file for grep, then grep for <inst>: to get comment from respective jump label :)
    //    grep . b | sed -e 's/  */ /g' -e 's/$/:/' > pat
    //    grep -f pat I.5-PDP-INTERP.TXT > rem
    //    awk '/;/{T=$1;CS[T]=$0;}/^    /{I=$1":";C=CS[I];print C}' rem a
    //  - rectangle-paste it back, manually fix it up
    //  - missing Cxx/Rxx pasted in from I5Z80Interp.TXT
    /*  - SLDCI
        foreach h ( 0 1 2 3 4 5 6 7 ) 
            foreach l ( 0 1 2 3 4 5 6 7 8 9 a b c d e f )
                echo -n SLDCI_$h$l,
            end
            echo
        end
    */
};
static_assert(SINDS_7 == 255, "opcode table not in sync");

enum cspcodes
{
    IOC,		// 0
    NEW,
    MVL,
    MVR,
    EXIT,
    UREAD,		// 5
    UWRITE,
    IDS,
    TRS,
    TIM,
    FLC,		// 10
    SCN,
    USTAT,
    GSEG = 21,
    RSEG,
    TNC,
    RND,
    SIN,		// 25
    COS,
    LOG,
    ATAN,
    LN,
    EXP,		// 30
    SQT,
    MRK,
    RLS,
    IORP,
    UBUSY,		// 35
    POT,
    UWAIT,
    UCLEAR,
    HALT,               // (Z80: HLT; conflicts with opcode)
    MEMA,	        // 40
    // our own special code
    DECOPS = 64         // we modified the compiler to do DECOPS as a CSP
};
static_assert(MEMA == 40, "CSP codes not in sync");

enum decopscodes        // instruction codes for decimal operations (from Z80 source code)
{
    DAJ = 0,
    DAD = 2,
    DSB = 4,
    DNG = 6,
    DMP = 8,
    DDV = 10,
    DSTR = 12,
    DCV = 14,         // note: not in Klebsch/Miller
    DECCMP = 16,
    DCVT = 18,
    DTNC = 20
};

};
