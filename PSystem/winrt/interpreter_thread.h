// p-code interpreter wrapper that runs it in a parallel thread
// Copyright (C) 2013 Frank Seide

#pragma once

#include "pmachine.h"           // my interpreter
//#include "ucsdpsys_vm/main.h"   // Klebsch/Miller interpreter
//#include "lib/memory.h"         // for suspend/resume functions
//#include "ucsdpsys_vm/term.h"
//#include "lib/diskio.h"
#include <vector>
#include <memory>

using namespace Windows::System::Threading;

namespace ReTro_Pascal   // TODO: change to psystem
{

public delegate void SuspendSaveStateCallback(const Platform::Array<unsigned char>^ suspendBlob);
public delegate void AbortCallback();
public delegate void HaltCallback(Platform::String^);

class InterpreterThread
{
    std::unique_ptr<psystem::pmachine> pmachine;
    volatile bool suspending;   // set if we are in Suspend(); set by Suspend(), cleared by SuspendContinuation() which must always be run when this flag is set
    volatile bool aborting;     // set by Abort(), cleared by AbortContinuation()
    bool running;               // running (incl. timer pending)? --set only by UI thread
    Windows::UI::Core::CoreDispatcher^ dispatcher;      // dispatcher remembered by constructor
    HaltCallback^ haltCallback;
    void threadproc()
    {
        int yieldms;            // ms to yield; 0 = yield then come back right away; < 0: don't come back
        Platform::String^ haltMessage;    // error in case of an exception flying
        try
        {
            //yieldms = pmachine ? pmachine->run() : (RunPCodeUntilInterrupt() ? 10 : -1);
            yieldms = pmachine->run();
        }
        catch(const std::exception & e)      // unhandled exception during interpreter execution
        {
            haltMessage = ref new Platform::String(std::wstring(e.what(), e.what()+strlen(e.what())).c_str());
            yieldms = -1;  // this will lead to a call to halted() on the UI thread, which will terminate the whole thing at the right place
        }
        catch(Platform::Exception^ ex)      // unhandled exception during interpreter execution
        {
            haltMessage = ex->Message;
            yieldms = -1;  // this will lead to a call to halted() on the UI thread, which will terminate the whole thing at the right place
        }
        catch(...)      // unhandled exception during interpreter execution
        {
            yieldms = -1;  // this will lead to a call to halted() on the UI thread, which will terminate the whole thing at the right place
        }
        if (yieldms >= 0 && !suspending && !aborting)   // is True if we returned due to an interrupt or due to waiting for input (TermRead())
            run(yieldms);                               // this will kick it of once again through the main thread
        else                                            // done: tell UI thread
        {
            bool callagain = yieldms >= 0;
            dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this,callagain,haltMessage]() -> void
            {
                halted(callagain, haltMessage);
            }));
        }
    }
    void run(int yieldms)       // call from UI thread or interpreter thread; interpreter thread is the timer thread
    {
        Windows::Foundation::TimeSpan ts;
        ts.Duration = yieldms ? yieldms * 10000LL : 1;    // in 100 ns units, make it longer than the terminal render thread period
        ThreadPoolTimer::CreateTimer(ref new TimerElapsedHandler([this](ThreadPoolTimer^ timer)
        {
            this->threadproc();
        }), ts);
    }
    void halted(bool callagain/*false if machine exited cleanly in the meantime*/, Platform::String^ haltMessage)   // called on UI thread by threadproc
    {
        running = false;
        auto callback = haltCallback;
        haltCallback = nullptr;                 // no longer running--clear this
        if (suspending)
            SuspendContinuation(callagain);
        else if (aborting)
            AbortContinuation();
        else if (callback)
            callback(haltMessage);
    }
    std::vector<std::string> mountedDisks;      // we keep them here in case of a restore --not used at the moment
