SEGMENT FUNCTION OUT: BOOLEAN;
LABEL 1,2;
VAR
  SAVE: PTRTYPE;
  I: INTEGER;
  BUF: PACKED ARRAY [0..1023] OF CHAR;
  FN: STRING;
BEGIN
  OUT:=FALSE;
  REPEAT
    PROMPTLINE:=' Quit:';
    PROMPT; BLANKCRT(1);
    WRITELN('     U(pdate the workfile and leave');
    WRITELN('     E(xit without updating');
    WRITELN('     R(eturn to the editor without updating');
    WRITELN('     W(rite to a file name and return');
    CH:=UCLC(GETCH);
  UNTIL CH IN ['U','E','R','W'];
  IF CH='R' THEN GOTO 2;
  IF CH='E' THEN BEGIN OUT:=TRUE; BLANKCRT(1); GOTO 2 END;
  CLOSE(THEFILE);
  IF CH='W' THEN
    BEGIN
      SAVE:=CURSOR;
      BLANKCRT(1);
      WRITE('Name of output file (<cr> to return)  -->');
      READLN(FN);
      IF LENGTH(FN)=0 THEN GOTO 2;
      IF (POS('.TEXT',FN)<>LENGTH(FN)-4) OR (LENGTH(FN)<=4) THEN
        FN:=CONCAT(FN,'.TEXT');
    END
  ELSE
    FN:='*SYSTEM.WRK.TEXT';
  BLANKCRT(1);
  WRITE('Writing');
  OPENNEW(THEFILE,FN);
  IF BLOCKWRITE(THEFILE,PAGEZERO,2) <> 2 THEN GOTO 1;
  WRITE('.');
  CURSOR:=1;
  WHILE CURSOR < BUFCOUNT-1023 DO
    BEGIN
      I:=SCAN(-1022,=CHR(EOL),EBUF^[CURSOR+1022]);
      MOVELEFT(EBUF^[CURSOR],BUF,1023+I);
      FILLCHAR(BUF[1023+I],ABS(I)+1,CHR(0));
      IF BLOCKWRITE(THEFILE,BUF,2) <> 2 THEN GOTO 1;
      CURSOR:=CURSOR+1023+I;
      WRITE('.');
    END;
  IF CURSOR<BUFCOUNT THEN
    BEGIN
      FILLCHAR(BUF,SIZEOF(BUF),CHR(0));
      MOVELEFT(EBUF^[CURSOR],BUF,BUFCOUNT-CURSOR);
      IF BLOCKWRITE(THEFILE,BUF,2) <> 2 THEN GOTO 1; WRITE('.')
    END;
  CLOSE(THEFILE,LOCK);
  WRITELN;
  WRITELN('Your file is ',BUFCOUNT,' bytes long.');
  IF CH='U' THEN
    WITH USERINFO DO
      BEGIN
        SYMVID:=SYVID; SYMTID:='SYSTEM.WRK.TEXT'; GOTSYM:=TRUE;
        OPENOLD(THEFILE,'*SYSTEM.WRK.CODE'); CLOSE(THEFILE,PURGE);
        GOTCODE:=FALSE; CODETID:=''; OUT:=TRUE;
      END
    ELSE
      BEGIN
        WRITE('Do you want to E(xit from or R(eturn to the editor? ');
        REPEAT CH:=UCLC(GETCH) UNTIL CH IN ['E','R'];
        OUT:= CH='E';
        CURSOR:=SAVE; (* QW returns to the editor *)
      END;
  GOTO 2; (* SORRY ABOUT THAT EDSGER *)
1: ERROR('Writing out the file',NONFATAL);
2:END;



