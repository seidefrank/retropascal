// interface to a character terminal for use by p-code engine
// (C) 2013 Frank Seide

#pragma once

#include <vector>
#include <string>

class iterminal
{
public:
    enum specialmodes			// special-knowledge hack modes for terminal (which function are we in)
    {
        none = 0,
        // these modes are outside the range that the UNITREAD MODE parameter can specify, since it only has 16 bits
        inShell = 0x80000,
        inEDITOR = 0x400000,
        inFILER = 0x800000,
        // EDITOR:
        syntaxHighlighting = 0x10000,
        emulateHomeEnd = 0x20000,
        emulatePageUpDown = 0x40000,
        // FILER:
        enablePathnameCompletion = 0x1000000,   // pathname completion (we are in FREADSTRING)
        // other:
        cannotResizeTerminal = 0x100000,    // in a program that we know cannot resize the terminal on the fly
        enableSuggestions = 0x200000,       // enable Pascal suggestions (we are in EDITOR INSERTIT)
        // TODO: rename to enablePascalSuggestions
        // Note: avoid conflict with pperipheral::specialmodes values
    };

    // basic terminal functions
    virtual int readchar(specialmodes) = 0;
    virtual void flush() = 0;               // TODO: currently this flushes the inputFIFO; should it?
    virtual void userprogchanged() = 0;     // a new app was started--currently this clears the idsSeenSoFar list

    // support for Undo
    virtual void undoenabled(bool canundo, bool canredo) = 0; // tell UX to enable the Undo/Redo button

    // support for command-bar highlighting
    virtual void setcommandbar(const std::string & commandbar) = 0;
    // support for Pascal suggestions in EDITOR

    // support for file-name completion
    virtual void setcompletioninfo(const std::string & prefix, std::vector<std::string> && completedstrings) = 0;

    // support for suspend/resume (which also implements undo/redo)
    virtual void suspendresume(class psnapshot *) = 0;

    // support for automation
    virtual void pushkeys(const char * keys, size_t numkeys) = 0;

    virtual ~iterminal() { }

    class call_me_again { };     // read() will throw this if no input is available, and thread will suspend for a while
};
