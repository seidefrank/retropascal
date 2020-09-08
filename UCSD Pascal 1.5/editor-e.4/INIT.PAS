SEGMENT PROCEDURE INITIALIZE;
LABEL 1;
VAR 
  BLOCK: ^BLOCKTYPE;
  DONE,OVFLW: BOOLEAN;
  CH: CHAR;
  QUIT,GAP,BLKS,PAGE,NOTNULS: INTEGER;
  FILENAME: STRING;
  BUFFER: PACKED ARRAY [0..1023] OF CHAR;

PROCEDURE MAP(CH:CHAR; C:COMMANDS);
BEGIN
  TRANSLATE[CH]:=C;
  IF CH IN ['A'..'Z'] THEN TRANSLATE[CHR(32+ORD(CH))]:=C; (* LC TOO *)
END;

BEGIN
  WITH PAGEZERO DO
    BEGIN
      
      (* Init the translate table *)
      
      FILLCHAR(TRANSLATE,SIZEOF(TRANSLATE),ILLEGAL);
      MAP('A',ADJUSTC);   MAP('C',COPYC);     MAP('D',DELETEC);
      MAP('F',FINDC);     MAP('I',INSERTC);   MAP('J',JUMPC);
      MAP('L',LISTC);     MAP('M',MACRODEFC); MAP('P',PARAC);
      MAP('Q',QUITC);     MAP('R',REPLACEC);  MAP('S',SETC);
      MAP('V',VERIFYC);   MAP('X',XECUTEC);   MAP('Z',ZAPC);
      MAP(',',REVERSEC);  MAP('>',FORWARDC);  MAP('.',FORWARDC);
      MAP('+',FORWARDC);  MAP('-',REVERSEC);  MAP('?',DUMPC);
      MAP('/',SLASHC);    MAP('=',EQUALC);    MAP('<',REVERSEC);

     
      (* Arrows *)
      
      (* NEXTCOMMAND and GETNUM handle VT-52 style vector keys *)
      IF SYSCOM^.CRTCTRL.ESCAPE=CHR(0) THEN
        BEGIN
          MAP(SYSCOM^.CRTINFO.LEFT,LEFT);
          MAP(SYSCOM^.CRTINFO.DOWN,DOWN);
          MAP(SYSCOM^.CRTINFO.RIGHT,RIGHT);
          MAP(SYSCOM^.CRTINFO.UP,UP);
        END;
      MAP(SYSCOM^.CRTINFO.CHARDEL,LEFT);
      MAP(CHR(EOL),ADVANCE); (* CR IS ADVANCE *)
      MAP(CHR(HT),TAB);
      MAP(CHR(SP),SPACE);


      (* Digits *)
      
      FOR CH:='0' TO '9' DO MAP(CH,DIGIT);


      (* Variable buffer sizing... added 17-Jan-78 *)
      
      QUIT:=10512+       (* Sizeof(editcore)-Sizeof(initialize) *)
            512;        (* Slop! *)
      MARK(EBUF);
      BLKS:=0;
      REPEAT
        NEW(BLOCK);
        BLKS:=BLKS+1;
        GAP:=ORD(SYSCOM^.LASTMP)-ORD(BLOCK)-512
      UNTIL ((GAP>0) AND (GAP<QUIT)) OR (BLKS=63);
      BUFSIZE:=BLKS*512-1;
      
      
      (* Open the workfile *)
      
      FILLCHAR(EBUF^,BUFSIZE+1,CHR(0));
      EBUF^[0]:=CHR(EOL);
      BUFCOUNT:=1;
      CURSOR:=1;
      HOME; WRITE('>Edit:'); ERASETOEOL;
      BLANKCRT(1);
      IF USERINFO.GOTSYM THEN
        BEGIN
          RESET(THEFILE,CONCAT(USERINFO.SYMVID,':',USERINFO.SYMTID));
          IF IORESULT<>0 THEN ERROR('Workfile lost.',FATAL)
        END
      ELSE
        BEGIN
          MSG:='No workfile is present. File? ( <ret> for no file ) ';
          REPEAT
            WRITE(MSG);
            READLN(INPUT,FILENAME);
            IF LENGTH(FILENAME)=0 THEN
              BEGIN
                FILLCHAR(PAGEZERO,SIZEOF(PAGEZERO),CHR(0)); GOTO 1;
              END;
            IF (POS('.TEXT',FILENAME)<>LENGTH(FILENAME)-4) OR
               (LENGTH(FILENAME)<=4) THEN
              FILENAME:=CONCAT(FILENAME,'.TEXT');
            OPENOLD(THEFILE,FILENAME);
            MSG:='Not present. File? ';
          UNTIL IORESULT=0;
        END;
      
      
      (* Read in the file *)
      
      BLANKCRT(1);
      WRITE('Reading');
      IF BLOCKREAD(THEFILE,PAGEZERO,2)<>2 THEN ERROR('Reading file',FATAL);
      WRITE('.');
      PAGE:=1;
      DONE:=FALSE; OVFLW:=FALSE;
      WITH USERINFO DO
        WHILE NOT (DONE OR OVFLW) DO
          BEGIN
            DONE:= BLOCKREAD(THEFILE,BUFFER,2)=0;
            IF NOT DONE THEN
              BEGIN
                WRITE('.');
                NOTNULS:=SCAN(-1024,<>CHR(0),BUFFER[1023])+1024;
                OVFLW:=NOTNULS+BUFCOUNT>=BUFSIZE-10;
                IF OVFLW THEN NOTNULS:=0;
                MOVELEFT(BUFFER[0],EBUF^[BUFCOUNT],NOTNULS);
                IF PAGE+PAGE=ERRBLK THEN CURSOR:=BUFCOUNT+ERRSYM; (* errblk>0 only *)
                BUFCOUNT:=BUFCOUNT+NOTNULS;
                PAGE:=PAGE+1;
              END;
          END;
      IF IORESULT<>0 THEN ERROR('Disk error',NONFATAL) ELSE
        IF NOT DONE THEN ERROR('Buffer overflow.',NONFATAL);
      1: IF (EBUF^[BUFCOUNT-1]<>CHR(EOL)) OR (BUFCOUNT=1) THEN
        BEGIN
          EBUF^[BUFCOUNT]:=CHR(EOL);
          BUFCOUNT:=BUFCOUNT+1;
        END;
      
      
      (* Initialize everything else! *)
      
      DIRECTION:='>';
      LASTPAT:=1; (* Init to the beginning of the buffer (for equalc) *)
      COPYOK:=FALSE;
      LINE1PTR:=1;
      WITH SYSCOM^.CRTINFO DO
        BEGIN
          ESC:=ORD(ALTMODE);
          ETX:=ORD(EOF);
          BS:=ORD(CHARDEL);
          DEL:=ORD(LINEDEL);
          SCREENWIDTH:=WIDTH-1;
          SCREENHEIGHT:=HEIGHT-1;
          MIDDLE:=(SCREENHEIGHT DIV 2) + 1;
        END;
      SYSCOM^.MISCINFO.NOBREAK := TRUE;
      SDEFINED:=FALSE; TDEFINED:=FALSE; (* No substring or target *)
      WITH PAGEZERO DO
        IF NOT DEFINED THEN 
          BEGIN
            FILLCHAR(BUF,1024,CHR(0));
            TOKDEF:=TRUE; (* Default mode is T(oken *)
            FILLING:=FALSE; AUTOINDENT:=TRUE; RUNOFFCH:='^';
            LMARGIN:=0; PARAMARGIN:=5; RMARGIN:=SCREENWIDTH;
            DEFINED:=TRUE;
          END;
    END(* WITH *);
  
  
  (* Initialize the KIND array for token find *)
  
  FOR CH:=CHR(0) TO CHR(255) DO KIND[CH]:=ORD(CH);  (* Make them all unique *)
  FOR CH:='A' TO 'Z' DO KIND[CH]:=ORD('A');
  FOR CH:='a' TO 'z' DO KIND[CH]:=ORD('A');
  FOR CH:='0' TO '9' DO KIND[CH]:=ORD('A');
  KIND[CHR(EOL)]:=ORD(' '); KIND[CHR(HT)] :=ORD(' ');

END(* INITIALIZE *);


