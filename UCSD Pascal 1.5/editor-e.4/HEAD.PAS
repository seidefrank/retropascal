(*$U-*)
PROGRAM PASCALSYSTEM;

CONST
  VIDLENG = 7;  (* Number of characters in a volume ID *)
  TIDLENG = 15;	(* Number of characters in a title ID *)
     
TYPE

  VID = STRING[VIDLENG];
  
  TID = STRING[TIDLENG];
  
  INFOREC = RECORD
              TRASH1,TRASH2: INTEGER;
              ERRSYM,ERRBLK,ERRNUM: INTEGER;    (* Error com for EDIT *)
              TRASH3: ARRAY [0..2] OF INTEGER;
              GOTSYM,GOTCODE: BOOLEAN;
              WORKVID,SYMVID,CODEVID: VID;      (* Perm&Cur workfile volumes *)
              WORKTID,SYMTID,CODETID: TID       (* Perm&Cur workfile titles *)
            END (*INFOREC*) ;
  
  SYSCOMREC = RECORD
                JUNK: ARRAY [0..6] OF INTEGER;
                LASTMP: INTEGER;
                EXPANSION: ARRAY [0..20] OF INTEGER;
                MISCINFO: PACKED RECORD
                            NOBREAK,STUPID,SLOWTERM,
                            HASXYCRT,HASLCCRT,HAS8510A,HASCLOCK: BOOLEAN
                          END;
                CRTTYPE: INTEGER;
                CRTCTRL: PACKED RECORD
                           RLF,NDFS,ERASEEOL,ERASEEOS,HOME,ESCAPE: CHAR;
                           BACKSPACE: CHAR;
                           FILLCOUNT: 0..255;
                           EXPANSION: PACKED ARRAY [0..3] OF CHAR
                         END;
                CRTINFO: PACKED RECORD
                           WIDTH,HEIGHT: INTEGER;
                           RIGHT,LEFT,DOWN,UP: CHAR;
                           BADCH,CHARDEL,STOP,BREAK,FLUSH,EOF: CHAR;
                           ALTMODE,LINEDEL: CHAR;
                           EXPANSION: PACKED ARRAY [0..5] OF CHAR
                         END
              END (*SYSCOM*);

VAR (* I.4 Globals as of 30-Jan-78 *)
  SYSCOM: ^SYSCOMREC;
  TRASHY: ARRAY [0..5] OF INTEGER;
  USERINFO: INFOREC;
  TRASHYY: ARRAY [0..4] OF INTEGER;
  SYVID,DKVID: VID;


SEGMENT PROCEDURE EDITOR;
CONST
  (* Unless otherwise noted all constants are upper bounds
     from zero.					        *)

  MAXBUFSIZE=32767;
  MAXSW=84; (* Maximum allowable SCREENWIDTH *)
  MAXSTRING=127;
  MAXCHAR=1023;  (* The maximum number of characters on a line in the EBUF *)
  TIDLENG=15;  (* From SYSCOM *)
  CHARINBUF=2048;  (* For final version. Not used. *)
  MAXOFFSET=1023; (* Maximum offset in a page *)
  MAXPAGE=255; (* Ridiculous upper bound! *)
  
  (* The following ASCII characters are hard-wired in *)
  HT=9; LF=10; EOL=13; DLE=16; SP=32;
  DC1=17; BELL=7; RUBOUT=127; CR=13;
  

