// globals from GLOBALS.TEXT
// (C) 2013 Frank Seide

#pragma once

#include "ptypes.h"

namespace psystem
{

static const size_t MMAXINT = 32767;   // MAXIMUM INTEGER VALUE
static const size_t MAXDIR = 77;       // MAX NUMBER OF ENTRIES IN A DIRECTORY
static const size_t VIDLENG = 7;       // NUMBER OF CHARS IN A VOLUME ID
static const size_t TIDLENG = 15;      // NUMBER OF CHARS IN TITLE ID
static const size_t MAX_SEG = 15;      // MAX CODE SEGMENT NUMBER in CODE-file segment dictionaries
// ^^ note: our original GLOBALS.TEXT source says 31, but this is a bug; 31 is the size of the runtime segment table
static const size_t MAX_RESIDENT_SEG = 31;      // MAX CODE SEGMENT NUMBER; Apple III: 63, TODO: change to that if it does not harm
static const size_t FBLKSIZE = 512;    // STANDARD DISK BLOCK LENGTH
static const size_t DIRBLK = 2;        // DISK ADDR OF DIRECTORY
static const size_t AGELIMIT = 300;    // MAX AGE FOR GDIRP...IN TICKS
static const size_t EOL = 13;          // END-OF-LINE...ASCII CR
static const size_t DLE = 16;          // BLANK COMPRESSION CODE
static const size_t NAME_LEN = 23;     // Length of CONCAT(VIDLENG,':',TIDLENG)
static const size_t FILL_LEN = 11;     // Maximum # of nulls in FILLER

struct SEG_DESC
{
    INT DISKADDR;       // REL BLK IN CODE...ABS IN SYSCOM!
    INT CODELENG;       // # BYTES TO READ IN; 0 means empty entry (Apple III Technical Reference Manual)
};

struct SEG_ENTRY
{
    INT/*UNITNUM*/ CODEUNIT;
    SEG_DESC CODEDESC;
};

struct SYSCOMREC
{
    // general execution
    WORD/*IORSLTWD*/ IORSLT;        // RESULT OF LAST IO CALL
    WORD XEQERR;                    // REASON FOR EXECERROR CALL
    WORD SYSUNIT;                   // PHYSICAL UNIT OF BOOTLOAD
    // debugger
    WORD BUGSTATE;                  // DEBUGGER INFO
    // disk
    PTR<BYTE/*DIRECTORY*/> GDIRP;   // GLOBAL DIR POINTER,SEE VOLSEARCH
    // crash information
    PTR<struct pstackframe> BOMBP;
    PTR<void> STKBASE;
    PTR<struct pstackframe> LASTMP;
    PTR<void> MEMTOP;
    PTR<struct pproctable> SEG;
    PTR<struct pprocdescriptor> JTAB;
    WORD BOMBIPC;                   // WHERE XEQERR BLOWUP WAS (relative IP)
    WORD HLTLINE;                   // MORE DEBUGGER STUFF
    WORD BRKPTS[4];                 // line numbers of breakpoints; BPT checks against these
    INT RETRIES;                    // DRIVERS PUT RETRY COUNTS
    INT EXPANSION[9];
    WORD LOWTIME, HIGHTIME;
    INT MISCINFO;                   // (expand type if needed)
    /*
        MISCINFO: PACKED RECORD
                    NOBREAK,STUPID,SLOWTERM,
                    HASXYCRT,HASLCCRT,HAS8510A,HASCLOCK: BOOLEAN;
                    USERKIND:(NORMAL, AQUIZ, BOOKER, PQUIZ);
                    WORD_MACH, IS_FLIPT : BOOLEAN
                  END;
     */
    INT CRTTYPE;
    BYTE CRTCTRL[12];               // (expand type if needed)
    /*
        CRTCTRL: PACKED RECORD
                   RLF,NDFS,ERASEEOL,ERASEEOS,HOME,ESCAPE: CHAR;
                   BACKSPACE: CHAR;
                   FILLCOUNT: 0..255;
                   CLEARSCREEN, CLEARLINE: CHAR;
                   PREFIXED: PACKED ARRAY [0..15] OF BOOLEAN
                 END;
     */
    WORD CRTINFO[11];               // (expand if needed)
    /*
        CRTINFO: PACKED RECORD
                   WIDTH,HEIGHT;
                   RIGHT,LEFT,DOWN,UP: CHAR;
                   BADCH,CHARDEL,STOP,BREAK,FLUSH,EOF: CHAR;
                   ALTMODE,LINEDEL: CHAR;
                   ALPHA_LOCK,ETX,PREFIX: CHAR;
                   PREFIXED: PACKED ARRAY [0..15] OF BOOLEAN
                 END;
     */
    // use of segments, based on ASSOCIATE() source code:
    //  0: boot code; this code is never unloaded
    //  1, 7+: gets overwritten by user programs during ASSOCIATE
    //  2..6: not touched by ASSOCIATE
    // segments 1..5 are loaded from the boot code
    // SYSTEM.LIBRARY contains these segments, but somehow the compiler does not find them
    //  4: TURTLEGR
    //  13: TRANSCEN
    //  14: LONGINTE
    //  15: PASCALIO
    SEG_ENTRY SEGTABLE[MAX_RESIDENT_SEG+1];              // runtime segment table; FIXME: seems to only have 16 entries? Above that looks like garbage in actual SYSCOM (is that comment up-to-date?)

