// p-code interpreter, TURTLEGRAPHICS emulator interface
// (C) 2013 Frank Seide

#pragma once

#include "ptypes.h"

namespace psystem
{

enum TGCOLOR
{
    TGCOLOR_NONE, TGCOLOR_WHITE, TGCOLOR_BLACK, TGCOLOR_REVERSE,
    // Apple:
    TGCOLOR_RADAR, TGCOLOR_BLACK1, TGCOLOR_GREEN, TGCOLOR_VIOLET,
    TGCOLOR_WHITE1, TGCOLOR_BLACK2, TGCOLOR_ORANGE, TGCOLOR_BLUE, TGCOLOR_WHITE2,
    // our own:
    TGCOLOR_FILLSCREEN_HOLDOFF = 0x1000,
    TGCOLOR_FILLSCREEN_HOLDON = 0x1001,
    // TODO: implement these:
    TGCOLOR_OUTLINEMODE_OFF = 0x2000,
    TGCOLOR_OUTLINEMODE_ON = 0x2001,
    TGCOLOR_TEXT_SEE_THROUGH = 0x4000,
};

struct/*interface*/ TURTLEGRAPHICS
{
    virtual void LOADED() = 0;
    virtual void INITTURTLE() = 0;
    virtual void TURN (int RELANGLE) = 0;
    virtual void TURNTO(int ANGLE) = 0;
    virtual void MOVE(int RELDISTANCE) = 0;
    virtual void MOVETO(int X, int Y) = 0;
    virtual void PENCOLOR(TGCOLOR PCOLOR) = 0;
    virtual void TEXTMODE() = 0;
    virtual void GRAFMODE() = 0;
    virtual void FILLSCREEN(TGCOLOR FILLCOLOR) = 0;
    virtual void VIEWPORT(int LEFT, int RIGHT, int BOTTOM, int TOP) = 0;
    virtual int TURTLEX() = 0;
    virtual int TURTLEY() = 0;
    virtual int TURTLEANG() = 0;
    virtual bool SCREENBIT(int X, int Y) = 0;
    virtual void DRAWBLOCK(const void * SOURCE, int ROWSIZE/*bytes per row; even in sample*/, int XSKIP, int YSKIP, int WIDTH, int HEIGHT, int XSCREEN, int YSCREEN, int MODE) = 0;
    virtual void WCHAR(int ch) = 0;
    virtual void WSTRING(const pstring & S) = 0;
    virtual void CHARTYPE(int MODE) = 0;
    virtual int SYNCFRAME() = 0;
};

/*
PROCEDURE INITTURTLE;
PROCEDURE TURN(ANGLE: INTEGER);
PROCEDURE TURNTO(ANGLE: INTEGER);
PROCEDURE MOVE(DIST: INTEGER);
PROCEDURE MOVETO(X,Y: INTEGER);
PROCEDURE PENCOLOR(PENMODE: SCREENCOLOR);
PROCEDURE TEXTMODE;
PROCEDURE GRAFMODE;
PROCEDURE FILLSCREEN(FILLCOLOR: SCREENCOLOR);
PROCEDURE VIEWPORT(LEFT,RIGHT,BOTTOM,TOP: INTEGER);
FUNCTION  TURTLEX: INTEGER;
FUNCTION  TURTLEY: INTEGER;
FUNCTION  TURTLEANG: INTEGER;
FUNCTION  SCREENBIT(X,Y: INTEGER): BOOLEAN;
PROCEDURE DRAWBLOCK(VAR SOURCE; ROWSIZE,XSKIP,YSKIP,WIDTH,HEIGHT,XSCREEN,YSCREEN,MODE: INTEGER);
PROCEDURE WCHAR(CH: CHAR);
PROCEDURE WSTRING(S: STRING);
PROCEDURE CHARTYPE(MODE: INTEGER);
 */
// proc nos; we also use them internally
enum TURTLEGRAPHICSprocs
{
    /*
procno: args
3: 1, an angle (rel? abs?)
5: 1+ (val = 27)
6: (2,42)
7: 1 (val = 1)
    */
    pnTURTLEGRAPHICSLOADED = 1,
    pnINITTURTLE,
    pnTURN,       // (RELANGLE: INTEGER);
    pnTURNTO,     // (ANGLE: INTEGER);
    pnMOVE,       // (RELDISTANCE: INTEGER);
    pnMOVETO,     // (X, Y: INTEGER);
    pnPENCOLOR,   // (PCOLOR: TGCOLOR);
    // Apple:
    pnTEXTMODE,
    pnGRAFMODE,
    pnFILLSCREEN, // (FILLCOLOR: TGCOLOR);
    pnVIEWPORT,   // (LEFT, RIGHT, BOTTOM, TOP: INTEGER);
    pnTURTLEX,    // : INTEGER;
    pnTURTLEY,    // : INTEGER;
    pnTURTLEANG,  // : INTEGER;
    pnSCREENBIT,  // (X, Y: INTEGER): BOOLEAN;
    pnDRAWBLOCK,  // (VAR SOURCE:INTEGER{TODO: should be SOURCE:^INTEGER}; ROWSIZE, XSKIP, YSKIP, WIDTH, HEIGHT, XSCREEN, YSCREEN, MODE: INTEGER);
    pnWCHAR,      // (CH: CHAR);
    pnWSTRING,    // (S: STRING);
    pnCHARTYPE,   // (MODE: INTEGER);
    // Retro Pascal additions uses these codes:
    pnSYNCFRAME   // : INTEGER; {ms}
};

};