TYPE
  PTRTYPE=0..MAXBUFSIZE;
  BUFRTYPE=PACKED ARRAY [0..0] OF CHAR;
  BLOCKTYPE=PACKED ARRAY [0..511] OF CHAR;
  ERRORTYPE=(FATAL,NONFATAL);
  OFFSET=0..MAXOFFSET;
  PAGE=0..MAXPAGE;
  NAME=PACKED ARRAY [0..7] OF CHAR;
  PTYPE=PACKED ARRAY [0..MAXSTRING] OF CHAR;
  COMMANDS=(ILLEGAL,ADJUSTC,COPYC,DELETEC,FINDC,INSERTC,JUMPC,LISTC,MACRODEFC,
            PARAC,QUITC,REPLACEC,SETC,VERIFYC,XECUTEC,ZAPC,REVERSEC,FORWARDC,UP,
            DOWN,LEFT,RIGHT,TAB,DIGIT,DUMPC,ADVANCE,SPACE,EQUALC,SLASHC);
  CTYPE=(FS,GOHOME,ETOEOL,ETOEOS,US);
  
  HEADER=    (* Page Zero layout changed 13-Jan-78 *)
    RECORD CASE BOOLEAN OF
      TRUE: (BUF: PACKED ARRAY[0..MAXOFFSET] OF CHAR);
      FALSE:(DEFINED:    BOOLEAN; (* New file nulls => false *)
             COUNT:      INTEGER;  (* The count of valid markers *)
             NAME:       ARRAY [0..9] OF PACKED ARRAY [0..7] OF CHAR;
             PAGEN:      PACKED ARRAY [0..9] OF PAGE;
             POFFSET:    PACKED ARRAY [0..9] OF OFFSET;
             AUTOINDENT: BOOLEAN; (* Environment stuff follows *)
             FILLING:    BOOLEAN;
             TOKDEF:     BOOLEAN;
             LMARGIN:    0..MAXSW;
             RMARGIN:    0..MAXSW;
             PARAMARGIN: 0..MAXSW;
             RUNOFFCH:   CHAR;
             FILLER:     PACKED ARRAY [0..895] OF CHAR)
    END;
    


VAR
  CURSOR: 0..MAXBUFSIZE;
  LINE1PTR: 0..MAXBUFSIZE;
  BUFCOUNT: 0..MAXBUFSIZE;   (* Number of valid characters in the EBUF *)
  STUFFSTART: 0..MAXBUFSIZE; (* GETLEADING sets this *)
  LINESTART: 0..MAXBUFSIZE;  (*  ditto!              *)
  BYTES,BLANKS: INTEGER;     (*  ditto!              *)
  CH: CHAR;
  DIRECTION: CHAR;               (* '>' OR '<' *)
  REPEATFACTOR: INTEGER;
  BUFSIZE: INTEGER;
  SCREENWIDTH,MIDDLE: INTEGER;   (* Moved to var 26-Jan *)
  SCREENHEIGHT: INTEGER;         (*   "    "  "    "    *)
  EBUF: ^BUFRTYPE;
  KIND: ARRAY [CHAR] OF INTEGER; (* for token find *)
  NEEDPROMPT: BOOLEAN;
  ETX,BS,DEL,ESC: INTEGER;          (* Moved from CONST 30-Jan-78 *)
  COMMAND: COMMANDS;
  LASTPAT: 0..MAXBUFSIZE;
  TRASH: INTEGER;                (* Totally without redeeming social value *)
  TARGET: PTYPE;
  SUBSTRING: PTYPE;
  SLENGTH,TLENGTH: INTEGER;      (* Length of target and substring *)
  SDEFINED,TDEFINED: BOOLEAN;    (* Whether the strings are valid *)
  COPYLENGTH,COPYSTART: PTRTYPE; (* For Copyc *)
  COPYLINE,COPYOK: BOOLEAN;      (*    "      *)
  INFINITY: BOOLEAN;             (* for slashc *)
  THEFILE: FILE;
  TRANSLATE: ARRAY [CHAR] OF COMMANDS;
  PAGEZERO: HEADER;
  MSG: STRING;
  PROMPTLINE: STRING;

SEGMENT PROCEDURE NUM2; BEGIN END;      SEGMENT PROCEDURE NUM3; BEGIN END;
SEGMENT PROCEDURE NUM4; BEGIN END;      SEGMENT PROCEDURE NUM5; BEGIN END;
SEGMENT PROCEDURE NUM6; BEGIN END;      SEGMENT PROCEDURE NUM7; BEGIN END;
SEGMENT PROCEDURE NUM8; BEGIN END;      SEGMENT PROCEDURE NUM9; BEGIN END;