public:
    // TODO: there is no value in passing diskPaths as a Platform array instead of a proper STL vector
    InterpreterThread(iterminal & term, const Platform::Array<Platform::String^>^ diskPaths)
        : running(false), suspending(false), aborting(false)
    {
        // get a dispatcher to send callbacks back to main (UI) thread
        dispatcher = Windows::UI::Core::CoreWindow::GetForCurrentThread()->Dispatcher;  // keep UI thread so we can marshal function calls back to it
#if 1   // new interpreter
        pmachine.reset(new psystem::pmachine(term));
        for (size_t i = 0; i < diskPaths->Length; i++)
            pmachine->mount(diskPaths[i]->Data());
#else
        std::vector<char> modes;
        for (size_t i = 0; i < diskPaths->Length; i++)
        {
            auto dp = diskPaths[i];
            mountedDisks.push_back(std::string (dp->Begin(), dp->End()));
            modes.push_back('f');       // TODO: will apply to volumes but not emulated disk files; we will move to files anyway
        }
        pmachine = nullptr;     // use C program
        std::vector<const char*> cpaths;    // convert to array of const char* for consumption by C code
        for (size_t i = 0; i < mountedDisks.size(); i++)
            cpaths.push_back(mountedDisks[i].c_str());
        InitPCode(mountedDisks.size(), modes.data(), cpaths.data(), nullptr/*batch input path*/);
#endif
    }
    // resume processing (or begin, after this was constructed)
    // TODO: should we have a callback to tell when it shut down?
    void Resume(int waitms, HaltCallback^ callback)       // call from foreground thread
    {
        suspending = false;
        running = true;
        haltCallback = callback;
        try { run(waitms); }
        catch(...) { running = false; throw; }
    }
    // suspend the p-code CPU
    // Will call the callback with the suspend package that needs to be written to disk.
    // while a blocking operation like reading a key just blocks on the thread (but must be written to respond to the interrupt request as well)
    void Suspend(SuspendSaveStateCallback^ callback)      // call from foreground thread
    {
        if (!running)   // not running: nothing to suspend
            callback(nullptr);
        else try
        {
            suspending = true;
            suspendContinuationParam_callback = callback;
            //if (pmachine) pmachine->yield(0); else InterruptPCode();
            pmachine->yield(0);
            // we will come back in SuspendContinuation()
        }
        catch(...) { suspending = false; suspendContinuationParam_callback = nullptr; throw; }
    }
private:
#if 0
    // suspend packages
    class SerializedDataPackage : public std::vector<unsigned char>
    {
        size_t readpos;
        unsigned char next() { return operator[](readpos++); }  // get next char
        void * next(size_t num) { void * cur = &operator[](readpos); readpos += num; if (readpos > size()) throw std::bad_exception("malformed file, unexpected end of file"); return cur; }
    public:
        // creation
        SerializedDataPackage(size_t alloc) { reserve(alloc); }
        void push_bytes(const void * p, size_t n)
        {
            insert(end(), (const unsigned char *) p, (const unsigned char *) p + n);
        }
        void push_block(const void * p, int n)
        {
            if (n < 0) throw std::logic_error("createSuspendPackage: buffer too small");
            insert(end(), (const unsigned char *) &n, (const unsigned char *) &n + sizeof(n));   // length
            push_bytes(p, n);
        }
        void push_tag(const char * tag)
        {
            push_bytes(tag, strlen(tag));
        }
        // restoration
        SerializedDataPackage(const unsigned char * buf, size_t size) : readpos(0) { insert(begin(), buf,buf + size); }
        void get_bytes(void * p, size_t n)
        {
            memcpy(p, next(n), n);
        }
        void get_block(void *& p, size_t & n)
        {
            int nint;
            get_bytes(&nint, sizeof(nint));
            p = next(nint);
            n = nint;
        }
        void get_tag(const char * tag)
        {
            for (size_t k = 0; tag[k]; k++)
                if (next() != tag[k])
                    throw std::bad_exception("malformed file, bad tag");
        }
    };
