// p-code interpreter, tracing/debugging functions
// (C) 2013 Frank Seide

#define _CRT_SECURE_NO_WARNINGS 1

#include "pmachine.h"
#include "pframestack.h"
#include "diskio_winrt.h"   // for pdisk.h to compile
#include "pdisk.h"
#include <string>
#include <set>
#include <map>
#include <io.h>
#include <windows.h>    // for OutputDebugString()
#undef max              // pollution from windows.h
#undef min

using namespace std;

void dprintf(const char * format, ...)
{
#ifdef _DEBUG
    va_list args; va_start (args, format);
    char buf[1000];
    vsprintf_s(&buf[0], _countof(buf), format, args);
    OutputDebugStringA(buf);
#endif
}

namespace psystem
{

static int prevstackheight = 0;
static int lineno = 0;
static void * prevIP = NULL;
static const void * prevSEG = NULL;
static const void * prevPROC = NULL;

static bool insttrace = false;
static bool sourcetrace = false;
static bool proctrace = false;       // trace procedure calls and returns

// simplistic tracing
void pmachine::settracefile(const wchar_t * diskpath)
{
    if (tracefile)
        return;
    if (!insttrace && !sourcetrace && !proctrace)
        return;
    wstring p(diskpath);
    p += L"/../trace.txt";  // we trace here
    tracefile = _wfopen(p.c_str(), L"wt");
}

// trace individual instructions
void pmachine::traceinst()
{
    if (lineno == 3030-10)
        sin(1.0);
    if (SEG == prevSEG && PROC == prevPROC && IP == prevIP)   // do not output multiple instances in case of TermRead() retries
        return;
    prevSEG = SEG;
    prevPROC = PROC;
    prevIP = IP;

    ++lineno;
    if (!insttrace || !tracefile)
        return;
    int stackheight = intptr_t(cpustacktop) - intptr_t(PSP);
    fprintf(tracefile, "%d\tS=%d\tP=%d\tIP=%d\top=%d\tH=%d\n",
        lineno,
        SEG->segmentid, PROC->procnum,
        IP - PROC->entryIP(), *IP,
        stackheight - prevstackheight);
    prevstackheight = stackheight;
    // the following fails for some cases, where 205 apparently does not push something
    if (*IP == 205 || *IP == 194) // fake the BP on the PSP stack
        prevstackheight -= 2;
    else if (*IP == 193) // fake the BP on the PSP stack
        prevstackheight += 2;
}

// trace by source-code lines, using compiler-generated LST files ($L option)
struct sourcemap
{
    vector<char> sourcecode;        // as one long string
    vector<size_t> lines;           // offset to each code line
    vector<vector<vector<size_t>>> codeloc; // [segno][procno][relip] -> index into lines[]
    sourcemap(){};                  // (needed for map)
    sourcemap(vector<char> & file)
    {
        sourcecode.swap(file);      // (or use && ?)
        lines.reserve(sourcecode.size() / 100);
        const char * delim = "\r\n";
        // turn it into an array of lines
        for (char * p = strtok(sourcecode.data(), delim); p; p = strtok(NULL, delim))
            lines.push_back(p - sourcecode.data());
        // parse each line
        for (size_t n = 0; n < lines.size(); n++)
        {
            const char * p = lines[n] + sourcecode.data();
            int lineno, segno, procno, nestinglevel, relip;
            // note: if nestinglevel == 'D' then relip contains the variable index (1-based, in words) of that line
            int res = sscanf(p, "%d %d %d:%d %d ", &lineno, &segno, &procno, &nestinglevel, &relip);
            if (res != 5)
                continue;
            if (segno >= (int) codeloc.size())
                codeloc.resize(std::max(segno+1, 2 * (int) codeloc.size()));
            auto & procdict = codeloc[segno];
            if (procno >= (int) procdict.size())
                procdict.resize(std::max(procno+1, 2 * (int) procdict.size()));
            auto & ipdict = procdict[procno];
            while (relip > (int) ipdict.size())         // so now we can look up by rel IP directly
                ipdict.push_back(ipdict.back());        // note: not guaranteed that the exit code is covered, so clip the IP at lookup time
            if (relip == (int) ipdict.size())
                ipdict.push_back(n);                    // line no of this
        }
    }
    int getcodelineindex(size_t segno, size_t procno, size_t relip) const
    {
        if (segno >= codeloc.size())
            return -1;
        auto & procdict = codeloc[segno];
        if (procno >= procdict.size())
            return -1;
        auto & ipdict = procdict[procno];
        if (ipdict.empty())
            return -1;
        if (relip >= ipdict.size())
            relip = ipdict.size()-1;
        return ipdict[relip];
    }
    const char * getcodeline(int index) const
    {
        if (index < 0)
            return NULL;
        else
            return lines[index] + sourcecode.data();
    }
    const char * getcodeline(size_t segno, size_t procno, size_t relip) const
    {
        return getcodeline(getcodelineindex(segno, procno, relip));
    }
};
static map<string,sourcemap> sourcemapsbyfile;          // [#u:t]
static const sourcemap * sourcemaps[64] = { 0 };        // [segno] we use 64 to cover including Apple //e and Apple ///

// filename conventions:
//  - XXX.CODE --> L.XXX.TEXT, where XXX gets cut to 8 characters
//  - SYSTEM.XXX --> L.XXX.TEXT, where XXX gets cut to 8 characters
static string maptolst(string file)
{
    string prefix;
    size_t bspos = file.find_last_of('\\');
    if (bspos != string::npos)
    {
        prefix = file.substr(0, bspos+1);
        file = file.substr(bspos+1);
    }
    // strip [...] type annotation
    size_t ppos = file.find_last_of('[');
    if (ppos != string::npos)
        file = file.substr(0, ppos);
    // strip .CODE or SYSTEM.
    if (file.size() > 5 && file.substr(file.size() - 5) == ".CODE")
        file = file.substr(0, file.size() - 5);
    else if (file.size() > 7 && file.substr(0, 7) == "SYSTEM.")
        file = file.substr(7);
    // now crop to ensure we can safely surround it by L. and .TEXT (we got a total of 15 characters)
    if (file.size() > 8)
        file = file.substr(8);
    // reassemble
    // Note: no need for [...] type annotation since .TEXT is sufficient
    return prefix + "L." + file + ".TEXT";
}

// a new segment was loaded--try to load the source file
void pmachine::tracesegchanged(size_t s)
{
    if (!sourcetrace || !tracefile)
        return;
    auto entry = segtable[s];
    if (entry.codefile == "")           // codefile unknown
    {
        sourcemaps[s] = NULL;
        return;
    }
    // find the sourcemap
    char key[80];
    sprintf (key, "#%d:%s", entry.codeunit, entry.codefile.c_str());
    auto iter = sourcemapsbyfile.find(key);
    if (iter == sourcemapsbyfile.end())
    {
        sourcemaps[s] = NULL;               // assume no entry, update if we have one
        // not loaded: try to find a LST file
        auto codedisk = dynamic_cast<diskperipheral *> (devices[entry.codeunit].get());
        if (codedisk == nullptr) return; // not a disk??
        auto path = codedisk->getdiskpath(entry.codefile);
        if (path.empty())
            return;                     // oops, didn't we just use the file?
        // map to LST file
        auto lst = maptolst(path);
        FILE * f = fopen(lst.c_str(), "rb");
        if (f == NULL)
            return;                     // no LST file
        bool ok = true;
        auto size = (size_t) _filelengthi64(_fileno(f));
        vector<char> buf(size+1, 0);    // pad with a 0 at the end (note: bad_alloc will leak file handle, I don't care for debugging)
        ok = (fread(buf.data(), size, 1, f) == 1);
        fclose(f);
        if (!ok)
            return;                     // failed to read the LST file
        sourcemapsbyfile[key] = sourcemap(buf);
        iter = sourcemapsbyfile.find(key);
    }
    sourcemaps[s] = &iter->second;      // must be there now
}

static set<string> triedtoloadsource;
const char * prevp = NULL;
size_t prevs = 0, prevpr = 0, previp = 0;

void pmachine::tracesource()
{
    if (!sourcetrace || !tracefile)
        return;
    size_t segno = SEG->segmentid;
    size_t procno = PROC->procnum;
    size_t relip = IP - PROC->entryIP();
    if (segno == prevs && procno == prevpr && relip == previp)   // dead loop waiting for character input: skip
        return;
    prevs = segno; prevpr = procno; previp = relip;
    if (sourcemaps[segno] == NULL)
        return;
    const char * p = sourcemaps[segno]->getcodeline(segno, procno, relip);
    if (p == NULL)
        return;
    if (p != prevp)                         // only print each line once
    {
        prevp = p;
        fputc('\n', tracefile);
        fwrite(p, strlen(p), 1, tracefile);
        size_t len = 0;                     // determine the length
        for (size_t k = 0; p[k]; k++)
            if (p[k] != '\t')
                len++;
            else
                len = len / 8 * 8 + 8;
        for (size_t k = len; k < 88; )      // go to some col at the end
        {
            if (k % 8 == 0)
            {
                fputc('\t', tracefile);     // (saving disk access may matter just a little)
                k += 8;
            }
            else
            {
                fputc(' ', tracefile);
                k++;
            }
        }
    }
    else fputc(' ', tracefile);             // concatenate multiple stack traces to the same line
    // append the stack
    fputc('(', tracefile);
    const size_t maxtos = 6;
    auto psp = (const WORD*) PSP;
    size_t stackheight = (const WORD*) cpustacktop - psp;
    for (size_t k = 0; k < maxtos && k < stackheight; k++)
    {
        if (k > 0)
            fputc(',', tracefile);
        fprintf(tracefile, "%d", psp[k]);
    }
    if (stackheight > maxtos)
        fprintf(tracefile, "+%d", stackheight - maxtos);
    fputc(')', tracefile);
    // flush so we see something early
    static int l = 0;
    if (l < 100 || l % 20 == 0)
        fflush(tracefile);
    l++;
}

// called after an update to syscom->IORSLT
void pmachine::traceIORSLT()
{
    if (syscom->IORSLT)
        sin(1.0);   // set breakpoint here
}

struct procidentity { size_t s; size_t p; };
stack<procidentity> callstack;
void pmachine::traceenterexitproc(size_t s, size_t p, bool entering)
{
    procidentity pr = { s, p };
    size_t depth = callstack.size();
    if (!proctrace || !tracefile)
        return;
    for (size_t k = entering ? 0 : 1; k < depth; k++)
        fputc(' ', tracefile);
    if (entering)
    {
        fprintf(tracefile, "CALL (%d, %d)\n", pr.s, pr.p);
        callstack.push(pr);
    }
    else
    {
        auto top = callstack.top();
        fprintf(tracefile, "EXIT (%d, %d)\n", pr.s, pr.p);
        if (top.s != pr.s || top.p != pr.p)
        {
            fprintf(tracefile, "?GOT (%d, %d)\n", top.s, top.p);
        }
        callstack.pop();
    }
    fflush(tracefile);
}

};
