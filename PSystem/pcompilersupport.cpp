// the p-code interpreter--compiler-support routines IDSEARCH and TREESEARCH
// (C) 2013 Frank Seide

// documentation:
//  - IDSEARCH: http://www.gno.org/pub/apple2/doc/apple/technotes/pasc/tn.pasc.014
//  - TREESEARCH: 

#define _CRT_SECURE_NO_WARNINGS 1

#include "pmachine.h"
#include "pglobals.h"       // system-related definitions such as SYSCOM and execption codes
#include <ctype.h>
#include <string>
#include <map>

using namespace std;

namespace psystem
{

// from the compiler sources
enum SYMBOL { IDENT,COMMA,COLON,SEMICOLON,LPARENT,RPARENT,DOSY,TOSY,
		DOWNTOSY,ENDSY,UNTILSY,OFSY,THENSY,ELSESY,BECOMES,LBRACK,
		RBRACK,ARROW,PERIOD,BEGINSY,IFSY,CASESY,REPEATSY,WHILESY,
		FORSY,WITHSY,GOTOSY,LABELSY,CONSTSY,TYPESY,VARSY,PROCSY,
		FUNCSY,PROGSY,FORWARDSY,INTCONST,REALCONST,STRINGCONST,
		NOTSY,MULOP,ADDOP,RELOP,SETSY,PACKEDSY,ARRAYSY,RECORDSY,
		FILESY,OTHERSY,LONGCONST,USESSY,UNITSY,INTERSY,IMPLESY,
                EXTERNLSY,SEPARATSY };
enum OPERATOR { MUL,RDIV,ANDOP,IDIV,IMOD,PLUS,MINUS,OROP,LTOP,LEOP,
                GEOP,GTOP,NEOP,EQOP,INOP,NOOP };

static bool isidchar(CHAR ch)  // is this char a valid identifier char?
{
    return isalnum(ch) || ch == '_';
}

static const map<string,SYMBOL> & getthemap()
{
    static map<string,SYMBOL> themap;
    if (themap.empty())
    {
        // get the table from the doc, add a # before every symbol, then this:
        // cat a | tr # '\n' | tr -d ' ' | tr -- '-' ' ' | awk '{print "themap[\""$2"\"] = "$1";"}'
        // manually fix up AND, DIV, MOD, RECORD; add SEPARATE; cut to 8 characters
        themap["DO"] = DOSY;
        themap["WITH"] = WITHSY;
        themap["IN"] = RELOP;
        themap["TO"] = TOSY;
        themap["GOTO"] = GOTOSY;
        themap["SET"] = SETSY;
        themap["DOWNTO"] = DOWNTOSY;
        themap["LABEL"] = LABELSY;
        themap["PACKED"] = PACKEDSY;
        themap["END"] = ENDSY;
        themap["CONST"] = CONSTSY;
        themap["ARRAY"] = ARRAYSY;
        themap["UNTIL"] = UNTILSY;
        themap["TYPE"] = TYPESY;
        themap["RECORD"] = RECORDSY;
        themap["OF"] = OFSY;
        themap["VAR"] = VARSY;
        themap["FILE"] = FILESY;
        themap["THEN"] = THENSY;
        themap["PROCEDUR"] = PROCSY;
        themap["USES"] = USESSY;
        themap["ELSE"] = ELSESY;
        themap["FUNCTION"] = FUNCSY;
        themap["UNIT"] = UNITSY;
        themap["BEGIN"] = BEGINSY;
        themap["PROGRAM"] = PROGSY;
        themap["INTERFAC"] = INTERSY;
        themap["IF"] = IFSY;
        themap["SEGMENT"] = PROGSY;
        themap["IMPLEMEN"] = IMPLESY;
        themap["CASE"] = CASESY;
        themap["FORWARD"] = FORWARDSY;
        themap["EXTERNAL"] = EXTERNLSY;
        themap["REPEAT"] = REPEATSY;
        themap["NOT"] = NOTSY;
        themap["OTHERWIS"] = OTHERSY;
        themap["WHILE"] = WHILESY;
        themap["AND"] = MULOP;
        themap["DIV"] = MULOP;
        themap["MOD"] = MULOP;
        themap["FOR"] = FORSY;
        themap["OR"] = ADDOP;
        themap["SEPARATE"] = SEPARATSY;
    }
    return themap;
}

#ifdef _DEBUG
static const char * idsearch_lastid = NULL; // aid for debugging compiler runs
#endif

// PROCEDURE IDSEARCH (VAR OFFSET:INTEGER; VARBUFFER:BYTESTREAM);
void pmachine::idsearch(idsearchargs & args, const CHAR * bytestream)
{
#ifdef _DEBUG
    idsearch_lastid = (const char *) bytestream + args.offset;
    //if (strncmp(idsearch_lastid, "INCLFILE,", 9) == 0)
    //    sin(1.0);
#endif
    // ID := ScanIdentifier (OFFSET, BUFFER);
    string id;
    for( ; isidchar(bytestream[args.offset]); args.offset++)
    {
        CHAR ch = bytestream[args.offset];
        if (ch == '_')
            continue;               // Miller does this, but I don't know according to what spec
        if (id.size() >= args.alpha.size())
            continue;               // too long
        id.push_back(toupper(ch));  // upper-case it
    }
    for (size_t k = 1; k <= args.alpha.size(); k++)             // copy it back
        args.alpha.at(k) = k-1 < id.size() ? id[k-1] : ' ';     // need to cut to 8 chars and pad with 'n'
    // compiler does this after this call:
    // SYMCURSOR := SYMCURSOR+1; (* NEXT CALL TALKS ABOUT NEXT TOKEN *)
    args.offset--;  // so compensate for it (doc bug: doc did not mention it)

    // SY := LookUpReservedWord (ID);
    auto themap = getthemap();
    auto iter = themap.find(id);
    if (iter == themap.end())
        args.sy = IDENT;            // not a keyword
    else
        args.sy = iter->second;

    // OP := LookUpOperator (ID);
    args.op = NOOP;
    if (args.sy == MULOP)
    {
        if (id == "AND") args.op = ANDOP;
        else if (id == "DIV") args.op = IDIV;
        else args.op = IMOD;
    }
    else if (args.sy == RELOP)
        args.op = INOP;
    else if (args.sy == ADDOP)
        args.op = OROP;
}

// FUNCTION TREESEARCH (ROOTPTR : ^NODE; VAR NODEPTR : ^NODE; NAMEID : PACKED ARRAY [1..8] OF CHAR) :INTEGER;
int pmachine::treesearch (PTR<treesearchnode> root, PTR<treesearchnode> & node, ALPHA & nameid)
{
    for (; ; )
    {
        node = root;
        const treesearchnode * nodep = ptr(node);
        int res = nameid.compare(nodep->name);
        if (res < 0)
        {
            root = nodep->left;
            if (root.offset == 0)
                return -1;      // new node goes into node->left
        }
        else if (res > 0)
        {
            root = nodep->right;
            if (root.offset == 0)
                return 1;       // new node goes into node->right
        }
        else
            return 0;
    }
}

};