#endif
    std::vector<unsigned char> createSuspendPackage()
    {
        return pmachine->savemachinestate();
#if 0
        SerializedDataPackage pkg(140000);       // 140k allocation
        unsigned char buf[128*1024];    // buffer for getting data from p-vm (C interface)
        // get CPU state
        // TODO: need to implement in new engine
        buf;
        pkg.push_tag("CPUSTATE");
        pkg.push_block(buf, SuspendCPU(buf, _countof(buf)));
        pkg.push_tag("VMMEMORY");
        pkg.push_block(buf, SuspendRAM(buf, _countof(buf)));
        pkg.push_tag("TERMINAL");
        pkg.push_block(buf, SuspendTerminal(buf, _countof(buf)));
        pkg.push_tag("DISKDIRS");
        pkg.push_block(buf, SuspendDisks(buf, _countof(buf)));
        pkg.push_tag("END.");
        return pkg;
#endif
    }
    void restoreFromPackage(const std::vector<unsigned char> & pkg)
    {
        pmachine->restoremachinestate(pkg);
#if 0
        void * p;
        size_t n;
        p;n;
        pkg.get_tag("CPUSTATE");
        pkg.get_block(p, n);
        if (RestoreCPU(p, n) < 0) throw std::bad_exception("malformed file, invalid CPU state");
        pkg.get_tag("VMMEMORY");
        pkg.get_block(p, n);
        if (RestoreRAM(p, n) < 0) throw std::bad_exception("malformed file, invalid RAM state");
        pkg.get_tag("TERMINAL");
        pkg.get_block(p, n);
        if (RestoreTerminal(p, n) < 0) throw std::bad_exception("malformed file, invalid terminal state");
        pkg.get_tag("DISKDIRS");
        pkg.get_block(p, n);
        if (RestoreDisks(p, n) < 0) throw std::bad_exception("malformed file, invalid disk state");
        pkg.get_tag("END.");
#endif
    }
    SuspendSaveStateCallback^ suspendContinuationParam_callback; // remembered by Suspend() for us
    // we will get here after call to Suspend() once code is done running
    void SuspendContinuation(bool callagain)
    {
        // get back the parameters
        auto callback = suspendContinuationParam_callback;
        suspendContinuationParam_callback = nullptr;    // clear it first, in case the callback throws an exception
        // create the suspend package (CPU and VM state)
        if (!callagain)
        {
            suspending = false;                         // done reading it out, we can run it again now it we want
            callback(nullptr);                          // VM not running; nothing to resume
            return;
        }
        // create the package
        std::vector<unsigned char> suspendPackage;
        try
        {
            suspendPackage = createSuspendPackage();
            suspending = false;                         // done reading it out, we can run it again now it we want
            // and pass it back to UI to save it
            callback(ref new Platform::Array<unsigned char>(suspendPackage.data(), suspendPackage.size()));
        }
        catch(...)
        {
            suspending = false;
            callback(nullptr);                          // TODO: how to 'return' an exception through the callback? Needs more complex Async machinery...
        }
    }
public:
    // restore the machine state from a package
    // Called from UI thread, machine must not be running.
    void Restore(const Platform::Array<unsigned char>^ suspendPackage)  // or throw
    {
        if (running)
            throw std::logic_error("Restore: attempted while VM is running, use Abort() first");
        // restore into VM
#if 0   // for now disabled
        BootLoader();
#else
        restoreFromPackage(std::vector<unsigned char>(suspendPackage->Data, suspendPackage->Data + suspendPackage->Length)); // or throw
#endif
    }
private:
    AbortCallback^ abortParam_callback; // remembered by Abort() for us
public:
    // abort current machine; afer this, we can Resume from a saved state
    // This is really only needed during debugging where we trigger these through buttons while VM is running.
    void Abort(AbortCallback^ callback)
    {
        if (!running)   // not running: nothing to suspend
            callback();
        else try
        {
            aborting = true;
            abortParam_callback = callback;
            //if (pmachine) pmachine->yield(0); else InterruptPCode();
            pmachine->yield(0);
            // we will come back in AbortContinuation()
        }
        catch(...) { aborting = false; abortParam_callback = nullptr; throw; }
    }
private:
    void AbortContinuation()
    {
        auto callback = abortParam_callback;
        suspendContinuationParam_callback = nullptr;    // clear it first, in case the callback throws an exception
        aborting = false;
        callback();
    }
public:
    // boot-load the p-machine
    // At start, UX must call either BootLoader() or Restore()
    void BootLoader()
    {
        if (pmachine)
            pmachine->reset(true);
        // (we don't really support this protocol for Klebsch/Miller machine)
    }
    // restore the machine state from a package
    void SoftReset()
    {
        Abort(ref new ReTro_Pascal::AbortCallback([this]() -> void  // abort current execution
        {
            if (pmachine)
                pmachine->reset(false);
            // we don't care about Klebsch/Miller at this point
            suspending = false;
            running = true;
            run(200);
        }));
    }
    virtual ~InterpreterThread()
    {
        //InterruptPCode();
        pmachine->yield(0);
        // BUGBUG: not enough, I guess
    };
};

}; // namespace
