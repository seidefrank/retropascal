// p-code interpreter, Apple unit emulation
// (C) 2013 Frank Seide

#define _CRT_SECURE_NO_WARNINGS 1

#include "pmachine.h"
#include "pglobals.h"       // system-related definitions such as SYSCOM and execption codes
#include "pturtlegraphics.h"
#include "plongint.h"
#include "pterminal.h"
#include <time.h>           // for time() for srand()

using namespace std;

int term_PADDLE(size_t k);
bool term_BUTTON(size_t k);

namespace psystem
{

// ---------------------------------------------------------------------------
// TURTLEGRAPHICS
// ---------------------------------------------------------------------------

extern TURTLEGRAPHICS * toTURTLEGRAPHICS(pperipheral*);

bool pmachine::emulateturtlegraphics(size_t procno)
{
    int x, y, l, r, t, b;
    auto g = toTURTLEGRAPHICS(devices[3].get());    // pointer into pgraphicsperipheral
    switch (procno)
    {
    case pnTURTLEGRAPHICSLOADED:
        // p-code intercept
        // Apple Pascal Compiler uses segment #20, which is TURTLEGRAPHICS, as a local segment proc FINISHUP.
        // UNIT emulation currently do not handle this case, and thus we end up here.
        if (incodefile("SYSTEM.COMPILER"))
            return false;           // in this case do not emulate anything, obviously
        // end p-code intercept
        g->LOADED();
        break;
    case pnINITTURTLE: g->INITTURTLE(); break;
    case pnTURN:        // (RELANGLE: INTEGER);
        g->TURN(popi());
        break;
    case pnTURNTO:      // (ANGLE: INTEGER);
        g->TURNTO(popi());
        break;
    case pnMOVE:        // (RELDISTANCE: INTEGER);
        g->MOVE(popi());
        break;
    case pnMOVETO:      // (X, Y: INTEGER);
        y = popi();
        x = popi();
        g->MOVETO(x,y);
        break;
    case pnPENCOLOR:    // (PCOLOR: TGCOLOR);
        g->PENCOLOR((TGCOLOR)popw());
        break;
    case pnTEXTMODE: g->TEXTMODE(); break;
    case pnGRAFMODE: g->GRAFMODE(); break;
    case pnFILLSCREEN: g->FILLSCREEN((TGCOLOR)popw()); break;
    case pnVIEWPORT:
        t = popi();
        b = popi();
        r = popi();
        l = popi();
        g->VIEWPORT(l, r, b, t);
        break;
    case pnTURTLEX:    // : INTEGER;
        popretspace();
        //push(g->TURTLEX()); // uncomment to force STKOVR in GRAFDEMO, for testing error handling
        pushi(g->TURTLEX());
        break;
    case pnTURTLEY:    // : INTEGER;
        popretspace();
        pushi(g->TURTLEY());
        break;
    case pnTURTLEANG:  // : INTEGER;
        popretspace();
        pushi(g->TURTLEANG());
        break;
    case pnSCREENBIT:  // (X, Y: INTEGER): BOOLEAN;
        y = popi();
        x = popi();
        push(g->SCREENBIT(x,y));
        break;
    case pnDRAWBLOCK:  // (VAR SOURCE:INTEGER{TODO: should be SOURCE:^INTEGER}; ROWSIZE, XSKIP, YSKIP, WIDTH, HEIGHT, XSCREEN, YSCREEN, MODE: INTEGER);
        {
            int MODE = popi();
            int YSCREEN = popi();
            int XSCREEN = popi();
            int HEIGHT = popi();
            int WIDTH = popi();
            int YSKIP = popi();
            int XSKIP = popi();
            int ROWSIZE = popi();
            auto SOURCE = popptr<BYTE>();
            g->DRAWBLOCK(SOURCE, ROWSIZE, XSKIP, YSKIP, WIDTH, HEIGHT, XSCREEN, YSCREEN, MODE);
        }
        break;
    case pnWCHAR:      // (CH: CHAR);
        g->WCHAR(popi());
        break;
    case pnWSTRING:    // (S: STRING);
        g->WSTRING(*ptr(pop<PTR<pstring>>()));
        break;
    case pnCHARTYPE:   // (MODE: INTEGER);
        g->CHARTYPE(popi());
        break;
    default:
        throw xeqerror("unknown TURTLEGRAPHICS function", NOTIMP);
    }
    return true;
}

// ---------------------------------------------------------------------------
// APPLESTUFF
// ---------------------------------------------------------------------------

enum APPLESTUFFprocs
{
    pnAPPLESTUFFLOADED = 1,
    pnPADDLE = 2,   // (SELECT: INTEGER): INTEGER;
    pnBUTTON,       // (SELECT: INTEGER): BOOLEAN;
    pnTTLOUT,       // (SELECT: INTEGER; DATA: BOOLEAN);
    pnKEYPRESS = 5, // : BOOLEAN;
    pnRANDOM = 6,   // : INTEGER;
    pnRANDOMIZE = 7,//
    pnNOTE          //(PITCH,DURATION: INTEGER);
};

bool pmachine::emulateapplestuff(size_t procno)
{
    switch (procno)
    {
    case pnAPPLESTUFFLOADED: break; // called when UNIT is loaded, it seems
        // TODO: intercept busy polling loops
        //  - KEYPRESS, BUTTON, PADDLE: maybe intercept every 1000th time with a yield of 10 ms? Should not be called in inner loops
    case pnPADDLE:
        popretspace();      // TODO: is this correct, to pop two?
        pushi(term_PADDLE(popw() % 4));
        beingpolled();
        break;
    case pnBUTTON:
        popretspace();
        push(term_BUTTON(popw() % 4));
        beingpolled();
        break;
    case pnTTLOUT:
        pop<bool>(); popi();    // we cannot do much with this...
        break;
    case pnKEYPRESS:
        popretspace();
        WORD statres;
        devices[2]->stat(statres);
        push(statres);
        beingpolled();
        break;
    case pnRANDOM: popretspace(); pushi(rand()); break;   // note: RAND_MAX = 32767, same as specified range for RANDOM function
#ifdef _DEBUG
    case pnRANDOMIZE: srand(0); break;  // have repeatable behaviour in when debugging
#else
    case pnRANDOMIZE: srand((unsigned)time(NULL)); break;
#endif
    case pnNOTE:
        {
            // TODO: we can use the paudioperipheral now, if we ever go back to Apple-built binaries
            int dur = popi();
            int pitch = popi();
            // TODO: and now?
            yield(dur);             // delay
        }
        break;
    default:
        throw xeqerror("unknown APPLESTUFF function", NOTIMP);
    }
    return true;
}

#if 0
// ---------------------------------------------------------------------------
// TRANSCEND
// No need to emulate, p=code version works just fine.
// ---------------------------------------------------------------------------

enum TRANSCENDprocs
{
    pnTRANSCENDLOADED = 1,
    pnSIN,      // (X:REAL):REAL;
    pnCOS,      // (X:REAL):REAL;
    pnEXP,      // (X:REAL):REAL;
    pnATAN,     // (X:REAL):REAL;
    pnLN,       // (X:REAL):REAL;
    pnLOG,      // (X:REAL):REAL;
    pnSQRT      // (X:REAL):REAL;
};

bool pmachine::emulatetranscend(size_t procno)
{
    switch (procno)
    {
    case pnTRANSCENDLOADED: break; // called when UNIT is loaded, it seems
    case pnSIN:
        popretspace();
        push(sinf(popr()));
        break;
    case pnCOS:
        popretspace();
        push(cosf(popr()));
        break;
    case pnEXP:
        popretspace();
        push(expf(popr()));
        break;
    case pnATAN:
        popretspace();
        {
            float x = 1.0;
            float y= atanf(x);
            float z = y;
        }
        push(atanf(popr()));
        break;
    case pnLN:
        popretspace();
        push(log(popr()));
        break;
    case pnLOG:
        popretspace();
        push(log10f(popr()));
        break;
    case pnSQRT:
        popretspace();
        push(sqrtf(popr()));
        break;
    default:
        throw xeqerror("unknown TRANSCEND function", NOTIMP);
    }
    return true;
}
#endif

// ---------------------------------------------------------------------------
// LONGINTINTRINSICS
// ---------------------------------------------------------------------------

/*
    TYPE DECMAX = INTEGER[36];
           STUNT = RECORD CASE INTEGER OF
                     2:(W2:INTEGER[4]);
                     3:(W3:INTEGER[8]);
                     4:(W4:INTEGER[12]);
                     5:(W5:INTEGER[16]);
                     6:(W6:INTEGER[20]);
                     7:(W7:INTEGER[24]);
                     8:(W8:INTEGER[28]);
                     9:(W9:INTEGER[32]);
                     10:(W10:INTEGER[36])
                   END;
    PROCEDURE FREADDEC(VAR F: FIB; VAR D: STUNT; L: INTEGER);
    PROCEDURE FWRITEDEC(VAR F: FIB; D: DECMAX; RLENG: INTEGER);
    // the intrinsic operators seem not declared yet present
 */

enum LONGINTINTRINSICSprocs
{
    pnLONGINTINTRINSICSLOADED = 1,
    pnFREADDEC,     // (VAR F: FIB; VAR D: STUNT; L: INTEGER);
    pnFWRITEDEC,    // (VAR F: FIB; D: DECMAX; RLENG: INTEGER);
    pnINTRINSICOP   // operation
};

bool pmachine::emulatelongintintrinsics(size_t procno)
{
    switch (procno)
    {
    case pnLONGINTINTRINSICSLOADED: break;
    case pnFREADDEC:
    case pnFWRITEDEC:
        return false;       // means execute in available p-code
    case pnINTRINSICOP:
        decops();           // we treat this like a special proc, which is handled back in pmachine.cpp
        break;
    default:
        throw xeqerror("unknown LONGINTINTRINSICS function", NOTIMP);
    }
    return true;
}

// (30, 4) = addition

};
