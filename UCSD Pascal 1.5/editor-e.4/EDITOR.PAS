(**********************************************************************)
(*                                                                    *)
(*    Screen Oriented Editor                        February 4, 1978  *)
(*    ------ -------- ------                                          *)
(*                                                   _________        *)
(*    By Richard S. Kaufmann,                       /         \       *)
(*       IIS                                        | Version |       *)
(*       University of California, San Diego        |   E.4a  |       *)
(*       La Jolla CA 92093                          \_________/       *)
(*                                                                    *)
(*    Copyright (c) 1978, by The Regents of the University of         *)
(*       California at San Diego                                      *)
(*                                                                    *)
(**********************************************************************)

(*$I SOURCE:HEAD *)

(* Forward declared procedures.. all procedures are in MISC and UTIL *)

PROCEDURE ERROR(S:STRING;HOWBAD:ERRORTYPE); FORWARD;
PROCEDURE ERASETOEOL; FORWARD;
FUNCTION  GETCH:CHAR; FORWARD;
FUNCTION  MAPTOCOMMAND(CH:CHAR): COMMANDS; FORWARD;
FUNCTION  UCLC(CH:CHAR): CHAR; FORWARD;
PROCEDURE PROMPT; FORWARD;
PROCEDURE REDISPLAY; FORWARD;
FUNCTION  MIN(A,B:INTEGER): INTEGER; FORWARD;
FUNCTION  MAX(A,B:INTEGER): INTEGER; FORWARD;
PROCEDURE CONTROL(CH:CTYPE); FORWARD;
PROCEDURE PUTMSG; FORWARD;
PROCEDURE HOME; FORWARD;
PROCEDURE ERRWAIT; FORWARD;
PROCEDURE BLANKCRT(Y: INTEGER); FORWARD;
FUNCTION  LEADBLANKS(PTR:PTRTYPE;VAR BYTES: INTEGER): INTEGER; FORWARD;
PROCEDURE CENTERCURSOR(VAR LINE: INTEGER; LINESUP: INTEGER; NEWSCREEN:BOOLEAN);
  FORWARD;
PROCEDURE FINDXY(VAR INDENT,LINE: INTEGER); FORWARD;
PROCEDURE SHOWCURSOR; FORWARD;
FUNCTION  GETNUM: INTEGER; FORWARD;
PROCEDURE GETLEADING; FORWARD;
FUNCTION  OKTODEL(CURSOR,ANCHOR:PTRTYPE):BOOLEAN; FORWARD;
PROCEDURE LINEOUT(VAR PTR:PTRTYPE; BYTES,BLANKS,LINE: INTEGER); FORWARD;
PROCEDURE UPSCREEN(FIRSTLINE,WHOLESCREEN:BOOLEAN; LINE: INTEGER); FORWARD;
PROCEDURE READJUST(CURSOR: PTRTYPE; DELTA: INTEGER); FORWARD;
PROCEDURE THEFIXER(PARAPTR: PTRTYPE;RFAC: INTEGER;WHOLE: BOOLEAN); FORWARD;
PROCEDURE GETNAME(MSG:STRING; VAR M:NAME); FORWARD;

(*$I SOURCE:INIT     *)
(*$I SOURCE:OUT      *)
(*$I SOURCE:ENVIRON  *)
(*$I SOURCE:COMMAND  *)
(*$I SOURCE:INSERTIT *)
(*$I SOURCE:MOVEIT   *)
(*$I SOURCE:FIND     *)
(*$I SOURCE:USER     *)
(*$I SOURCE:MISC     *)
(*$I SOURCE:UTIL     *)

PROCEDURE PUTSYNTAX;
VAR
  D0,D1,D2,BLK,PTR,COLON: INTEGER;
  T,C:PACKED ARRAY [0..2] OF CHAR;
  F: FILE;
  BUF:PACKED ARRAY [0..1023] OF CHAR;

PROCEDURE PUTNUM;
BEGIN
  MSG:='Syntax Error #'; PUTMSG;
  WRITE(USERINFO.ERRNUM,'. Type <sp>');
END;

BEGIN (* putsyntax *)
  WITH USERINFO DO
    BEGIN
      OPENOLD(F,'*SYSTEM.SYNTAX');
      IF IORESULT<>0 THEN PUTNUM
      ELSE
        BEGIN
          IF ERRNUM<=109 THEN BLK:=2
          ELSE
            IF ERRNUM<=131 THEN BLK:=4
            ELSE
              IF ERRNUM<=156 THEN BLK:=6
              ELSE
                IF ERRNUM<=254 THEN BLK:=8
                ELSE BLK:=10;
          IF BLOCKREAD(F,BUF,2,BLK)<>2 THEN PUTNUM
          ELSE
            BEGIN
              IF BUF[0]=CHR(DLE) THEN PTR:=3 ELSE PTR:=0;
              D0:=ERRNUM DIV 100; (* convert error number to characters *)
              D1:=(ERRNUM-D0*100) DIV 10;
              D2:=ERRNUM MOD 10;
              T[0]:=CHR(D0+ORD('0')); T[1]:=CHR(D1+ORD('0'));
              T[2]:=CHR(D2+ORD('0'));
              REPEAT
                FILLCHAR(C,3,'0');
                COLON:=SCAN(MAXCHAR,=':',BUF[PTR]);
                MOVELEFT(BUF[PTR],C[3-COLON],COLON);
                COLON:=COLON+PTR;
                PTR:=SCAN(MAXCHAR,=CHR(EOL),BUF[PTR])+PTR+3
              UNTIL (T=C) OR (BUF[PTR]=CHR(0));
              IF BUF[PTR]=CHR(0) THEN PUTNUM
              ELSE
                BEGIN
                  MOVELEFT(BUF[COLON+1],MSG[1],(PTR-COLON)-4);
                  MSG[0]:=CHR(MIN(68,(PTR-COLON)-4)); (* R- required *)
                  HOME; ERASETOEOL; WRITE(MSG,'.  Type <sp>');
                END
            END
          END(* if ioresult<>0 *);
        SHOWCURSOR;
        REPEAT UNTIL GETCH=' ';
        ERRBLK:=0; ERRSYM:=0; ERRNUM:=0; (*  Only yell once!!! *)
      END(* with userinfo *)
  END(* putsyntax *);
  
BEGIN (* Segment procedure EDITOR *)
  INITIALIZE; GETLEADING; CURSOR:=MAX(CURSOR,STUFFSTART);
  REPEAT
    CENTERCURSOR(TRASH,(SCREENHEIGHT DIV 2)+1,TRUE);
    NEEDPROMPT:=TRUE;
    IF USERINFO.ERRBLK>0 THEN PUTSYNTAX;
    REPEAT
      HOME; ERASETOEOL;
      EDITCORE;
      IF COMMAND=SETC THEN ENVIRONMENT
    UNTIL COMMAND<>SETC;
  UNTIL OUT;
  SYSCOM^.MISCINFO.NOBREAK := FALSE  (* 28 SEPT 77*)
END;

BEGIN END.
