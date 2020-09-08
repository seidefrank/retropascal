// p-code interpreter --code to handle snapshotting
// (C) 2013 Frank Seide

// current issues with snapshots for Undo:
//  - snapshot makes full memory copy before dup check
//    Could do dup check first, will save a full write-read back cycle for unchanged pages (~140 KB)
//  - snapshots also for cursor movement
//    I need to know which keystrokes are content-changing.
//  - snapshot is done after key input
//    If done right before, there is no latency after the keystroke
//    Also can check first for available keystroke, and if so, skip snapshot.
//  - visible lag when typing very fast
//    Could make snapshot interruptible?
//  - lots of small blocks
//    Collating them into structs should make it faster.

#define _CRT_SECURE_NO_WARNINGS 1

#include "pmachine.h"
#include "pperipherals.h"
#include "psnapshot.h"
#include <memory>
#include <vector>
#include <map>

using namespace std;

namespace psystem
{

// high-level management of snapshots for suspend/resume
std::vector<unsigned char> pmachine::savemachinestate()
{
    unique_ptr<psnapshot> s(new psnapshot(nullptr, rambottom, ramtop));
    suspendresume(s.get());
    return s->serialize();
}

void pmachine::restoremachinestate(const std::vector<unsigned char> & snapshotimage)
{
    unique_ptr<psnapshot> s(new psnapshot (snapshotimage, rambottom, ramtop));
    suspendresume(s.get());
}


// high-level management of snapshot stack (for Undo)
bool pmachine::haspastsnapshot() { return !pastsnapshotstack.empty(); }
bool pmachine::hasfuturesnapshot() { return !futuresnapshotstack.empty(); }

void pmachine::clearsnapshots()
{
    pastsnapshotstack.clear(); futuresnapshotstack.clear();
    dprintf("clearsnapshot: all cleared\n");
}

void pmachine::pushsnapshot()
{
    shared_ptr<psnapshot> s(new psnapshot(haspastsnapshot() ? pastsnapshotstack.back().get() : nullptr, rambottom, ramtop));
    suspendresume(s.get());                 // 'new' has set it to 'creating' mode (!resuming)
    pastsnapshotstack.push_back(s);
    futuresnapshotstack.clear();            // and clear the future
}

bool pmachine::poppastsnapshot()
{
    if (!haspastsnapshot())
        return false;
    auto s = pastsnapshotstack.back();
    pastsnapshotstack.pop_back();
    futuresnapshotstack.push_back(s);       // remember it so that we can Redo
    suspendresume(s.get());
    return true;
}

bool pmachine::popfuturesnapshot()
{
    if (!hasfuturesnapshot())
        return false;
    auto s = futuresnapshotstack.back();
    futuresnapshotstack.pop_back();
    pastsnapshotstack.push_back(s);       // remember it so that we can Redo
    suspendresume(s.get());
    return true;
}

// snapshotting of p-machine itself (called from devices[0]->suspendresume()/resumefrom())
// "All hope abandon ye who enter here -Dante" [from LINKER source; seems fitting]

void pmemory::suspendresume(psnapshot * s)
{
    s->ramblocks("words", words, sizeof(words), 1024);
    s->ramblocks("eval", evalstack, sizeof(evalstack), 1024);
}
void pcpu::suspendresume(psnapshot * s)
{
    s->setprefix("pcpu:", -1);
    s->ptr("IP", IP);    // ugh! We have no access to pmemory here...
    s->ptr("PSP", PSP);
}
void psegtable::suspendresume(psnapshot * s)
{
    for (size_t k = 0; k < _countof(segtable); k++)
    {
        s->setprefix("S%d:", k);
        s->scalar("isSYSTEM", segtable[k].isSYSTEM);
        s->scalar("refcount", segtable[k].refcount);
        if (segtable[k].refcount == 0)
        {
            if (s->resuming)
            {
                segtable[k].segend = nullptr;
                segtable[k].proctable = nullptr;
                segtable[k].variables = nullptr;
                segtable[k].segbase = nullptr;
                //segtable[k].unitemulator = nullptr;
                segtable[k].segname.clear();
                segtable[k].codefile.clear();
                segtable[k].codeunit = 0;
            }
            continue;
        }
        s->ptr("segend", segtable[k].segend);
        s->ptr("proctable", segtable[k].proctable);
        s->ptr("variables", segtable[k].variables);
        s->ptr("segbase", segtable[k].segbase);
        // BUGBUG: cannot handle segtable[k].unitemulator; will support it for Undo only for now i.e. not update after resume from disk
        s->str("segname", segtable[k].segname);
        s->str("codefile", segtable[k].codefile);
        s->scalar("codeunit", segtable[k].codeunit);
    }
}
void pperipherals::suspendresume(psnapshot * s)
{
    for (size_t u = 0; u < _countof(devices); u++)
    {
        s->setprefix("#%d:", u);
        getunit(u)->suspendresume(s);
    }
}

void pmachine::suspendresume(psnapshot * s)
{
    // base classes
    pcpu::suspendresume(s);
    pmemory::suspendresume(s);
    psegtable::suspendresume(s);
    pperipherals::suspendresume(s);
    // execution state
    s->setprefix("exec:", -1);
    s->ptr("NP", NP);    // ugh! We have no access to pmemory here...
    s->ptr("SP", SP);
    s->ptr("SEG", SEG);
    s->ptr("PROC", PROC);
    s->ptr("BP", BP);
    s->ptr("GBP", GBP);
    s->ptr("GBP0", GBP0);
    // system state
    s->setprefix("sys:", -1);
    s->ptr("syscom", syscom);
    s->ptr("SYVID", SYVID);
    s->ptr("DKVID", DKVID);
    s->ptr("thedate", thedate);
    // intercept state
    s->setprefix("intercept:", -1);
    s->scalar("systemproccallstate", systemproccallstate);
    s->scalar("lastcharswrittentoconsole", lastcharswrittentoconsole);
    s->str("promptlineascaptured", promptlineascaptured);
    s->ptr("FREADSTRING_S", FREADSTRING_S);
    s->scalar("completepathnames", completepathnames);
    s->strarray("completionsuffixconstraints", completionsuffixconstraints);
    s->str("currentpathnameprefix", currentpathnameprefix);
    s->scalar("FCLOSE_unittofinalize", FCLOSE_unittofinalize);
    // we don't serialize these
    if (s->resuming)
    {
        //  - tracefile   --keep
        //  - yieldrequest
        yieldrequest = -1;
        //  - pastsnapshotstack, futuresnapshotstack  --obviously cannot touch them
        //  - lastjumpedtoIP, lastjumpedtoiterations  --just clear upon read, will catch up
        lastjumpedtoIP = (CODE*) NILPTR;
        lastjumpedtoiterations = 0;
        //  - libc current rand() state  --RANDOM numbers will be a little more random
    }
    // done
    s->finalize();
}

};