    SYSCOMREC() { memset(this, 0, sizeof(*this)); }
};

struct DATEREC
{
    unsigned __int16 month : 4;
    unsigned __int16 day : 5;
    unsigned __int16 year : 7;    // "100 is temp disk flag"
};

enum segkinds
{
    LINKED,             // fully executable, no unresolved references
    HOSTSEG,            // outer block of Pascal program with unresolved references
    SEGPROC,            // (segment procedure; unused acc. to Apple TechNote #16)
    UNITSEG,            // compiled unit (regular)
    SEPRTSEG,           // separately compiled procedure / set of procedures; assembly modules
    // Apple extensions:
    UNLINKED_INTRINS,   // intrinsic unit with unresolved calls to assembly
    LINKED_INTRINS,     // intrinsic unit, ready to run
    DATASEG             // intrinsic unit data segment 
};

// segment dictionary in a CODE file
// Every CODE file begins with this.
// more info on Apple extensions in Apple TechNote #16, e.g. in 'apple_forumdesdeveloppeurs_05_tn_pascal.pdf', page 85
struct SEGTBL   // TODO: rename to SEGDICT, per Apple III Technical Reference Manual
{
#if 0
    // from SYSTEM: this is WRONG as it uses a 0..31 SEGRANGE
    SEGTBL: RECORD
              DISKINFO: ARRAY [SEGRANGE] OF SEGDESC;
              SEGNAME: ARRAY [SEGRANGE] OF 
                         PACKED ARRAY [0..7] OF CHAR;
              SEGKIND: ARRAY [SEGRANGE] OF
                         (LINKED,HOSTSEG,SEGPROC,UNITSEG,SEPRTSEG);
              FILLER: ARRAY [0..143] OF INTEGER
            END { SEGTBL } ;
    // from LIBRARY.TEXT:
    BLOCK0 = RECORD
               SEGDSC: ARRAY [SEGRANGE] OF SEGDESC;
               SEGNAME: ARRAY [SEGRANGE] OF
                          PACKED ARRAY [0..7] OF CHAR;
               SEGKIND: ARRAY [SEGRANGE] OF INTEGER;
               EXTRA: ARRAY [SEGRANGE] OF INTEGER;
               FILLER: ARRAY [1..88] OF INTEGER;
               NOTICE: STRING[79]
             END;
#endif
    static const size_t capacity = MAX_SEG+1;
    SEG_DESC DISKINFO[capacity];        // descriptor for each segment; note: DISKADDR is relative to file
    CHARARRAY<8> SEGNAME[capacity];
    WORD/*segkinds*/ SEGKIND[capacity];
    WORD TEXTADDR[capacity];            // block number where INTERFACE text of a unit starts
    // to run code, there are 3 kinds of segments in the SYSCOM::SEGTABLE: [TN #16]
    //  - one entry per segment stored in this file (DISKINFO), numbers fixed by compiler
    //     - main segment = 1
    //     - 7ff are segment procedures of user program and linked units
    //       (units may be renumbered during linking)
    //    This SEGTBL lists the above only.
    //  - 6 system segments (fixed to 0, 2..6) (8 for Apple ///: also 62 and 63)
    //  - one entry per referenced intrinsic unit code or data segment
    //     - intrinsic seg no come from 'intrinsic' declaration when compiling UNIT
    //       and are fixed (seems Apple only; UCSD II.0 compiler does not understand that).
    //       Cannot be system unit (0, 2..6) or
    //       "magic units" PASCALIO and LONGINTIO at segs 30 and 31
    //       ("magic" for the compiler??)
    //       On the Miller disks, PASCALIO is segment 15 and includes 2 LONGINT I/O routines.
    // Apple allows capacity=16 code user segments plus 10 intrinsic references (plus system).
    struct
    {
        BYTE SEGNUM : 8;    // segment will go into this position in SYSCOM::SEGTABLE
        BYTE MTYPE : 4;     // (0=unidentified, 1=pcodebigendian, 2=pcodelittleendian, 3..9=assembly 7=6502)
        BYTE UNUSED : 1;    // ^^ Miller disk system segment #0 is 0, not 2
        BYTE VERSION : 3;   // 2=Apple ][, 3=Apple ///
    } SEGINFO[capacity];    // what is this? Miller code tests this
    unsigned __int64 INTRINS_SEGS;  // bit vector (low byte first): 1 means segment is intrinsic
    // ^^ only first 32 on Apple ][; 0 on Miller disks obviously since they don't have intrinsics
    BYTE INT_NAM_CHECKSUM[64];  // (Apple /// only; 0 on Apple ][)
    BYTE FILLER[72];
    // starting at byte 432:
    CHARARRAY<80> COMMENT;  // code comment $C option
};
static_assert(sizeof(SEGTBL) == 512, "SEGTBL size is off");

struct FIB
{
    PTR<void> WINDOWP;          // (*USER WINDOW...F^, USED BY GET-PUT*)
    WORD/*BOOLEAN*/ FEOLN,FEOF;
    WORD FSTATE;                // (FJANDW,FNEEDCHAR,FGOTCHAR);
    INT FRECSIZE;               // (*IN BYTES...0=>BLOCKFILE, 1=>CHARFILE*)
    WORD/*BOOLEAN*/ FISOPEN;
    WORD/*BOOLEAN*/ FISBLKD;    // (*FILE IS ON BLOCK DEVICE*)
    WORD FUNIT;                 // (*PHYSICAL UNIT #*)
    // more; will add as needed
};

};
