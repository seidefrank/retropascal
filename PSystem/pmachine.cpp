// p-code interpreter
// (C) 2013 Frank Seide

// information sources:
//  - Apple III Pascal Technical Reference Manual. Has the most comprehensive info, including the way to have code in separate segments.
//  - Apple III Tech Notes: http://apple3.org/iiitechnotes.html
//  - UCSD Pascal II.0 Users Manual. Has full I.5 spec in 'implementor's guide' section.
//  - Ken Bowles: http://www.kenbowles.net, bowlesk@att.net
//  - UCSD Pascal museum http://www.threedee.com/jcm/psystem/index.html  --contact the author once published!
//  - Apple Pascal boot screen: http://apple2history.org/history/ah15/
//  - Apple Pascal Language Reference Manual: http://www.scribd.com/doc/232825/Apple-Pascal-Language-Reference-Manual
//    has a quick usage tutorial on page 180
//  - Apple Pascal Operating System Reference Manual
//  - Apple Pascal 1.3 sold here: http://store.syndicomm.com/index.php?main_page=product_info&cPath=2&products_id=32
//  - Apple Pascal: A hands-on approach http://www.scribd.com/doc/232823/Apple-Pascal-A-Handson-Approach
//  - Apple III Pascal intro: http://archive.org/details/Apple_III_Pascal_Update-An_Introduction_to_Version_1.1_of_Apple_Pascal_for_Impro
//  - Call A.P.P.L.E.: http://www.callapple.org/category/programming/pascal/ with link for "News to Share"

// first release:
//  - graphics: force to keep aspect ratio: if started with keyboard, will stay; if without, will shrink and regrow by hiding the keyboard again  ######## toughest
//  - graphics: WCHAR did not honor color mode in TOUCHPONG
//  - MANDELBROT: remove xcolor (use RETROSTUFF)
//  - RETROSTUFF: function to read out touch points and buttons (up to two) #######
//     - then a lib that tracks it and returns events, written in Pascal? Problem: keeping state in units. At least function should enable that.
//  - list ALL standard Pascal functions, and all units, in a file somewhere --PASCALDOC.TEXT  --needs references ########
//  - first-time user *TUTORIAL.TEXT -> *SYSTEM.WRK.TEXT  ########
//  - calculator demo  ######
//  - fix sources that have TABs in non-first position (editor cannot handle that) --maybe untab/tab all sources? TFS won't check in...  ###
//  - fix and include DISASM?  ####
//  - clickable http:// URLs in terminal  --DONE
//  - can we tap on <cr>? and <bs>?  --DONE (test)
//     - rename <bs> to <backspace> and <cr>/<ret>... to <enter>  --DONE
//  - app package still is ReTro not Retro
//  - sources too large to edit--break up into S.XXXn.TEXT, n=1..   --DONE
//  - V(iew command in main shell (a simple MORE)   -> new tool *SYSTEM.VIEWER[CODE] or *FILEVIEWER.CODE (nearly done) connect to V(iew command --DONE
//  - BLDDOC.TEXT --copy PASCALDOC and SYSTEM.WRK.TEXT; or put SYSTEM.WRK.TEXT into SAMPLES?  --DONE
//  - EDITOR:
//     - terminal MUST recognize Find (and also Replace) as cmd bars; otherwise the Find line scrolls off --DONE
//     - also we really need case-insensitive matching :(
//     - intro sources: warn that there are no Undo!  --DONE
//     - intro sources: ship a copy in SOURCES:  --DONE
//     - screws up K(eypress sim files; maybe does not support non-zero headers?  --no repro
//     - help screen too big  --are we really 80 chars wide??
//     - save cursor pos when saving file   --DONE
//     - W(rite to X: -> don't write header, don't keep name for S(ave  --DONE
//     - completion: do not mask '?' when inside a string or comment   --DONE
//     - top row not syntax-highlighted once the cmd bar scrolls off --next version
//  - LONGINT STR function crashes with string overflow (STR(int[8],str))   --bug in CONCAT, wontfix
//  - syntax HL:
//     - do not disable when showing an error message  --i.e. always start from second row?  --DONE
//     - enable in V(iew command  --DONE
//  - file-name completion could remember its respective last 4 entries or so and show them when the list is otherwise empty --next version
//  - YIELDCPU should return actual, for precise sync e.g. music playing; or sync w.r.t. the last call to it?
//     - or add a new function, TICKMS?
//     - also rename this one? WAITMS? SLEEPMS?
//     - SLEEPMS(durinms, fromlastsleep); fromlastsleep: measure time since last return (precise) rather than now
//       We can implement SYNCFRAME with this as well, or share code.
//  - shell: X(ecute can handle TEXT files  --DONE
//  - critical bugs:
//     - restore older SYSTEM.WRK.TEXT (got overwritten)   --DONE
//     - gets into weird states now after a few compile rounds  --no repro
//     - EDITOR H(elp freezes it    --DONE
//     - RAMAVAIL not working in Release, returns 0?  --DONE
//     - suddenly gets Illegal Pathname after a few edits; restart helps   --DONE
//        - K(eypress gets 'Error opening file'  --debug this  --no repro
//     - READLNPATHNAME: document pattern in lib source  --DONE
//     - get Filer to not fail on Y, also change NGETCHAR, then add NY to all BLD scripts at start  --DONE
//     - file dates --DONE; but how about newly written files?
//     - Esc key no longer working for EDITOR suggestions, popup will come up, blocking the use of Esc  --DONE
//     - Esc in file completion hides then reappears but still inserts the Esc in the string..??  --DONE
//     - weird volume loss bugs and IOError #64--report a proper error code here!  --no longer happening?  --no repro
//  - compiler must remember relative line number of source file  --all DONE
//     - keep track of nested lines counted in a separate counter, adjust when popping
//     - just don't offer E(dit if wrong file ... now we can work with an include chain!
//       basically don't offer E(dit if it is not the work file, right?
//     - and flush the input terminal in case of an error to break automation
//  - remove SYSTEM.STARTUP, merge into Get Started or opening text  --DONE
//  - test all RETROSTUFF functions, beautify it, and remove what is broken  --DONE
//  - implement:   --all DONE
//     - gettermsize()  --DONE
//     - NOTE --DONE
//     - SYNCFRAME -- must return ms since last call --DONE
//  - Filer help: "remember, it's 1979!"  --DONE
//  - SYSTEM.SYNTAX not working  --DONE
//  - when to allow Ctrl-V paste? while reading lines and in INSERT mode? how about newlines? --how the hell does it control this now?? --next version
//  - Filer Save command not working, does not find target volume  --all DONE
//     - tries to save :SYSTEM.WRK.TEXT even if specified with different volume
//     - seems to have to do with *target* volume-name length: 7 letters then strips *src* vol name, e.g. :SYSTEM.WRK.TEXT instead of *:SYSTEM.WRK.TEXT
//       Same for Transfer: 'USER:HELLOWORLD.TEXT,samples:abc' -> FOPEN(':HELLOWORLD.TEXT'); also if given in two separate lines
//       Source vol name of 7 is OK. Add write statements to TRANSFER, FROMWHERE and TOWHERE
//       -> may just want to use 6 letters...
//       VOLNAME1, VOLNAME2 means VOLNAME2 precedes VOLNAME1; if VOLNAME2 gets written with a 0-terminator, it will nuke VOLNAME1
//     - need to remove .TEXT if given
//  - delete backwards from first char--hang  --no repro
//  - USER: should be first non-system disk -> PREFIX should be on USER:; but sources that include GLOBALS.TEXT should specify SOURCES: for that... :( --DONE
//  - once typing, mouse pointer should disappear until moved
//    cf. http://blogs.msdn.com/b/devfish/archive/2012/08/02/customcursors-in-windows-8-csharp-metro-applications.aspx
//    Window.Current.CoreWindow.PointerCursor = new Windows.UI.Core.CoreCursor(Windows.UI.Core.CoreCursorType.Hand, 0 );
//    or http://social.msdn.microsoft.com/Forums/windowsapps/en-US/26abcf3e-f97a-4e30-ab67-edee816c8e10/how-do-i-make-cursor-change-to-a-hand-when-mouse-is-over-text
//    just set the TextBlock style with PointerOver
//  - suspend/resume --graphics
//  - can we implement the POINTER function? (which can assign to any PTR type)

// disk content:
//  - RETRO: system disk
//  - SOURCES: sources used to build system disk --too big for editor; what to do? $I S.COMPILE2.TEXT at end etc.
//     - everything in 'bootdisk' and 'units', nothing else
//  - SAMPLES:
//  - USER: default disk for user
//  - TOOLS: ? where do all the system tools go? RETRO:? -> need to add DISASM

// build process:
//  - can we automatize that?? as a Pascal program?

// features to do for first version:
//  - build process:
//     - List files are missing
//     - SYSTEM.WRK.TEXT, SYSTEM.STARTUP etc.
//  - line counting with includes
//  - Edit of include file??, doable?
//  - Understand W(hat command, remembered compiled code!
//  - better UX:
//     - linker: small help text to linker. What it does.
//  - do next:
//     - predictive keyboard/sugg: Esc, =? insertion, prefix inside comment/string, clear kept symbols when reentering EDITOR
//     - Esc/Etx: visual; show only if needed
//     - suspend/resume
//  - critical for complete experience and AppStore submission:
//     - suspend/resume should actually work
//  - critical BUGs we need to fix:
//     - error messages in EDITOR --should we move to compiler??
//     - weird volume loss bugs and IOError #64--report a proper error code here!
//     - Esc key no longer working for EDITOR suggestions, popup will come up, blocking the use of Esc
//     - librarian must work; we can disable comments in the compiler if needed
//     - strangely, FILER refuses to copy to SOURCES:, replaces SRC vol with : always if TGT is SOURCES:--scratch head
//  - keep last input of a certain FREADSTRING calling hierarchy and offer it up
//  - NOTE command
//  - AppBar: paste? screen shot when graphics is visible?
//  - DISASM crashes?
//  - visual feedback on touch
//     - a small transparent circle for up to 2 touch points?
//     - use buttons for X(y and <xy>??
//  - BUG: TAB in sugg box should ignore the special one-character symbols when deciding
//     - and ignore the actual prefix, so we can complete SYS as APPLE0:SYSTEM.WRK.TEXT by TAB
//  - multi-file handling:
//     - EDITOR remembers where it came from, S(ave saves right there no matter what  --done, no?
//     - compiler error:
//        - E(dit should open the right file at the right place (but we cannot pass the filename...)
//          => Quit-Save-Run workflow with include files
//           - problem: cannot set it as the workfile, as we may be compiling a master that includes it
//        - line number of $I files should be correct
//     - COMPILE to: empty input should be $ not SYSTEM.WRK.CODE; maybe use * ?
//  - BUG: $C comment destroys CODE file (or maybe librarian only--not updated yet?)  --done, no?
//  - and still LIBRARIAN sometimes screws up even if no $C??   --done, no?
//  - BUG: can no longer paste a whole sequence of commands--some flushing going on
//  - line counting with includes
//  - Edit of include file??, doable?
//  - Understand W(hat command, remembered compiled code!
//     - seems wicked, better not touch
//  - better UX:
//     - linker: small help text to linker. What it does.
//  - Apple catch-up:
//     - COMPILER:
//        - allow lowercase letters for compiler options--really?
//     - EDITOR:
//        - dirty flag to ask to save changes
//  - ModernMix has a problem with scaling the window; window content will disappear except for the background of the title bar--track that down
//  - Esc/Etx should only be visible if they can be used
//  - where to allow Paste? FREADLN? EDITOR Insert mode? How to automatize OS compilation?
//  - predictive keyboard: know parentheses state to offer keywords
//  - need a way to peek into function definitions or other source code; a MORE program?
//  - once typing, mouse pointer should disappear until moved
//  - Esc and Etc keys: pretty up
//  - touch positioning in EDITOR
//  - touch keyboard appears: http://code.msdn.microsoft.com/windowsapps/Touch-keyboard-sample-43532fda/sourcecode?fileId=51009&pathId=1073073130

// features not in first version:
//  - EDITOR undo...
//  - hierarchical file system, Windows file access
//  - single-step debugger
//  - help screens

// really important TODOs:
//  - EDITOR dirty flag, so it can ask to save changes
//  - add a dirty flag for UI-side changes (currently I invalidate entire terminal; replace that by a single flag)
//  - Esc/Etx pad: show only when there is such key requested; we need another flag that analyzes the prompt string
//  - screen shot, some form of clipboard
//  - GRAFDEMO somehow slow again, after MOVETO single-pixel fix
//  - syntax highlighting is too expensive (slows down scrolling)
//  - touch positioning in EDITOR navigation mode
//  - Settings pane does not come back on <- arrow
//  - BUGBUG: sometimes the on-screen keyboard does not come out--useless! How to fix that?? Button that sets focus on inputTextBox? Or is it my keyboard's problem?
//  - visual feedback on touch-enabled stuff
//  - fix touch events in graphics mode, so that we can do Mandelbrot and 3D demo
//  - my own OS build (test: are GLOBALS the same as for Apple II? E.g. check offset of UNITTABLE)

// only changes to Pascal sources:
//  - only show-stopper bugs discovered, e.g. due to larger RAM
//  - changing the all.x.text files, not the individual ones (we will regroup later)
//  - COMPILER:
//     - source code: avoid two FILEs vars declared at once (compiler bug)
//     - increase procedure limit to Apple III values --TODO: find out
//  - EDITOR:
//     - MAXW=84 -> 224
//     - add Save Under Same Name option like Apple's
//     - copy-from-file: accept FBLOCKIO to return 1 as well (in addition to 2) (DONE:=BLOCKREAD(F,BUFR,2,PAGE+PAGE)<>2; --> > 0)
//     - overflow in overflow check: OVFLW:=NOTNULS+BUFCOUNT>=BUFSIZE-10;  -->  OVFLW:=BUFCOUNT>=BUFSIZE-10-NOTNULS;
//  - patch to allow CLIP: device? or do that in interpreter?
//  - future: FILER: do not blow up on hierarchical path names
//  - FREADSTR: update string length each char (since we monitor it externally for pathname completion)

// desirable features and bug fixes:
//  - change diskio_winrt from WriteSector() to unit interface (writing multiple sectors)
//  - ROAMING: disk (vol 12) limited to 99k, stored in a roam folder instead
//     - limit not working since we duplicate all TEXT files; we'd need to dup elsewhere...
//  - pathname completion: CR on volume name when selecting a file should not accept the volume
//  - test beingpolled() with Apple OS
//  - machine undo for EDITOR
//     - snapshot package: array of pages to shared block array, as to only keep deltas
//     - need to reliably detect regular exit from a program, as well as irregular one
//  - do not run refresh tick function if not visible
//  - why does text disappear of a program when going back? Wrong reset call? rename resetterminal() to updateterminal() again or flushterminal()
//  - add init() and reset() functions to peripheral, which match FINIT and FRESET
//  - source-level debugger
//  - snapshot debugging--set "trace points" which copy the VM, and allow to go backwards (use same machinery as EDITOR)
//     - only makes sense when someone is able to modify or at least inspect variable values, though
//  - touch scroll (TEXTMODE only, when in occluded mode)
//  - font-spacing issue: it is minimally off for Consolas in some sizes; how to measure a string width in WinRT?
//  - EDITOR undo
//  - predictive keyboard
//  - DRAWBLOCK color bitmaps
//  - transparent color to overlay graphics over text screen (problem: positioning)
//  - FILLCOLORs: we can also have a transparent value for use with FILLSCREEN--overlay graphics over text!
//  - an open-file dialogue when typing a path? Or IntelliSense completion?
//     - e.g. to teach users about moving cursor by Enter, Space, P
//  - a copy tool from/to Windows?
//  - screen-shot function for graphics (also for text?)
//  - an online-help pane on the right with the most important info on the current tool; e.g. width of the Settings
//  - a settings dialogue! e.g. for menu and syntax
//  - get that Beep to work
//  - clipboard --> #4:CLIPBOARD.TEXT? shows up whenver there is text in the clipboard, as last file entry (artificially added)
//  - full file access? Sean says the store is in disarray, maybe it can go through
//  - Note: for extended calls into our system, we can use unit #0 (SYSTEM:), which is currently a nullptr
//  - helper keyboard adapts based on INSERT/EXCHANGE mode (shows more)
//  - does EDITOR use mode 8? Any way to detect the code that renders the source?
//  - detect EDITOR Xchange mode
//  - page up/down key emulation should use the min of 'confirmed' and 'actual' terminal height, i.e. scroll less with kbd up
//  - share table from IDSEARCH?
//  - fix the AppBar button colors in the correct way (cf.{StaticResource AppBarItemForegroundThemeBrush} which I changed to "white"); get rid of that white separator line or change it to brown
//  - can we keep the Windows predictive keyboard if we are in EDITOR but not editing Pascal?
//  - better graphics scaling with on-screen kbd; show/hide keyboard with graphics
//  - PADDLE scaling with graphics other than 0..255 range
//  - decint < 3 words not allowed acc. to Apple III manual (3..11 words; fix that)
//  - hide keyboard when graphics is visible, with keyboard icon on AppBar to bring it back up (=set focus on inputTextBox--will that work?)
//     - i.e. graphics Rect will absorb taps, keyboard icon will restore foucs on inputTextBox --TODO: try that
//  - single-step debugger (where compiler listing is available)

// from Lisa Pascal:
//  - ETX = "end of transmission"!
//     - means end of file for character device--there we have it! -> fix clipboard, also check read functions if they check for it
//  - Lisa Pascal:
//     - SCREENSIZE(var x, var y)
//     - FRAMECOUNTER:FRAMES (what is FRAMES?)
//  - Lisa Pascal: Pointer function
//     - assignment-compatible with any pointer; this would need compiler support (and it's a no-op anyway)
//     - does ORD work for pointers?
//     - @ operator returns the address of an object
//  - Lisa Pascal OTHERWISE clause in CASE?
//  - release memory: DISPOSE (not RELEASE)
//  - Lisa Pascal D value for WRITEREAL: D <= 0 means to only write an integer part; we could use -1
//     - (UCSD uses 0 as a default; while Lisa says D can be specified or unspecified)

// from Apple notes on their system 1.2 vs. 1.1:
//  - Pascal System Size is 64K  --use this wording
//  - % prefix: disk of executing program; could be set in ASSOCIATE into a global var
//     - Used for: files during program exec, e.g. OPCODES; chaining SETCHAIN('%XXX'); library name files
//  - Unit numbers 13..20
//  - Ctrl-@ terminates the current program—easy!
//     - Program Interrupted by user (all-caps), wait for space, then reinitialize system
//  - Ctrl-] is what? End of file?
//  - 64 segments for 128k system
//  - Cursor control:
//     - 5,6: make cursor visible, invisible
//     - 15,14: reverse video on, off
//  - UNITSTATUS?? (UNITNUM, PAB{ptr}, CONTROL)
//  - Pascal 1.2 IO: these may be issues with current SYSTEM:
//     - SEEK expands the file if going over at the end (error 8 if no room); PUT already expands
//     - bug: SEEK to between new EOF and EOF at time of opening will not position/refill buffer correctly
//     - bug: SEEK to end then PUT may overwrite file following this one if no room
//  - compiler bugs in 1.1:
//     - Missing FORWARD proc will not wait on ERROR but just scroll on (hard to see)
//     - Compiler released heap from symbol table too soon--? The out of bounds access?
//  - LIBRARY and LIBMAP: does not copy/list INTERFACE text if file > 200 blocks
//  - SYSTEM:
//     - WRITE of an empty string did not clear IORESULT
//     - FWRITEREAL sometimes had a random char in right-most position—they probably do that in assembly though
//     - CHAINSTUFF: SETCHAIN, SETCVAL, GETCVAL, SWAPON, SWAPOFF
//  - 'CODEFILE' is the term
//  - Make Exec -> generates a .TEXT file that can be executed?? SETCHAIN('EXEC/...')
//  - 1200 bytes code; 18000 words max size of local vars
//  - NOTE(1,1) gives a click
//  - FILER:
//     - C(hange can change name of disk... ?

// known BUGBUGs:
//  - TURTLEGRAPHICS: MOVETO does not support XOR mode; DRAWBLOCK misses some modes, too
//  - need my own extended TURTLEGRAPHICS
//  - need a soft reset key? to interrupt current program and return to main?
//  - TODO: xmode for right-arrow during EXCHANGE mode, which should read out what's on the screen, to avoid a ? (editor bug)
//  - graphics throttling still not working, seems to slow down in GRAFDEMO; what am I missing? rendering still too slow?
//  - Apple Pascal knows when an intrinsic lib is missing from SYSTEM.LIBRARY; so maybe we don't need to pre-init them after all --TODO: try this

// design of moving code into separate address space: following Apple III design
//  - LPA and LSA want to push address of something inside the code onto the eval stack
//  - approach:  --rather simple, actually!!
//     - LSA and LPA would copy those objects to function stack (like code is copied) and return those addresses instead
//     - copy objects only once; use a linked list of these objects to search for them
//     - linked list is like a stack, and cleaned up when a stack frame is destroyed (traverse until object above stack frame)

// design of unlimited code space:
//  - requires above LSA/LPA stack
//  - maintain a completely separate code stack, linked in regular Windows RAM
//     - make IP in stack frame relative
//     - need additional level of indirection for seg/proc descriptors, e.g. in the stack frame, using yet another stack

// design comments that should be sorted to the respective pieces of code
//  - screen resizing (incl. user changes font size)
//     - causes of screen resizing:
//        - on-screen keyboard appears/disappears (only vertical resizing of terminal)
//        - user changes font size (resizing of terminal)
//        - app gets resized (resizing of graphics area; if term size is not fixed, then this resizes the terminal)
//     - different situations to consider:
//        - horizontal wraparound assumed vs. not ever desired
//        - vertical scrolling assumed (e.g. EDITOR does in INSERT mode) vs. not ever desired (GOTOXY)
//        - program uses terminal (e.g. COMPILER) vs. absolute screen positions (GOTOXY)
//     - if the screen resizes:
//        - distinguish actual and desired terminal dimensions
//           - desired terminal dimensions = from environment, e.g. font or screen-size changes
//           - actual terminal dimensions
//              - what's in SYSCOM.CRTINFO
//              - algorithm:
//                 - desired dims come from environment, set as a request
//                 - at appropriate times, interpreter polls this and passed it on to both SYSCOM.CRTINFO and terminal as actual
//                    - program change event
//                    - UWRITE/UREAD on CONSOLE:/SYSTERM: while we believe that a program can handle it
//                       - this may also just update the vertical dimensions and keep horizontal (to avoid ugly wraparound)
//        - we make educated guesses on whether the current program can handle it
//           - whitelisted: some are known to handle it, e.g. shell and FILER (TODO: check FILER)
//           - blacklisted: some are known to not handle it (we will try without a blacklist)
//              - ugh: EDITOR has a bug: WIDTH > MAXSW = 84 will cause unchecked buffer overruns (fix in its source code: limit SCREENWIDTH; but Apple EDITOR does not have that); leave to interpreter
//           - unknown state
//        - some tools may require wraparound, some won't--how to decide?
//        - state updated at program change; default: unknown (exception: known programs)
//        - state transitions from 'unknown' to 'known not to handle' when first absolute GOTOXY (except 0,0) in the terminal,then assume they cannot
//           - shell uses FGOTOXY(0,CRTINFO.HEIGHT DIV 3); for opening display --thus, we must whitelist it
//           - (better: once a tool reads out CRTINFO, but there is no way to know)
//        - detecting program change event
//           - call to seg #1, proc #0 --> trigger a program change
//           - detecting going back to shell --> trigger a program change
//     - if the font resizes
//        - font setting always takes immediate effect w.r.t. character display
//           - or give it a few frames delay to give interpreter a change to pick up on it, to have both switch at the same time
//        - it changes the desired (not actual) terminal dimensions, to be picked up soon hopefully
//     - graphics screen:
//        - tools can monitor SYSCOM height/width and redo INITTURTLE/TURTLEX/TURTLEY to get current dimensions
//        - graphics buffer is only ever reallocated by INITTURTLE; it will always assume no on-screen keyboard
//  - touch keyboard compatibility:
//     - keep top text row stable if it looks like a command row
//        - e.g. if it equals globals.PL
//        - ensure cursor position is in visibile range
//        - does that already solve all problems?
//        - this will allow the editor to be used even with vertical scrolling of the rest of the screen
//     - more dynamic sizing of vertical size of terminal
//        - ideally, when on-screen keyboard appears, it should adapt
//           - but when it disappears (or user switches to a real keyboard), it should adapt back
//           - EDITOR simply cannot do that
//              - reads out SYSCOM --> SCREEN.HEIGHT/WIDTH
//              - reads out SYSCOM also at startup --> SCREENHEIGHT/SCREENWIDTH
//        - depends on user font setting but also on-screen keyboard coming up
//        - which programs cache the terminal size? EDITOR does, but e.g how about FILER?
//           - FILER directly reads SYSCOM^.CRTINFO.HEIGHT
//           - what happens if terminal height changes in EDITOR?
//              - screen refresh will scroll unexpectedly
//        - so when to update SYSCOM^.CRTINFO with terminal size?
//  - on-screen keyboard while graphics is visible?
//     - hide keyboard once graphics becomes visible
//     - summon the keyboard by an AppBar menu
//        - since we need to enter key presses sometimes
//        - theoretically, a program could replicate a terminal using graphics; or at least enter a text string
//     - while keyboard is visible, vertical scrolling by touch is enabled
//        - key down only reported if user releases (with a key-up after a fixed amount of time)
//        - or when user drags horizontally
//     - while keyboard is invisible, all touch events are reported like a mouse

// thoughts on debugging:
//  - cmd line tool for P code, so we can compile from a command shell--easy one the interpreter is there

// thoughts on modern world:
//  - we could use REMOTE: for IP connections. REMOTEINIT? gets a UIR pointer--can we encode a URL here?

#define _CRT_SECURE_NO_WARNINGS 1

#include "pmachine.h"
#include "pglobals.h"       // system-related definitions such as SYSCOM and execption codes
#include "pframestack.h"    // frame stack
#include "popcodes.h"       // the names of the op codes
#include "pperipherals.h"   // the hardware devices
#include "pterminal.h"      // for terminal size
#include "diskio_winrt.h"   // for pdisk.h to compile
#include "pdisk.h"          // disk I/O
#include <assert.h>
#include <math.h>
#include <windows.h>
#include <time.h>           // for time() for srand()

using namespace std;

namespace psystem
{

// flush GDIRP if necessary
// Per doc, 5.A.6
// Logic: The GDIRP is a cache on top of the heap. It is only at top of heap.
// If we want to allocate anything with it present, it is flushed.
// This is fully correct as long as the Pascal code never caches the GDIRP and then calls NEW.
// Note: Apple III Technical Reference Manual does not state to copy GDIRP to NP; but that seems a bug.
void pmachine::flushGDIRP()
{
    if (syscom->GDIRP.offset)
    {
        NP = ptr(syscom->GDIRP);
        syscom->GDIRP.offset = 0;
    }
}

// validate p-code
// If 'cond' is not true, it indicates locally incorrect p-code.
// Such error is typically not caused by memory corruption except the code itself was overwritten.
__forceinline void pmachine::validatepcode(bool cond) const
{
#ifdef _DEBUG
    if (!cond)
        throwfatalerror(invalidpcode);
#endif
}

// validate p-code logic
// If 'cond' is not true, it indicates a compiler bug, e.g. calling segments that it did not load first.
// These may also be caused by memory corruption or by interpreter errors.
__forceinline void pmachine::validatepcodelogic(bool cond) const
{
#ifdef _DEBUG
    if (!cond)
        throwfatalerror(invalidpcode);
#endif
}

// check whether pointer is valid
template<typename T> __forceinline void pmachine::validateptr(const T * p) const
{
#ifdef _DEBUG
    if (p < rambottom || p > ramtop)   // pointing outside our VM's used RAM range
        throwfatalerror(BADMEM);
    if (p >= NP && p < SP)                  // pointing into unallocated RAM
    {
#if 1
        // p-code intercept
        // SYSTEM.COMPILER (incl. Apple) writes above NP at several places, must mean something
        // SYSTEM.LINKER II.0 has the same behaviour
        if (incodefile("SYSTEM.COMPILER") || incodefile("SYSTEM.LINKER"))
            return; // we tolerate it, as it does not seem to harm anything
        // end p-code intercept
#endif
        throwfatalerror(BADMEM);
    }
#endif
}

template<typename T> __forceinline void pmachine::validateptr(const PTR<T> & addr) const
{
#ifdef _DEBUG
    if (addr.offset == 0)
        throwfatalerror(BADMEM);    // TODO: really no NIL-pointer violation code?
    validateptr(ptr(addr));
#endif
}

static int round(float f)   // round to nearest neighbor as int --TODO: use standard function; there must be one
{
    if (f > 0)
        return (int) (f + 0.5);
    else
        return -(int) (-f + 0.5);
}

static INT toINT(int val)
{
    // range check --not.
    // Apple III TRM specifies that there is no integer-overflow check.
    // For example, EDITOR's INITIALIZE function computes MEMAVAIL * 2 which overflows for >= 32 KB free; and it tests for the value being < 0 to know...
    //if (val < -32768 || val > 32767) throw intoverflow();
    return (INT) val;
}

// TODO: call this regularly from a secondary thread
// TODO: Apple Pascal book claims that this is in 1/60 second units.
void pmachine::updatedatetime()
{
    SYSTEMTIME st;
    GetLocalTime(&st);     // (ignoring return code; can really not fail, we just got the time from Windows)
    if (syscom)
    {
        unsigned long daytimeinsecs = ((st.wHour * 60) + st.wMinute) * 60;
        syscom->LOWTIME = (WORD) daytimeinsecs;
        syscom->HIGHTIME = (WORD) (daytimeinsecs >> 16);
    }
    if (thedate)
    {
        thedate->day = st.wDay;
        thedate->month = st.wMonth;
        thedate->year = st.wYear % 100; // we are pre-Y2K!
    }
}

// call this when a program is changed
// This function checks and picks up possible changes in terminal dimensions.
// This is called for every terminal I/O of a program; and also at a program change
void pmachine::updateterminal(bool max84Cols/*EDITOR*/)
{
    // get desired size from UI
    // Desired change is a combination of user settings (e.g. font size), window size, and visibiliy of on-screen keyboard.
    size_t screencols, screenrows, usablecols, usablerows;
    term_desired_size(screencols, screenrows, usablecols, usablerows);
    // we may need to artificially clamp them
    auto cols = usablecols;
    auto rows = usablerows; // may be less due to on-screen keyboard
    // p-code intercept
    // TODO: here the logic kicks in on what our currently running program allows us to change
    if (max84Cols && cols > 224)        // EDITOR has been patched to handle this now
        cols = 224;
    // end p-code intercept
    // we need to pass back the screencols/screenrows to confirm which set of requests we picked up
    // because user might have changed it in the meantime
    term_confirm_size(screencols, screenrows, cols, rows);  // make it so, then remember it in SYSCOM
    syscom->CRTINFO[0] = term_height(); // height
    syscom->CRTINFO[1] = term_width();  // width
}

static bool begins_with_i(const char * s, const char * with)
{
    return _strnicmp(s, with, strlen(with)) == 0;
}

// determine special-knowledge hack mode for terminal, e.g. to emulate syntax highlighting and home/end keys, and to reset to TEXTMODE when leaving a user program
// Note: This is not super-cheap, so don't do it all the time. Will it slow down rendering?
// TODO: cache the last result; intended for loops that output multiple characters.
size_t pmachine::getspecialterminalmodes() const
{
    // somehow not working
    //if (SEG->segmentid == lastxmodesegno && PROC->procnum == lastxmodeprocno)
    //    return lastxmode;
    size_t xmode = 0;
    // SYSTEM.EDITOR
    if (incodefile("SYSTEM.EDITOR"))
    {
        xmode |= iterminal::specialmodes::inEDITOR;
        xmode |= iterminal::specialmodes::syntaxHighlighting;
        const char * prompt = promptlineascaptured.c_str();
        if (prompt[0] && prompt[0] < 'A')
            prompt++;       // skip over initial '>' or '<'
        bool inINSERTIT = begins_with_i(prompt, "INSERT:");
        bool inADJUSTING = begins_with_i(prompt, "ADJUST");
#if 0   // not working in Apple
        bool inADJUSTING1 = infunction(15, 39);  // this seems brittle in Apple system
#endif

        if (!inADJUSTING)
            xmode |= iterminal::specialmodes::emulateHomeEnd;
        if (!inADJUSTING)
            xmode |= iterminal::specialmodes::emulatePageUpDown;
#if 0   // not working in Apple
        if (infunction(15, 7))          // INSERTIT
            xmode |= enableSuggestions; // enable suggestions box
#endif
        // EDITOR does not use PL
        if (inINSERTIT)
            xmode |= iterminal::specialmodes::enableSuggestions; // enable suggestions box
        // TODO: detect as 'navigation' mode
        // TODO: xmode for right-arrow during EXCHANGE mode, which should read out what's on the screen, to avoid a ? (editor bug)
        // INSERTIT: (15, 7)    --maybe not in the Apple system?
        // DELETING: (15,45)
        xmode |= iterminal::specialmodes::cannotResizeTerminal;  // EDITOR cannot handle changed terminal size
    }
    // SYSTEM.FILER
    else if (incodefile("SYSTEM.FILER"))
    {
        xmode |= iterminal::specialmodes::inFILER;
    }
    // SYSTEM.VIEWFILE --we enable syntax highlighting for file viewing
    else if (incodefile("SYSTEM.VIEWFILE"))
    {
        xmode |= iterminal::specialmodes::syntaxHighlighting;
    }
    // shell
    if (!insysfunction(systemprocedures::syspnUSERPROG))
    {
        xmode |= iterminal::specialmodes::inShell;
    }

    // pathname completion enabled?
    if (insysfunction(systemprocedures::syspnFREADSTRING) && completepathnames)
        xmode |= iterminal::specialmodes::enablePathnameCompletion;  // enable suggestions for pathnames and TAB
#if 0
    // Note: We cannot check incodefile since the code file is SYSTEM.PASCAL which contains all I/O functions. Maybe check segment == 5? Not very different from this
    if (infunction(5, 1))
        xmode |= iterminal::specialmodes::inShellGETCMD;
#endif
    lastxmodesegno = SEG->segmentid;
    lastxmodeprocno = PROC->procnum;
    return xmode;
}

// decide whether a line written at top position looks like a command bar, based on heuristics
bool pmachine::promptlineiscommandbar() const
{
    auto p = promptlineascaptured.c_str();
    bool iscommandbar = false;
    // check for three X(y patterns
    size_t items = 0;
    for (wchar_t k = 0; p[k]; k++)
        if (p[k+1] == '(' && isupper((int) p[k]) && islower((int) p[k+2])) // X(y pattern
            items++;
    iscommandbar |= (items >= 2);            // two such patterns: surely must be a command bar! Yay!
    // check for EDITOR prompts
    iscommandbar |= (incodefile("SYSTEM.EDITOR")
                    && (p[0] == '<' || p[0] == '>')
                    && isalpha(p[1]) && isalpha(p[2]));
    return iscommandbar;
}

// we detected a state where we should show text--so better do that
// This function is called often (in keyboard read retry loop), so it should be fast.
// This is also called in the SYSTEM at reinitialization time
void pmachine::forcekillmultimedia()
{
    getunit(3)->clear();            // clear multimedia (which is device #3)
}

// call this when we know we should reset the terminal, e.g. a program change
// We also call it when we read a cmd character in the Shell.
// Note that this does NOT flush the input FIFO. Use UNITCLEAR(1) for that.
void pmachine::resetterminal()
{
    forcekillmultimedia();          // clear multimedia (which is device #3)
    updateterminal(false);
}

// we intercept all calls to SYS procedures (segment 0)
// This is only for the purpose of having the terminal do the rigth thing, with two exceptions:
//  - debugging (check entry/exit of functions)
//  - working around a bug in the BLOCKREAD function that will affect reading from CLIP: (or something like that)
// Note: This is similar but not the same as Apple UNIT emulations since we also track exits.
bool pmachine::emulatesystem(size_t procno, bool entering)
{
    switch (procno)
    {
    case systemprocedures::syspnBEGINEND:   // initialize
        memset(systemproccallstate, 0, sizeof(systemproccallstate));
        // reset pathname completion
        resetpathnameprefix();
        break;
    case systemprocedures::syspnUSERPROG:
        if (entering)                       // update term dimensions upon entering; handle EDITOR special which is limited to 84 cols
        {
            dprintf("entering USERPROG %s:\n", segtable[1].codefile.c_str());
            updateterminal(segtable[1].codefile == "SYSTEM.EDITOR");   // TODO: should be done only for the Apple disks? or how to know our own patched version?
        }
        else    // exiting from program back to shell
        {
            dprintf("exiting USERPROG:\n");
            resetterminal();
        }
        // reset the terminal w.r.t. stuff it may have memorized for the current program; used to flush Pascal symbols in the EDITOR
        {
            auto pterminal = dynamic_cast<terminalperipheral*>(getunit(2));
            if (pterminal)
                pterminal->getterminal().userprogchanged(); // flush leftover info from previous program
        }
        // reset Undo stack
        clearsnapshots();
        // check the segmenttable
        // All segments except 0 and, iff exiting, 1 must be unloaded.
        {
            size_t referencedsystemsegments = 0;
            for (size_t s = 0; s < _countof(segtable); s++)
            {
                dprintf("  RSEG #%d [%c] REF=%d\n", s, segtable[s].isSYSTEM ? 'S' : 'U', segtable[s].refcount);
                if (segtable[s].isSYSTEM && segtable[s].refcount > 0)
                    referencedsystemsegments++;
                else if (!segtable[s].isSYSTEM && segtable[s].refcount != (size_t)(s==1 && !entering))
                    throw logic_error("non-system non-entry segment not un-referenced at start time");
            }
            if (referencedsystemsegments ==0 || referencedsystemsegments > 2)      // SYS and, for Apple, GETCMD, one ref each
                throw logic_error("unexpected number of system segments referenced at start/end time");
        }
        break;
    case systemprocedures::syspnFWRITESTRING:
        if (entering)
        {
            auto & S = *ptr(tos<PTR<pstring>>(1));
            dprintf("FWRITESTRING(\'%s\')\n", S.tostring().c_str());
        }
        break;
    case systemprocedures::syspnFRESET:
        if (entering)
        {
            dprintf("FRESET\n");
        }
        break;
    case systemprocedures::syspnFINIT:
        if (entering)
        {
            auto & recwords = tos<INT>();
            dprintf("FINIT(%d)\n", recwords);
        }
        break;
    case systemprocedures::syspnFOPEN:
        if (entering)
        {
            const auto & S = *ptr(tos<PTR<pstring>>(2));
            dprintf("FOPEN(\'%s\')\n", S.tostring().c_str());
        }
        break;
    case systemprocedures::syspnFCLOSE: // (VAR F: FIB; FTYPE: CLOSETYPE)
        if (entering)
        {
            const auto & fib = *ptr(tos<PTR<FIB>>(1));
            dprintf("FCLOSE on %s unit #%d\n", fib.FISBLKD ? "block" : "char", fib.FUNIT);
            if (fib.FISOPEN && !fib.FISBLKD && fib.FUNIT <= _countof(devices)) // closing a char device like CLIP:
                FCLOSE_unittofinalize = fib.FUNIT;
            else
                FCLOSE_unittofinalize = -1;
        }
        else
        {
            if (FCLOSE_unittofinalize != -1)
                getunit(FCLOSE_unittofinalize)->finalize();     // finalize it
        }
        break;
    case systemprocedures::syspnFREADSTRING:    // we monitor FREADSTRING for pathname completion
        if (entering)
        {
            dprintf("entering FREADSTRING\n");
            FREADSTRING_S = ptr(tos<PTR<pstring>>(1));  // keep it for monitoring purposes
            // we "secretly" communicate the constraints in here, in the form
            // \x03S1,S2,...
            // enable pathname completion where we know it; otherwise user can toggle with Ctrl-F
            completepathnames = false;
            completionsuffixconstraints.clear();
            if (FREADSTRING_S->size() > 0 && FREADSTRING_S->at(1) == 3) // aha!
            {
                completepathnames = true;
                string patterns(FREADSTRING_S->tostring().substr(1));   // get the pattern
                for (size_t pos = patterns.find(','); pos != string::npos; pos = patterns.find(','))
                {
                    if (pos > 0)
                        completionsuffixconstraints.push_back(patterns.substr(0,pos));
                    patterns.erase(0,pos+1);
                }
                if (!patterns.empty())  // last one
                    completionsuffixconstraints.push_back(patterns);
            }
            // reset to 0 bytes
            // Original UCSD version will set length byte to max while reading,
            // i.e. string will contain uninitialized bytes.
            // Resetting it gives us an approximation to know the length (will fail when using backspace).
            // It will work correctly if we patch FREADSTRING to always update the length byte.
            // Latest SYSTEM no longer has this problem.
            size_t SLENG = tos<INT>(0);
            memset(FREADSTRING_S, 0, SLENG+1);
            //   || incodefile("SYSTEM.FILER") || !insysfunction(systemprocedures::syspnUSERPROG);
            // reset pathname completion
            resetpathnameprefix();
        }
        else
        {
            dprintf("exiting FREADSTRING with '%s'\n", FREADSTRING_S ? FREADSTRING_S->tostring().c_str() : "(null)");
            FREADSTRING_S = nullptr;
            completepathnames = false;
            // reset pathname completion
            resetpathnameprefix();
        }
        break;
    case systemprocedures::syspnFBLOCKIO:   // bug fix for BLOCKREAD
        // p-code intercept
        if (!entering && syscom->IORSLT == INOERROR_return_2) // bug fix of BLOCKREAD of TEXT file
        {
            // TODO: can this just be fixed in the Pascal source code?
            auto & retval = BP->locals<WORD>(1);
            retval = 2;
            syscom->IORSLT = INOERROR;
        }
        // end p-code intercept
        break;
    }
    // keep track of all entering and exiting
    if (procno < _countof(systemproccallstate))
        if (entering)
            systemproccallstate[procno]++;
        else if (systemproccallstate[procno] == 0)
            throw logic_error("too many exits from a system procedure registered");
        else
            systemproccallstate[procno]--;
    return false;   // means 'execute normally'
}

diskperipheral * pperipherals::getdiskunit(size_t u)
{
    return dynamic_cast<diskperipheral*> (getunit(u));
}

// scan disks for volume match
int pmachine::VOLSEARCH(const char * FVID)
{
    for (size_t u = 0; u < _countof(devices); u++)
    {
        auto disk = getdiskunit(u);
        if (!disk)
            continue;
        auto volname = disk->volname();
        if (_stricmp(volname.c_str(), FVID) == 0)
            return u;
    }
    return -1;      // not found
}

// flush pathname completion
void pmachine::resetpathnameprefix()
{
    // Not resetting it to empty so that we detect a change when starting a new FREADSTRING
    currentpathnameprefix.assign(1, 27);    // something that is not matching a path
}

// update 'matchingpathnames' for given string
// return 'true' if we are quite positive that it's a pathname to complete:
//  - a full matching volume spec with : (but not : or *)
//    -or-
//  - at least 3 matching characters of a filename
// (not used; we may use this for any FREADSTRING later)
bool pmachine::determinematchingpathnames(const string & prefix, vector<string> & matchingpathnames, bool aftercomma, bool wildcardbeforecomma)
{
    matchingpathnames.clear();
    auto bracketpos = prefix.find('[');
    if (bracketpos != string::npos)             // has a [, means it is a full pathname, nothing to complete
        return false;
    // parse volume; we may either have a complete one, or not
    string vtid;    // vol title if got one
    int u = -1;     // resolved unit if got one
    size_t k = 0;
    int codeunit = (segtable[1].codeunit <= _countof(devices)) ? segtable[1].codeunit : -1;
    // parse for a volume
    auto colonpos = prefix.find(':');
    if (colonpos != string::npos)
    {
        vtid = prefix.substr(0, colonpos);
        k = colonpos+1;
        // got a volume name: try to resolve
        // special cases:
        if (vtid.empty())           // just : is the P(refix as set in FILER
        {
            if (!DKVID->empty() && DKVID->size() <= 7)  // otherwise it's invalid
                vtid = DKVID->tostring();
        }
        else if (vtid == "*")       // * is the system volume
            u = syscom->SYSUNIT;
        else if (vtid == "%")       // % is the volume where seg 1 was loaded from
            u = codeunit;
        else if (vtid[0] == '#')    // #unit
        {
            char * endptr;
            double d = strtod(&vtid[1], &endptr);
            if (*endptr == 0 && d == (int) d)   // full volume number
                u = (int) d;        // (negative number will keep u negative)
        }
        // if unit not known yet then try to resolve by name
        if (u < 0)
            u = VOLSEARCH(vtid.c_str());
    }
    else if (!prefix.empty() && prefix[0] == '*')      // * but not *: is also valid
    {
        u = syscom->SYSUNIT;
        k = 1;
    }
    else if (!prefix.empty() && prefix[0] == '%' && codeunit >= 0)  // likewise for %
    {
        u = codeunit;
        k = 1;
    }
    // here we can have unit >= 0 (resolved unit) or vtid not empty (full but non-resolvable volume name)
    if (u < 0 && !vtid.empty())     // unresolvable volume spec
        return false;
    bool fullvolspec = k > 1;       // only true if there is a : and it's not just :
    // if no unit spec then input could be a volume prefix; enumerate all matches
    // Note: the empty string gets us here.
    if (u < 0)
    {
        // if empty then also include *, :, and CLIP: with comment what they are
        if (prefix.empty())
        {
            // this one &bnsp; tells suggestion box where to cut off the string
            matchingpathnames.push_back(":\xa0      ("+DKVID->tostring()+":, prefix)");
            matchingpathnames.push_back("*\xa0      ("+SYVID->tostring()+":, system)");
        }

        // enumerate all matching volumes
        for (size_t k = 0; k < _countof(devices); k++)
        {
            auto disk = getdiskunit(k);
            if (!disk)
                continue;
            string volname;
            if (prefix[0] == '#')
            {
                char buf[20];
                sprintf(buf, "#%d", k);
                volname = buf;
            }
            else
            {
                volname = disk->volname();
                if (volname.empty())
                    continue;
            }
            if (begins_with_i(volname.c_str(), prefix.c_str()))
                matchingpathnames.push_back(volname + ":");
        }

        // fake volumes we want to show as well
        if (begins_with_i("CLIP", prefix.c_str()))
            matchingpathnames.push_back("CLIP:\xa0  (clipboard)");
    }
    // show hints for wildcards
    //  ? wildcard (FILER allows two: ? and =)
    //  ! second arg: wildcard string goes here
    //  $ substitute source name
    // Wildcards follow the logic in FILER:
    //  = means any sequence (at most one wildcard)
    //  ? means any sequence and ask
    //  $ means substitute the source name
    // SYSTEM also accepts $ for compile target:
    //  $ means substitute the source stem + .CODE
    // Notes:
    //  - We do not support the format SRC,$ without wildcards enabled for SRC.
    //  - We have no way of knowing whether a second arg is even allowed (e.g. R(emove)
    // FILER:
    //  - for initial input (which accepts one or two args):
    //     - FILER will pass ?
    //     - when we are after comma, our caller will tell us whether there was a wildcard before the comma
    //  - for second input:
    //     - FILER will pass ! if first arg had a wildcard
    //     - FILER will pass $ if 
    bool hasw = false;  // only one wildcard allowed per name
    for (size_t j = 0; !hasw && j < prefix.size(); j++)
        hasw |= prefix[j] == '?' || prefix[j] == '=';
    bool isfirstfilechar = prefix.size() == k;
    size_t numspecialconstraints = 0;   // (we discount these later)
    for (size_t j = 0; j < completionsuffixconstraints.size(); j++)
    {
        if (completionsuffixconstraints[j] == "?")
        {
            if (!hasw)                  // only one per name
                if (!aftercomma)
                {
                    matchingpathnames.push_back ("?\xa0      (wildcard with verification)");
                    matchingpathnames.push_back ("=\xa0      (wildcard)");
                }
                else if (wildcardbeforecomma)   // first name had a wildcard
                    matchingpathnames.push_back ("=\xa0      (wildcard goes here)");
            if (aftercomma && isfirstfilechar)
                matchingpathnames.push_back ("$\xa0      (same name)");
        }
        else if (completionsuffixconstraints[j] == "!")
        {
            if (!hasw)              // only one per name
                matchingpathnames.push_back ("=\xa0      (wildcard goes here)");
            if (isfirstfilechar)
                matchingpathnames.push_back ("$\xa0      (same name)");
        }
        else if (completionsuffixconstraints[j] == "$")
        {
            if (isfirstfilechar)
                matchingpathnames.push_back ("$\xa0      (same name)");
        }
        else continue;
        numspecialconstraints++;    // (we need to discount these later)
    }
    // we got a volume spec: show matching files
    // if no vol spec but at least one letter, show matching files on all volumes
    if (u >= 0 || !prefix.empty())
    {
        // trying to cover both full enumeration and single-volume in a single loop
        vector<bool> done(_countof(devices), false);        // we hit the same volume multiple times possibly
        int udef = (u < 0) ? VOLSEARCH(DKVID->tostring().c_str()) : u;      // default volume
        for (int u1 = -2; u1 < (int) _countof(devices); u1++)     // -2 is default, -1 is system
        {
            if (u >= 0 && u1 != u)                          // vol is specified: only do that one
                continue;
            int u2 = (u1 == -2) ? udef : (u1 == -1) ? syscom->SYSUNIT : u1;    // not: do default and system first
            if (u2 < 0)                                     // a missing default
                continue;
            auto disk = getdiskunit(u2);
            if (!disk)
                continue;
            if (done[u2])                                   // avoid including the same volume twice
                continue;
            done[u2] = true;
            auto volspec = prefix.substr(0, k);             // user-typed vol spec if we got a complete one
            auto fileprefix = prefix.c_str() + k;           // if we got a vol spec then k > 0
            // if no vol spec from user and showing the non-default disk: prepend vol name
            if (volspec.empty() && u1 > -2)                 // (no prefix for default disk)
                volspec = disk->volname() + ":";
            // show all files that match on this volume
            if (fileprefix[0] == 0 || isalnum((unsigned char)fileprefix[0])) // empty fileprefix means show all paths
            {
                for (size_t k = 0; k < volspec.size(); k++) // uppercase the user-typed vol name
                    volspec[k] = toupper((unsigned char)volspec[k]);
                auto matches = disk->completefilename(fileprefix);
                for (size_t k = 0; k < matches.size(); k++) // show names complete
                {
                    string path = volspec + matches[k];
                    // BUGBUG: conflicts with special ones
                    if (completionsuffixconstraints.size() == numspecialconstraints)
                        matchingpathnames.push_back (path);
                    else for (size_t j = 0; j < completionsuffixconstraints.size(); j++)
                    {
                        const string & suff = completionsuffixconstraints[j];
                        if (path.find(suff) == path.size() - suff.size())
                        {
                            matchingpathnames.push_back (path);
                            break;
                        }
                    }
                }
            }
        }
    }
#if 0
    // if we have a full match and only that one, don't open the box
    if (matchingpathnames.size() == 1)  // only one ch
        matchingpathnames.clear();
#endif
    return !matchingpathnames.empty() && fullvolspec;
}


// RETROSTUFF procedure NEWARRAY, like calloc()
PTR<void> pmachine::NEWARRAY(int count, int bytesize)
{
    if (count < 0 || bytesize < 0)
        throwoutofbounds();
    size_t wordsize = (bytesize+1)/2;                   // can't have odd sizes in arrays
    flushGDIRP();                                       // flush GDIRP if necessary
    size_t numwords = count * wordsize;
    auto newNP = numwords + (WORD*) NP;                 // heap grows from the bottom
    if (newNP > SP)
        throw heapoverflow();
    validatetosptr();
    PTR<void> result = addr(NP);
    memset(NP, 0, numwords * sizeof(WORD));
    NP = newNP;
    return result;
}

size_t pmachine::RAMAVAIL()
{
    return intptr_t(SP)-intptr_t(NP);
}

#define UT      // mark UNTESTED op-codes
// standard procedures
void pmachine::csp()
{
    cspcodes cspcode = (cspcodes) *IP++;
    switch (cspcode)
    {
    case IOC: if (syscom->IORSLT) throwruntimeerror(UIOERR); return;
    case NEW:   // (var^^, numwords)
        {
            flushGDIRP();                                       // flush GDIRP if necessary
            size_t numwords = popw();
            auto newNP = numwords + (WORD*) NP;                 // heap grows from the bottom
            if (newNP > SP)
                throw heapoverflow();
            validatetosptr();
            PTR<void> * varpp = ptr(pop<PTR<PTR<void>>>());     // address of var^
            *varpp = addr(NP);
            //memset(NP, 0xcd, numwords * sizeof(WORD));          // to give it some defined value; Pascal NEW must init it also
            NP = newNP;
            return;
        }
    case MVL:                                                   // MOVELEFT(src, dst, numbytes)
        {
            int size = popi();
            checkboundscondition(size >= 0);
            auto dst = byteptr(pop<BYTEPTR>());
            auto src = byteptr(pop<BYTEPTR>());
            checkboundscondition(src + size <= ramtop);         // avoid emulatede code to write outside the VM RAM
            checkboundscondition(dst + size <= ramtop);
            for (int k = 0; k < size; k++)
                dst[k] = src[k];
            return;
        }
    case MVR:                                                   // MOVERIGHT(src, dst, numbytes)
        {
            int size = popi();
            checkboundscondition(size >= 0);
            auto dst = byteptr(pop<BYTEPTR>());
            auto src = byteptr(pop<BYTEPTR>());
            checkboundscondition(src + size <= ramtop);         // avoid emulatede code to write outside the VM RAM
            checkboundscondition(dst + size <= ramtop);
            for (int k = size -1; k >= 0; k--)
                dst[k] = src[k];
            return;
        }
    case EXIT:
        // Exit from procedure. Tos is the procedure number, tos-1 is the segment number.
        // Strangely, they are not constants in the p-code but stackbased.
        // This operator sets IPC to point to the exit code of the currently executing procedure,
        // then sees if the current procedure is the one to exit from. If it is, control returns to the instruction fetch loop.
        // Otherwise, each MSCW has its saved IPC changed to point to the exit code of the procedure that invoked it, until the desired procedure is found.
        // If at any time the saved IPC of main body of the operating system is about to be changed, a run-time error occurs. 
        // TODO: The above does not sound like I wrote this. Where does this come from?
        // BUGBUG(?): Can we EXIT from FUNCTIONs? E.g. from F(x) in (1 + F(x))? That would leave '1' on the stack.
        // Looking at the compiler sources, it seems EXIT is allowed in functions.
        // Getting rid of the cpu stack altogether would solve this.
        {
            size_t procid = popw();
            size_t segid = popw();
            const pproctable * proctable = SEG;
            const pprocdescriptor * procdesc = PROC;
            IP = procdesc->exitIP();                                // this is like a GOTO to an exit label
            pstackframe * stackframe = BP;
            while (segid != proctable->segmentid || procid != procdesc->procnum)
            {
                // Special check: return to the outer-most HLT instruction will trigger EXECERROR which
                // will try ask to press SPACE then EXIT(COMMAND), which will fail since we are no longer
                // in there. We catch this and cleanly terminate the interpreter.
                if (stackframe == GBP0 && SEG->segmentid == 0 && PROC->procnum == 2)
                    throw std::runtime_error("Programmed HALT");
                validatepcodelogic(stackframe != GBP0);             // no return IP to patch at this point
                proctable = ptr(stackframe->proctableaddr);         // my caller
                procdesc = ptr(stackframe->procdescraddr);
                CODE * IP0 = procdesc->entryIP();
                stackframe->IPoffset = procdesc->exitIP() - IP0;    // I return to my caller's exitIP instead of where I was called from
                stackframe = ptr(stackframe->callerframeaddr);
            }
            return;
        }
    case UREAD:     // (u, (addr, offset), size, block, mode)
        // This is a VERY complex thing. UREAD from terminal intercepts a lot of stuff:
        //  - polling from keyboard (if no input available, we signal yield the p-machine and set up to reenter here next time)
        //  - pathname completion (if we are in FREADSTRING)
        //  - undo for EDITOR and string input --not yet completed
        {
            auto mode = tosw(0);                        // we don't pop yet because of all that restart/undo semantics
            auto block = tosi(1);
            auto size = tosw(2);
            auto byteptr = *(BYTEPTR *) &tosw(3);       // (ugh: 3 is an index into an array of tos<> type, so cannot use tos<BYTEPTR>() here)
            auto u = tosw(5);                           // (note: byteptr is two words, so this is word [5])
            IP -= 2;                                    // also reset PC to BEFORE the CSP UREAD call (for restart/undo)
            // we now have all args, but the p-machine is in the state before the CSP UREAD call in case we need to restart
restart_UREAD:                                          // we get here if we intercept and process a keystroke
            {   // (TODO: remove this level of indentation; no need)
#ifdef _DEBUG
                fflush(0);  // flush the trace file
#endif
                validateptr(byteptr);
                BYTE * p = ptr<BYTE>(byteptr.addr);
                // p-system intercept
                size_t xmode = mode;                    // for emulation purposes, we can add special modes here that are inaccessible to Pascal (>16 bit)
                if (u == 1 || u == 2)
                {
                    xmode |= getspecialterminalmodes(); // special emulation
                    // detect terminal size change (done for every read from terminal)
                    if (!(xmode & iterminal::specialmodes::cannotResizeTerminal))
                        updateterminal(false);               // make sure we got the correct terminal size; done for every call, except if we detect a program that cannot handle it
                    // pathname completion
                    if (xmode & iterminal::specialmodes::enablePathnameCompletion)
                    {
                        // get current ongoing input line
                        // Note: S may have wrong length; we cleared it, so we can construct from c_str() unless user typed BS.
                        // Our own build of SYSTEM makes sure the length is correct before calling FGET
                        string prefix(FREADSTRING_S ? string(FREADSTRING_S->tostring().c_str()) : string());
                        // delete all expressions up to the last , since FILER allows multiple paths
                        bool wildcardbeforecomma = false;   // wildcard before comma?
                        bool aftercomma = false;
                        for (auto pos = prefix.find(','); pos != string::npos; pos = prefix.find(','))
                        {   // this code is weird; why not just use find_last_of?
                            wildcardbeforecomma |= (prefix.find_first_of("?=") < pos);
                            prefix.erase(0, pos+1);
                            aftercomma = true;
                        }
                        // if changed then pass it on
                        if (prefix != currentpathnameprefix)    // (we cache it, so search only once per character typed)
                        {
                            vector<string> matchingpathnames;         // for pathname completion in FREADSTRING
                            determinematchingpathnames(prefix, matchingpathnames, aftercomma, wildcardbeforecomma);
                            // pass it over to the terminal
                            auto pterminal = dynamic_cast<terminalperipheral*>(getunit(2));
                            if (pterminal)
                                pterminal->getterminal().setcompletioninfo(prefix, move(matchingpathnames));
                            // remember we sent it
                            currentpathnameprefix = prefix;
                        }
                    }
                }   // end if read from terminal
                // we track BLOCKREAD for reading from CLIP:
                if (systemproccallstate[systemprocedures::syspnFBLOCKIO] > 0)
                    xmode |= pperipheral::specialmodes::inFBLOCKIO;
                // UNDO
                bool undoenabled = false;
#if 0			// undo not implemented for this release (it's too slow on a Surface RT)
                if (u == 1 || u == 2)
                {
                    // are we at a point where UNDO is allowed?
                    int readprocs[] = { syspnFREADINT, syspnFREADREAL, syspnFREADCHAR, syspnFREADSTRING, syspnFREADLN, syspnFREADDEC };
                    undoenabled = (xmode & iterminal::specialmodes::inEDITOR) != 0;
                    for (size_t k = 0; !undoenabled && k < _countof(readprocs); k++)
                        undoenabled |= insysfunction(readprocs[k]);
                    // if we actually have a snapshot to undo then tell UX to enable the Undo button on the AppBar
                    // We do this every time we poll the keyboard, so this better be fast.
                    auto pterminal = dynamic_cast<terminalperipheral*>(getunit(2));
                    if (pterminal)
                        pterminal->getterminal().undoenabled(undoenabled && haspastsnapshot(), undoenabled && hasfuturesnapshot());
                }
#endif
                // end p-system intercept

                auto ioResult = getunit(u)->read(p, byteptr.byte, size, block, xmode);
                if (ioResult == ICALLAGAIN)
                {
                    yield(50);              // come back in 50 ms
                    return;
                }
                syscom->IORSLT = ioResult;
                traceIORSLT();

                // p-system intercept
                // allow to toggle pathname completion while inside FREADSTRING
                if ((u == 1 || u == 2) && insysfunction(systemprocedures::syspnFREADSTRING) && syscom->IORSLT == INOERROR)
                {
                    if (size == 1 && p[byteptr.byte] == 0x06)   // Ctrl-F was just entered
                    {
                        completepathnames = !completepathnames; // toggle
                        if (!completepathnames)
                            resetpathnameprefix();
                        goto restart_UREAD;
                    }
                }
                // paste support
                //if (size == 1 && p[byteptr.byte] == 22)     // Ctrl-V was just entered
                //    sin(1.0);
                // UNDO
                // If we are in EDITOR or a string read function (and reading from terminal of course)
                // then snapshot the machine in the state before responding to the keystroke.
                // Ctrl-Z will take us back, and Ctrl-Y forward (redo).
                // Undo stack will be kept across read calls, i.e. we can go back in an app that prompts user one by one for text input.
                // Note that some engine state changes break Undo; such as writing to disk, drawing (we don't keep graphics), or changing programs of course.
                // Those will be intercepted and explicitly kill all snapshots.
                if (undoenabled)
                {
                    if (size == 1 && p[byteptr.byte] == 26)     // Ctrl-Z (undo) was just entered
                    {
                        if (poppastsnapshot())                  // roll back entire VM
                        {
                            yield(1);                           // and give outer level a chance to do something
                            return;                             // this will pick up execution at an earlier state
                        }
                        else
                            goto restart_UREAD;                 // no snapshot to roll back: just ignore this
                    }
                    else if (size == 1 && p[byteptr.byte] == 25)// Ctrl-Y (redo) was just entered
                    {
                        if (popfuturesnapshot())                // roll *forward* entire VM
                        {
                            yield(1);                           // and give outer level a chance to do something
                            return;                             // this will pick up execution at an earlier state
                        }
                        else
                            goto restart_UREAD;                 // no snapshot to roll back: just ignore this
                    }
                    else
                        pushsnapshot();                         // add an undo state (snapshot)
                }
                // end p-system intercept
#ifdef _DEBUG
                // debugging for CODE files
                // We keep a copy of the last SEGTBL read so we can check it in case pushsegment() fails
                if (size == 512)
                {
                    // TODO: use getdiskunit()
                    auto foundondisk = dynamic_cast<diskperipheral *> (devices[u].get());
                    if (foundondisk)     // not a disk
                    {
                        string name = foundondisk->findfilebyblock(block);
                        size_t beginblock, filesize;
                        if (foundondisk->findfile(name.c_str(), beginblock, filesize) == INOERROR)
                        {
                            if (beginblock == block && filesize >= 1024 && byteptr.byte == 0)
                            {
                                if (!lastsegtable)
                                    lastsegtable = (SEGTBL*) malloc(sizeof (*lastsegtable));
                                memcpy(lastsegtable, p, sizeof(*lastsegtable));
                                // print it for debugging
                                dprintf("segtable of '%s':\n", name.c_str());
                                for (size_t s = 0; s < lastsegtable->capacity; s++)
                                {
                                    if (lastsegtable->DISKINFO[s].CODELENG == 0)
                                        continue;
                                    dprintf("  LSEG #%2d '%s' RBLK=%4d LENG=%4d KIND=%1d, INFO=%04x -> RSEG #%2d\n", (int) s, lastsegtable->SEGNAME[s].tostring().c_str(),
                                            (int) lastsegtable->DISKINFO[s].DISKADDR, (int) lastsegtable->DISKINFO[s].CODELENG,
                                            (int) lastsegtable->SEGKIND[s], (int) *(WORD*)&lastsegtable->SEGINFO[s],
                                            lastsegtable->SEGINFO[s].SEGNUM ? lastsegtable->SEGINFO[s].SEGNUM : s);
                                }
                            }
                        }
                    }
                }
#endif
#if 1   // debugging
                {
                    const SEGTBL * segtbl = (const SEGTBL*) p;
                    sin(1.0);
                }
#endif
                // success: now we can finally advance the p-machine beyond this CSP UREAD
                dropwords(6);           // pop the arguments off the stack
                IP += 2;                // recover PC (CSP UREAD)
                return;
            }
        }
    case UWRITE:
        {
            auto mode = popw();
            auto block = popi();
            auto size = popw();
            auto byteptr = pop<BYTEPTR>();
            auto u = popw();
            // p-system intercept
            size_t xmode = mode;                    // for emulation purposes, we can add special modes here that are inaccessible to Pascal (>16 bit)
            if ((u == 1 || u == 2) && size > 0)       // writing to CONSOLE
            {
                //xmode |= getspecialterminalmodes(); // TODO: why is this necessary? for writing?
                auto p = (const char*) ptr<BYTE> (byteptr.addr) + byteptr.byte;
                // track whether we just moved the cursor to home
                // We will ignore single-char writes of clear-to-end-of-screen/line, since it does not change the state of interest.
                if (size > 1 || (p[0] != 29 && p[0] != 11))
                {
                    // if we just moved to home then we capture the string
                    size_t kept = _countof(lastcharswrittentoconsole);
                    if (lastcharswrittentoconsole[kept-1] == 12 || lastcharswrittentoconsole[kept-1] == 25 // detect a move to home
                        || (lastcharswrittentoconsole[kept-3] == 30 && lastcharswrittentoconsole[kept-2] == 32 && lastcharswrittentoconsole[kept-1] == 32))
                    {
                        // capture any string that gets written
                        if (byteptr.byte == 1                   // we try to capture a string write, which has this pattern (offset 0 is the string length)
                            && size > 1)
                            promptlineascaptured.assign(p, min(size, 224)); // capture the string
                        else if (size == 1 && !promptlineascaptured.empty()
                                 && (*p == '<' || *p == '>')
                                 && incodefile("SYSTEM.EDITOR"))    // EDITOR writes this as single chars to update the prompt
                                 promptlineascaptured[0] = *p;
                        else
                            promptlineascaptured.clear();       // anything else (e.g. newlines) is known to be not a prompt, so don't capture it as one
                        // if it looks like a command bar then tell terminal to treat it as one
                        if (promptlineiscommandbar())
                            dynamic_cast<terminalperipheral*>(getunit(2))->getterminal().setcommandbar(promptlineascaptured);
                    }
                    // keep a history to detect 'arming', that is GOTOXY(0,0) or the home characters (29, 12)
                    size_t topush = min(size, kept);
                    memmove(lastcharswrittentoconsole, lastcharswrittentoconsole + topush, kept - topush);
                    memcpy(lastcharswrittentoconsole + kept - topush, p + size - topush, topush);
                }
            }
            else if (dynamic_cast<diskperipheral*> (getunit(u)) != nullptr)     // writing to disk: kills Undo/Redo snapshots as we cannot recover disk changes
                clearsnapshots();
            else if (u == 3)            // rendering something GRAPHIC: also kills Undo since we cannot undo the graphics screen
                clearsnapshots();
#if 0
            if (size != 35 || u != 3)   // SYSTEM.COMPILER writes 35 bytes from random RAM to unit 3--we ignore it
                validateptr(byteptr);
#endif
            if (systemproccallstate[syspnFBLOCKIO] > 0)
                xmode |= pperipheral::specialmodes::inFBLOCKIO;
            // end p-system intercept
            syscom->IORSLT = getunit(u)->write(ptr<BYTE> (byteptr.addr), byteptr.byte, size, block, xmode);
            traceIORSLT();
            return;
        }
    case IDS:
        // called if symbol begins with a letter: IDSEARCH(SYMCURSOR,SYMBUFP^); (* MAGIC PROC *)
        {
            const CHAR * SYMBUFP = ptr(pop<PTR<CHAR>>());
            idsearchargs & SYMCURSORff = *ptr(pop<PTR<idsearchargs>>());    // actually the first of 4 variables
            idsearch(SYMCURSORff, SYMBUFP);
            return;
        }
    case TRS:
        // e.g. IF TREESEARCH(FCP,FCP1,ID) = 0 THEN (*NADA*)
        {
            ALPHA & nameid = *ptr(pop<PTR<ALPHA>>());
            PTR<treesearchnode> & node = *ptr(pop<PTR<PTR<treesearchnode>>>());
            PTR<treesearchnode> root = pop<PTR<treesearchnode>>();
            INT res = treesearch(root, node, nameid);
            pushi(res);
            return;
        }
    case TIM:                                   // TIME(VAR HNOW,VAR LNOW); also update SYSCOM
        {
            // TODO: run this as a background thread; just update SYSCOM directly?
            updatedatetime();
            *ptr(pop<PTR<WORD>>()) = syscom->LOWTIME;
            *ptr(pop<PTR<WORD>>()) = syscom->HIGHTIME;
            return;
        }
    case FLC:                                   // FILLCHAR(DESTINATION: array, LENGTH: INTEGER, CHARACTER: CHAR);
        {
            BYTE val = (BYTE) popw();
            int size = popi();
            auto dst = byteptr(pop<BYTEPTR>());
            if (size < 0)
            {
                size = -size;
                dst -= size;
            }
            checkboundscondition(dst + size <= ramtop);
            memset(dst, val, size);
            return;
        }
    case SCN:                                   // FUNCTION SCAN(LENGTH, partial-expression, ARRAY): INTEGER;
        {
            /*
   	    ; scan(maxdisp: integer; forpast: (forch, pastch); ch: char; start: ^; mask: PACKED ARRAY[0..7] of boolean): integer
	    ; scan until either
	    ;   maxdisp characters examined, or
	    ;   a match (if forpast=forch) or non-match (if forpast=pastch) occurs.
	    ; as function value return end_position-start
             */
            popw();                             // Z80 code comment: "junk the mask (fuckin' Richard)"
            const CHAR * p = byteptr(pop<BYTEPTR>());
            CHAR ch = (CHAR) popw();
            bool notmatch = (popw() != 0);      // (forpast)
            int maxdisp = popi();               // note: misleading name; not max but num
            int step = maxdisp >= 0 ? 1 : -1;
            int size = abs(maxdisp);
            int k;
            // security: this may scan outside the VM, but not critical since it will not write anything
            for (k = 0; k < size; k++)
            {
                bool match = p[k * step] == ch;
                if (match ^ notmatch)
                    break;
            }
            pushi(k * step);
            return;
        }
UT  case USTAT: // (u, addr, offset, ?)
        {
            popw(); popi();
            WORD & res = *ptr(pop<PTR<WORD>>());
            syscom->IORSLT = getunit(popw())->stat(res);
            traceIORSLT();
            return;
        }
    case GSEG: refsegment(popw()); return;                  // get segment: loads in a segment if it isn't in already. segnum is on tos
    case RSEG: derefsegment(popw()); return;                // release segment: bumps down refcount, then junks seg if count goes to 0
    case TNC: push(toINT((int)(popr()))); return;
    case RND: push(toINT(round(popr()))); return;
    case SIN: tosr() = sinf(tosr()); return;
    case COS: tosr() = cosf(tosr()); return;
UT  case LOG: tosr() = log10f(tosr()); return;
UT  case ATAN: tosr() = atanf(tosr()); return;
UT  case LN: tosr() = log(tosr()); return;
UT  case EXP: tosr() = expf(tosr()); return;
    case SQT: tosr() = sqrtf(tosr()); return;
    case MRK:                                               // mark(VAR i: ^integer) store NP in i
        {
            flushGDIRP();                                   // flush GDIRP if necessary
            PTR<void> & mark = *ptr(pop<PTR<PTR<void>>>()); // reference to i^
            mark = addr(NP);
            return;
        }
    case RLS:                                   // release(VAR i: ^integer) store contents of i into NP; Set GDIRP to nil, then store word pointed to by tos into NP. 
        {
            PTR<void> & mark = *ptr(pop<PTR<PTR<void>>>());
            // Apple TURTLEGRAPHICS calls this with 16484 to move the heap above the graphics buffer!
            if (mark.offset == 16384 && SEG->segmentid == 20/*TURTLEGRAPHICS*/ && PROC->procnum == 1/*INITTURTLE*/)
                return; // we just ignore it
            void * newNP = ptr(mark);
            checkboundscondition(newNP <= NP);
            // Note: we must clear GDIRP AFTER reading out the mark, because the mark may be the GDIRP itself!
            syscom->GDIRP.offset = 0;
            NP = newNP;
            return;
        }
    case IORP: push(syscom->IORSLT); return;
    case POT: push(powf(10.0f, popi())); return;    // TODO: we don't fail for arg < 0 or arg > 39; esp. we should avoid NaN!
    case UBUSY: popw(); syscom->IORSLT = INOERROR; pushw(0); return;    // dummy
    case UWAIT: popw(); syscom->IORSLT = INOERROR; return;              // dummy
    case UCLEAR:
        {
            auto u = popw();
            // p-system intercept
            /* from SYSTEM:
            reads a full SYSCOMREC from '*SYSTEM.MISCINFO' file and copies these values over to real SYSCOM:
            MISCINFO := MSYSCOM.MISCINFO;
            CRTTYPE := MSYSCOM.CRTTYPE;
            CRTCTRL := MSYSCOM.CRTCTRL;
            CRTINFO := MSYSCOM.CRTINFO;
            UNITCLEAR(1) (*GIVE BIOS NEW SOFT CHARACTERS FOR CONSOLE*)
            */
            // A hack: above load looses the WORD_MACH flag, but since we call here immediately,
            // we can patch it back here. This will never be wrong, since that's what our machine is.
            // TODO: What I don't get is that we get here with unit=2, not 1. Understand this. Or not.
            syscom->MISCINFO |= 0x400;  // WORD_MACH got lost when loading the MISCINFO file
            syscom->IORSLT = (tosi() <= 6 || tosi() >= 11) ? INOERROR : IBADUNIT;   // emulate Miller system although it's buggy (should be >= 9!)
#if 0       // no longer needed since we changed SYSTEM.PASCAL
            // another hack: we patch the name of REMIN to CLIP
            if (u == 8) // once we touch unit 8, we have done 7, so we can patch the name from REMIN to CLIP
            {
                // TODO: change this so that we only check a fixed offset (Apple), assuming our own SYSTEM already has that name in it
                // we don't know whether the system uses the precise layout, so we search for it
                char * pstart = (char*) &GBP0->locals<char>(68)/*after THEDATE*/;
                char * pend = pstart + 2000;
                for (char * p = pstart; p < pend; p++)
                {
                    if (strcmp(p, "\005REMIN") == 0) // hit FILE_TABLE, which is after REMIN
                    {
                        // found it
                        //size_t offset = p - pstart;
                        // Apple disks: at word offset 168 from GBP0
                        strcpy(p, "\004CLIP");
                        break;
                    }
                    if (strcmp(p, "SYSTEM.COMPILER") == 0) // hit FILE_TABLE, which is after REMIN
                        break;
                }
                //auto REMIN = &GBP0->locals<pstring>(67);    // UNITTABLE starts at 210; each entry has 4
            }
#endif
            // end p-system intercept
            // execute the clear() on the unit
            syscom->IORSLT = getunit(u)->clear();
            traceIORSLT();
            return;
        }
    case HALT: throwruntimeerror(HLT); return;
    case MEMA:
        {
            size_t wordsavail = (WORD*)SP - (syscom->GDIRP.offset ? (WORD*)ptr(syscom->GDIRP) : (WORD*)NP);
#if 0       // for regression test against Miller engine:
            wordsavail /= 2;                // Miller does this also for word mode, which is incorrect
            wordsavail -= 24559 - 24226;    // Miller has less space (starts lower I think)
            wordsavail -= 1;                // for compiler run; seems different
#endif
            if (wordsavail > 32767)         // it's an INT, so this is the max number we can report--p-code design bug!
                wordsavail = 32767;
            pushw(wordsavail);
            return;        // # words of memory left
        }
    // DECOPS is a ReTro-Pascal specific CSP code, only our modified compiler uses this
    case DECOPS: decops(); return;
    default: cspcode; throwfatalerror(NOTIMP); return;
    }
}

// DECOPS special procedures
// the format seems up to us; for format spec, see pfixeddecint type
void pmachine::decops()
{
    pdecint a, b, r;
    size_t n;
    decopscodes decopscode = (decopscodes) popw();
    switch (decopscode)
    {
    case DAJ:
        n = popw();
        if (tosw() != n)
        {
            popd(a);
            a.adjust(n);
            pushd(a, false);    // note: no length word anymore
        }
        else
            popw();             // same length: just pop the length word
        return;
    case DAD:
        popd(b); popd(a);
        r.sum(a, b);
        pushd(r, true);
        return;
    case DSB:
        popd(b); popd(a);
        r.diff(a, b);
        pushd(r, true);
        return;
    case DNG:
        {
            auto & d = tos<pdecint>();
            d.negate();
            return;
        }
    case DMP:
        popd(b); popd(a);
        r.mul(a, b);
        pushd(r, true);
        return;
    case DDV:
        popd(b); popd(a);
        r.div(a, b);
        pushd(r, true);
        return;
    case DSTR:      // (decval, string, numchars)
        {
            n = popw();          // max number of digits in string (passed by WRITE: 38)
            auto & ps = *ptr(pop<PTR<pstring>>()); // string
            popd(a);
            a.toSTR(ps, n);
            return;
        }
    case DCV:       // convert second tos
        popd(a);    // tuck away TOS, then do the same as DCVT
        {
            int val = popi();
            pfixeddecint<5> decval(val);
            pushd(decval, true);
        }
        pushd(a, true);
        return;
    case DECCMP:
        {
            compareop op = (compareop) popw();  // operation code
            popd(b); popd(a);
            compareinstrpush(pdecint::compar(a,b), op);
            return;
        }
    case DCVT:
        {
            int val = popi();
            pfixeddecint<5> decval(val);
            pushd(decval, true);
        }
        return;
    case DTNC:
        popd(a);
        push(a.toINT());
        return;
    default: decopscode; throwfatalerror(NOTIMP); return;
    }
}

// pop a string pointer, handling the special 1-character strings as well
const pstring * pmachine::popconststringptr(pfixedstring<2> & singletonbuf)
{
    PTR<pstring> srcaddr = pop<PTR<pstring>>(); // source: either string or char
    const pstring * str;
    if (srcaddr.offset < 256)       // it's a single-char string
    {
        // I've never actually seen this branch to be taken.
UT      singletonbuf.length = 1;
        singletonbuf.at(1) = (BYTE) srcaddr.offset;
        str = (const pstring *) &singletonbuf; // (note: charstr does not exist in emulated RAM, so don't pass the pointer to p-code)
    }
    else
    {
        validateptr(srcaddr);
        str = ptr(srcaddr);
    }
    return str;
}

// comparisons; using some templates here to make the code shorter, but with limited success
template<typename INTTYPE> int compare(INTTYPE a, INTTYPE b, size_t) { return (int) a - (int) b; }
static int compare(float a, float b, size_t) { if (a > b) return 1; else if (a < b) return -1; else return 0; }
// TODO: EQUBYT is specified for packed arrays of characters only (Apple III TRM); test this.
static int compare(BYTE * a, BYTE * b, size_t size) { return memcmp(a, b, size * sizeof(BYTE)); }   // BYTEC
static int compare(WORD * a, WORD * b, size_t size) { return memcmp(a, b, size * sizeof(WORD)); }   // WORDC

// finish the comparison: interpret the comparison result given the desired operation, push the result
__forceinline void pmachine::compareinstrpush(int compareresult, int op)
{
    switch(op)
    {
    case EQ: push(compareresult == 0); return;
    case GE: push(compareresult >= 0); return;
    case GT: push(compareresult >  0); return;
    case LE: push(compareresult <= 0); return;
    case LT: push(compareresult <  0); return;
    case NE: push(compareresult != 0); return;
    }
    validatepcode(false);
}

template<int op> void pmachine::compareinstrpush(int compareresult)
{
    // note: 'op' is a constant here, so the switch in compareinstrpush(,) resolves to a single test
    compareinstrpush(compareresult, op);
}

// execute compare operation on top-of-stack
template<typename T, int op> void pmachine::compareinstr(size_t sizearg)
{
    auto right = pop<T>();
    auto left = pop<T>();
    compareinstrpush<op>(compare(left, right, sizearg));
}

// special version for string
template<int op> void pmachine::compareinstrstr()
{
    pfixedstring<2> right1buf, left1buf;  // buffer for 1-character strings
    auto right = popconststringptr(right1buf);
    auto left = popconststringptr(left1buf);
    compareinstrpush<op>(left->compare(*right));
}

// execute compare operation on top-of-stack, for sets
template<int op> void pmachine::compareinstrset()
{
    pbitset & b = *(pbitset *) PSP;
    pbitset & a = *(pbitset *) b.next();
    void * top = a.next();      // PSP location if we popped both sets, use later for memmove()
    bool result;
    switch(op)
    {
    case EQ: result = a.isequalto(b); break;
    case GE: result = b.issubsetof(a); break;
    case GT: result = b.issubsetof(a) && !a.isequalto(b); break;    // not allowed acc. to Z80 code and Apple
    case LE: result = a.issubsetof(b); break;
    case LT: result = a.issubsetof(b) && !a.isequalto(b); break;    // not allowed acc. to Z80 code and Apple
    case NE: result = !a.isequalto(b); break;
    default: validatepcode(false);
    }
    PSP = top;
    push(result);
}

/* From the Z80 code:
; "Beware that many comparisons work only because compiler restricts you
;   to = and <> on certain types.
; The opcode tells what relation is being tested
;   the next byte indicates the type of the things being compared
;   if arrays are being compared, the next GBDE is the array size
; Tests allowed...
; Boolean:  all relations.  stuff is on the stack.
; Real:		    all relations.  stuff is on the stack.
; Set:	    =, <>, <= (subset), >= (superset).	stuff is on the stack.
; String:   all relations. pointers to stuff are on stack.
; Arrays and records:	=, <>. pointers to stuff on stack"
 */
template<int op> void pmachine::complexcompareinstr()
{
    size_t type = *IP++;
    switch(type)
    {
    case 2: return compareinstr<float,op>();            // REALC
    case 4: return compareinstrstr<op>();               // STRGC  value is pointer to pstring
    case 6: return compareinstr<bool,op>();             // BOOLC  bool (low bit of WORD)
    case 8: return compareinstrset<op>();               // POWRC  set
    case 10: return compareinstr<BYTE *,op>(*IP++);     // BYTEC  BYTE array of fixed size
    case 12: return compareinstr<WORD *,op>(*IP++);     // WORDC  WORD array of fixed size
    default: throwinvalidpcodearg();
    };
}

// execute a branch instruction
void pmachine::condbranchinstr(bool cond)
{
    SBYTE displacement = (SBYTE) *IP++;
    if (cond)
    {
        if (displacement >= 0)
            IP += displacement;
        else
        {
            auto fromIP = IP;   // for emulation below
            IP = PROC->jumpIP(displacement);    // displacement is directly the byte offset into the jump table entry from PROC
            // emulation for simple detection of dead loops and wait loops:
            //  - FOR I := 1 TO 2000 DO;    going back by 12 (unconditional)
            //  - WHILE NOT KEYPRESS DO;    going back by 10 (unconditional)
            //  - REPEAT UNTIL KEYPRESS;    going back by 7 (conditional)
            // approach:
            //  - remember last branch instruction (incl. proc calls)
            //  - see if same branch and within 12 bytes--cannot do any computation in there except through a proc call
            //  - polling from I/O must be detected in all polling functions; that is KEYPRESS, BUTTON, and PADDLE
            // This will, of course, miss more complex nested loops.
            if ((size_t) fromIP - (size_t) IP <= 12 && lastjumpedtoIP == IP)
            {
                lastjumpedtoiterations++;
                if (lastjumpedtoiterations >= 100)
                {
                    lastjumpedtoiterations = 0;
                    yield(1);       // 1 ms; probably more due to overhead; tests with GRAFDEMO made 1ms / 100 loops
                }
            }
        }
        lastjumpedtoIP = IP;
    }
}

// get a 'Big' type param (variable-length WORD constant)
WORD pmachine::bigparam()
{
    WORD param = *IP++;
    if (param & 0x80)
        param = ((param & 0x7f) << 8) + *IP++;
    return param;
}

// equivalent to the compiler's GENWORD
// which aligns the word to an even address
void pmachine::alignwordparam()
{
    // In GENWORD():
    //   IF ODD(IC) THEN IC := IC + 1;
    // i.e. the words themselves are aligned, but not the instruction
    if (intptr_t(IP) & 1)
        IP++;         // skip dummy byte
}

// get a LONG INT parameter
// Note: This does NOT necessarily come out of GENWORD.
// Note: The compiler never generates negative numbers here; instead it emits the abs value and adds a NEGI instruction.
INT pmachine::intparam()
{
    INT val = *IP++;            // low byte first
    val += (*IP++ << 8);        // then high byte
    return val;
}

// get and resolve a segment-var param (LDE, STE, LAE)
WORD & pmachine::segmentvarparam()
{
    size_t segid = *IP++;
    size_t varindex1based = bigparam();
    validatepcode(segid < _countof(segtable));
    validatepcodelogic(segtable[segid].refcount > 0);
    // access a variable in a different segment (a data segment)
    WORD * dataseg = segtable[segid].variables;     // note: variables[] is 1-based
    WORD * var = &dataseg[varindex1based];
    validatepcode(var >= (void*) segtable[segid].segbase && var < segtable[segid].segend);
    return *var;
}

// call this when we know we must be at the start of a statement
__forceinline void pmachine::assertatstatement() const
{
#ifdef _DEBUG
    //validatepcode(PSP == statementPSPs.top());     // we should be at a statement start (all elements on TOS cleared)
#endif
}

BYTE * IPs[4] = { 0 };
opcodes ops[4];
static bool singlestepping = false;

// execution
// Error handling: Some instructions may throw; run() will catch and set up call to EXECERROR, then come back here
// This will execute "PROCEDURE NUMBER TWO," that is EXECERROR.
__forceinline void pmachine::step()
{
    size_t val, segid, procid;              // some variables declared here so we avoid { }
    pstackframe * IBP;                      // BP for intermediate load/store
    int right;
    float a, b;
    void * p;

    pmemory & RAM = *this;                  // this allows us to write RAM[index] to access memory

#ifdef _DEBUG
    IPs[3] = IPs[2];
    IPs[2] = IPs[1];
    IPs[1] = IPs[0];
    IPs[0] = IP;
    ops[3] = ops[2];
    ops[2] = ops[1];
    ops[1] = ops[0];
    ops[0] = (opcodes) *IP;
#endif
    opcodes opcode = (opcodes) *IP++;
#ifdef _DEBUG
    if (singlestepping)
        sin(1.0);   // debug here
#endif
    switch (opcode) // 'ssss': search for this to find this location quickly to set a breakpoint
    {
    // constants
    case SLDCI_00: case SLDCI_01: case SLDCI_02: case SLDCI_03: case SLDCI_04: case SLDCI_05: case SLDCI_06: case SLDCI_07: case SLDCI_08: case SLDCI_09: case SLDCI_0a: case SLDCI_0b: case SLDCI_0c: case SLDCI_0d: case SLDCI_0e: case SLDCI_0f:
    case SLDCI_10: case SLDCI_11: case SLDCI_12: case SLDCI_13: case SLDCI_14: case SLDCI_15: case SLDCI_16: case SLDCI_17: case SLDCI_18: case SLDCI_19: case SLDCI_1a: case SLDCI_1b: case SLDCI_1c: case SLDCI_1d: case SLDCI_1e: case SLDCI_1f:
    case SLDCI_20: case SLDCI_21: case SLDCI_22: case SLDCI_23: case SLDCI_24: case SLDCI_25: case SLDCI_26: case SLDCI_27: case SLDCI_28: case SLDCI_29: case SLDCI_2a: case SLDCI_2b: case SLDCI_2c: case SLDCI_2d: case SLDCI_2e: case SLDCI_2f:
    case SLDCI_30: case SLDCI_31: case SLDCI_32: case SLDCI_33: case SLDCI_34: case SLDCI_35: case SLDCI_36: case SLDCI_37: case SLDCI_38: case SLDCI_39: case SLDCI_3a: case SLDCI_3b: case SLDCI_3c: case SLDCI_3d: case SLDCI_3e: case SLDCI_3f:
    case SLDCI_40: case SLDCI_41: case SLDCI_42: case SLDCI_43: case SLDCI_44: case SLDCI_45: case SLDCI_46: case SLDCI_47: case SLDCI_48: case SLDCI_49: case SLDCI_4a: case SLDCI_4b: case SLDCI_4c: case SLDCI_4d: case SLDCI_4e: case SLDCI_4f:
    case SLDCI_50: case SLDCI_51: case SLDCI_52: case SLDCI_53: case SLDCI_54: case SLDCI_55: case SLDCI_56: case SLDCI_57: case SLDCI_58: case SLDCI_59: case SLDCI_5a: case SLDCI_5b: case SLDCI_5c: case SLDCI_5d: case SLDCI_5e: case SLDCI_5f:
    case SLDCI_60: case SLDCI_61: case SLDCI_62: case SLDCI_63: case SLDCI_64: case SLDCI_65: case SLDCI_66: case SLDCI_67: case SLDCI_68: case SLDCI_69: case SLDCI_6a: case SLDCI_6b: case SLDCI_6c: case SLDCI_6d: case SLDCI_6e: case SLDCI_6f:
    case SLDCI_70: case SLDCI_71: case SLDCI_72: case SLDCI_73: case SLDCI_74: case SLDCI_75: case SLDCI_76: case SLDCI_77: case SLDCI_78: case SLDCI_79: case SLDCI_7a: case SLDCI_7b: case SLDCI_7c: case SLDCI_7d: case SLDCI_7e: case SLDCI_7f:
        push((WORD) (opcode - SLDCI_00)); return;                       // Short load constant word; the op-code itself is the value
    case LDCN: push((WORD)0); return;                                   // LOAD CONSTANT NIL
    case LDCI: pushi(intparam()); return;                               // LOAD LONG INTEGER CONSTANT; Note: param does NOT come out of GENWORD, i.e. not word-aligned
    // local vars
    case SLDL_01: case SLDL_02: case SLDL_03: case SLDL_04: case SLDL_05: case SLDL_06: case SLDL_07: case SLDL_08: case SLDL_09: case SLDL_0a: case SLDL_0b: case SLDL_0c: case SLDL_0d: case SLDL_0e: case SLDL_0f: case SLDL_10:
        push(BP->locals<WORD>(opcode - (SLDL_01-1))); return;           // Short load local word; the op-code itself is the var index
    case LDL: push(BP->locals<WORD>(bigparam())); return;               // LOAD LOCAL
    case STL: BP->locals<WORD>(bigparam()) = popw(); return;            // STORE LOCAL
    case LLA: push(addr<void>(&BP->locals<WORD>(bigparam()))); return;  // LOAD LOCAL ADDRESS
    // intermediate vars
    case LOD: IBP = getstackframenlexlevelsup(*IP++); push(IBP->locals<WORD>(bigparam())); return;       // LOAD INTERMEDIATE
    case STR: IBP = getstackframenlexlevelsup(*IP++); IBP->locals<WORD>(bigparam()) = popw(); return;    // STORE INTERMEDIATE
    case LDA: IBP = getstackframenlexlevelsup(*IP++); push(addr<void>(&IBP->locals<WORD>(bigparam()))); return;  // LOAD INTERMEDIATE ADDRESS
    // global vars
    case SLDO_01: case SLDO_02: case SLDO_03: case SLDO_04: case SLDO_05: case SLDO_06: case SLDO_07: case SLDO_08: case SLDO_09: case SLDO_0a: case SLDO_0b: case SLDO_0c: case SLDO_0d: case SLDO_0e: case SLDO_0f: case SLDO_10:
        push(GBP->locals<WORD>(opcode - (SLDO_01-1))); return;          // Short load global word - just like SLDL
    case LDO: push(GBP->locals<WORD>(bigparam())); return;              // LOAD GLOBAL
    case SRO: GBP->locals<WORD>(bigparam()) = popw(); return;           // STORE GLOBAL
    case LAO: push(addr<void>(&GBP->locals<WORD>(bigparam()))); return; // LOAD GLOBAL ADDRESS
    // external vars (global variables in a unit)
    // Note: LDO seems to have been renumbered for version II to 169 from 167 which is now LAE
    case LDE: push(segmentvarparam()); return;                          // load external
    case STE: segmentvarparam() = popw(); return;                       // store external
    case LAE: push(addr(&segmentvarparam())); return;                   // load external address
    // strings:
    case LCA:                                                           // LOAD CONSTANT (STRING) ADDRESS; data follows in code
        // TODO: rename to LSA
        // TODO (for separate code RAM): copy to frame stack, with linked list to avoid dup copying; keep list head in frame pointer
        // Doc says this pushes a BYTEPTR, but I have seen evidence that it pushes a
        // regular WORD ptr. E.g. I saw a NOP before, to align it.
        push(addr(IP));     // note: will check if IP is even (we can't take the address of an odd IP)
        IP += *IP++;        // data is embedded with leading length byte: skip it
        return;
    case SAS:   // STRING ASSIGNMENT (dest, source) where source can be a char (<256)
        {
            size_t allocsize = *IP++;                               // alloc size of target string
            pfixedstring<2> singletonbuf;                           // 1-char string constructed here
            const pstring * src = popconststringptr(singletonbuf);  // this handles the special 1-character-string pointers
            pstring * dst = pop<pstring *>();
            dst->assign(*src, allocsize);                            // (will throw in case of overflow)
            // not correct:
            //assertatstatement();            // correct? (do we ever assign inside an expression evaluation?)
            return;
        }
UT  case IXS:                                           // STRING INDEX...DYNAMIC RANGE CHECK
        {
            // 1.5 doc: "push a byte pointer formed from the integer index tos and string address. index must be in 1..current length"
            // Looking at the II.0 compiler, it's already a byte pointer.
            BYTEPTR byteptr = tos<BYTEPTR>();
            validateptr(byteptr);
            int index = byteptr.byte;
            const pstring * str = (const pstring *) ptr(byteptr.addr);
            if (index < 1 || index > (int) str->size())
                throwoutofbounds();
            return;
        }
    //case S1P: push(PACKEDPTR(pop<PTR<WORD>>(), 2, 8, 1)); // string to packed on TOS, v1.5 only
    // arrays and packed records
    case MOV: validatetosptr(); p = ptr(pop<PTR<void>>()); validatetosptr(); memcpy(ptr(pop<PTR<void>>()), p, sizeof(WORD) * bigparam()); return;  // MOVE WORDS
UT  case CHK:                                                       // CHK: CHECK INDEX OR RANGE (val, min, max), leaving val on the stack
        {
            int max = popi();
            int min = popi();
            if (tosi() < min || tosi() > max)
                throwoutofbounds();     // Z80 code reports INVNDX, not INTOVR
            return;
        }
    // note: INC seems inconsistent with Z80, which adds the address directly. Maybe 1.5 used a byte offset?
    case INC: validatetosptr(); tos<PTR<WORD>>().offset += bigparam(); return;        // INCREMENT TOS BY PARAM for indexing WORD arrays [<-- or accessing record fields??]
    // note: FILER uses index [-1]; will this work? offset is 16 bit, so probably yes
    case IXA: val = popw(); validatetosptr(); tos<PTR<WORD>>().offset += val * bigparam(); return; // INDEX ARRAY
    case IXP:                                                       // IXP: INDEX PACKED ARRAY
        // Tos is an integer index, tos-1 is the array base word pointer. UB_1 is the number of elements per word,
        // and UB_2 is the field width (in bits). Compute and push a packed field pointer
        {
            size_t index = popw();
            PTR<WORD> arr = pop<PTR<WORD>>();
            size_t elementsperword = *IP++;
            size_t fieldwidth = *IP++;
            validateptr(arr); 
            PACKEDPTR packedptr(arr, elementsperword, fieldwidth, index);
            push(packedptr);
            return;            
        }
UT  case LPA:                                                       // load word address of packed array embedded in code, like LCA
        {
            // Apple III TRM specifies this, and uses the same caching-to-data-seg as for LSA
            // Difference to LSA is that LSA has the size information inside the data to push (the str length),
            // while for LPA, the size information is an explicit bigparam().
            // Not clear how this differs at all to LSA. Maybe in that LSA also allows for pointers < 256?
            size_t size = bigparam();
            validatepcode((intptr_t(IP) & 1) == 0);
            // LSA uses push(addr(IP)), and says it tests to be even; can we use the same? TODO: change to cached-in-data-seg mode; then this gets rewritten
            pushw(addr(IP).offset);                         // array is embedded in code
            IP += size;                                     // skip to end of data
            return;
        }
    //case IXB: // IXB: INDEX BYTE ARRAY Pascal 1.5
UT  case BYT: pushw(0); return;                                             // CONVERT WORD TO BYTE ADDR (seems never generated by compiler)
    // version II uses a two-arg byte ptr that is constructed explicitly in p-code by individual loads
    case LDB: push((WORD) *byteptr(pop<BYTEPTR>())); return;                // LOAD BYTE (byteptr)
    case STB: val = popw(); *byteptr(pop<BYTEPTR>()) = (BYTE) val; return;  // STORE BYTE (byteptr, value)
    //case MVB: // MOVE BYTES Pascal 1.5
    case LDC:                                                               // LOAD MULTIWORD CONSTANT
        // doc: "UB is the number of words to load, and <block> is a word aligned block of UB words in reverse word order. Load the block onto the stack."
        {
            size_t size = *IP++;
            alignwordparam();   // the subsequent p-code words are generated by GENWORD and are thus word-aligned
            for (size_t k = 0; k < size; k++)
            {
                WORD val = *(const WORD*) IP;
                push(val);
                IP += 2;
            }
            return;
        }
    case LDM: validatetosptr(); push(ptr(pop<PTR<WORD>>()), *IP++); return; // LOAD MULTIPLE WORDS
    case STM:                                                               // STORE MULTIPLE WORDS
        {
            //  Tos is a block of UB words, tos-1 is a word pointer to a similar block. Transfer the block from the stack to the destination block
            size_t size = *IP++;
            WORD * src = (WORD*) PSP;
            const PTR<WORD> & dstaddr = tos<PTR<WORD>>(size);
            validateptr(dstaddr);
            popbytes(ptr(dstaddr), size * sizeof(WORD));
            assert(PSP == (void*) &dstaddr);
            popw();     // (dstaddr, already consumed above)
            return;
        }
    case LDP:                                                               // LOAD PACKED FIELD (packedptr)
        {
            auto packedptr = pop<PACKEDPTR>();
            validateptr(packedptr.addr);
            push(packedptr.get(*ptr(packedptr.addr)));
            return;
        }
    case STP:                                                               // STORE PACKED FIELD (packedptr, val)
        {
            val = popw();                                                   // value to write
            auto packedptr = pop<PACKEDPTR>();
            validateptr(packedptr.addr);
            packedptr.insert(*ptr(packedptr.addr), val);
            return;
        }
    // sets
    case INN:                                                               // SET INCLUSION (i, (set_a, sza)); is i in set_a ?
        {
            const pbitset & set = tos<pbitset>();
            INT & i = tosi(set.allocsize + 1);
            bool res = i >= 0 && set.testbit(i);
            PSP = &i;               // pop the set
            tosi() = res ? 1 : 0;   // implant the result instead of i
            return;
        }
    case SGS:                                                               // MAKE SINGLETON SET
        // "The integer tos is checked to ensure that 0 <= tos <= 4079, a run-time error occurring if not. The set [tos] is pushed. 
        // on the stack, it is represented by a word containing the length, and then that number of words, all of which contain valid information"
        {
            int index = popi();
            pushzeroset(index);
            tos<pbitset>().setbit(index);
            return;
        }
    case SRS:                                                               // build a subrange set
        // like SGS, but set all bits from min to max
        {
            int max = popi();
            int min = popi();
            pushzeroset(max);
            for (int k = min; k <= max; k++)
                tos<pbitset>().setbit(k);
            return;
        }
    case UNI:   // SET UNION
    case INTS:  // SET INTERSECTION
    case DIF:   // SET DIFFERENCE (a, b)  a AND not b
        {
            // TODO: move this into the pbitset class
            WORD * b = (WORD*) PSP;
            WORD * resbuf = b;                          // we do this in-place, result words go here
            size_t bsize = *b++;                        // now b points to the data
            WORD * a = b + bsize;
            size_t asize = *a++;
            WORD * top = a + asize;                     // PSP location if we popped both sets, use later for memmove()
            size_t k;
            for (k = 0; k < bsize || k < asize; k++)
            {
                WORD aword = (k < asize) ? a[k] : 0;
                WORD bword = (k < bsize) ? b[k] : 0;
                WORD rword;
                switch (opcode)
                {
                case UNI:  rword = aword | bword; break;
                case INTS: rword = aword & bword; break;
                case DIF:  rword = aword & ~bword; break;
                }
                resbuf[k] = rword;              // note that we may stomp over a, but since we go up, it's OK
            }
            // now the set lives in resbuf[0..k-1]
            // We need to move it up to 'top'.
            WORD * res = top - k;
            memmove(res, resbuf, k * sizeof(WORD));     // now it is at the correct location
            PSP = res;                                  // PSP now points to the data
            pushw(k);                                   // and close it with the size
            return;
        }
    case ADJ:   // SET ADJUST
        // "The set tos is forced to occupy UB words, either by expansion (putting zeroes “between” tos and tos-1)
        // or compression (chopping off high words of set), and its length word is discarded."
        {
            size_t targetsize = *IP++;
            pbitset & p = *(pbitset*) PSP;
            pbitset & targetp = ((pbitset*)p.next())->allocateinfront(targetsize);
            if (&p != &targetp)
                targetp.assign(p, targetsize);      // note: handles overlapping
            PSP = &targetp.bits;                    // and pop the size field
            return;
        };
    // indirection
    case SIND_0: validatetosptr(); push(RAM[popw()]); return;               // SIND0: Short index and load word, index=0 (load indirect)
    case SINDS_1: case SINDS_2: case SINDS_3: case SINDS_4: case SINDS_5: case SINDS_6: case SINDS_7:
        validatetosptr();                                                   // SIND: Short static (constant) index and load word
        push(RAM[popw() + (opcode - (SINDS_1-1))]);
        return;
    // SYSTEM.COMPILER accesses data above NP :( ; TOS=NP, *IP=10, 1:20:471
    case IND: validatetosptr(); push(RAM[popw() + *IP++]); return;          // IND: INDIRECT LOAD (constant index > 7)
    case STO: val = popw(); validatetosptr(); RAM[popw()] = val; return;    // STO: STORE INDIRECT (addr, val)
    // comparisons
    case EQUI: compareinstr<INT,EQ>(); return;                      // INTEGER EQUAL COMPARE
    case GEQI: compareinstr<INT,GE>(); return;                      // INTEGER GREATER OR EQUAL COMPARE
    case GRTI: compareinstr<INT,GT>(); return;                      // INTEGER GREATER THAN COMPARE
    case LEQI: compareinstr<INT,LE>(); return;                      // INTEGER LESS THAN OR EQUAL COMPARE
    case LESI: compareinstr<INT,LT>(); return;                      // INTEGER LESS THAN COMPARE
    case NEQI: compareinstr<INT,NE>(); return;                      // INTEGER NOT EQUAL COMPARE
    case COMPAR_EQ: complexcompareinstr<EQ>(); return;              // COMPAR: ; COMPARE COMPLEX THINGS
    case COMPAR_GE: complexcompareinstr<GE>(); return;
    case COMPAR_GT: complexcompareinstr<GT>(); return;
    case COMPAR_LE: complexcompareinstr<LE>(); return;
    case COMPAR_LT: complexcompareinstr<LT>(); return;
    case COMPAR_NE: complexcompareinstr<NE>(); return;
    // int arithmetic
    case NGI: tosi() = toINT(-tosi()); return;                          // INTEGER NEGATION
    case ABI: tosi() = toINT(abs(tosi())); return;                      // INTEGER ABSOLUTE VALUE
    case SQI: tosi() = toINT(tosi() * tosi()); return;                  // square integer
    case ADI: right = popi(); tosi() = toINT(tosi() + right); return;   // ADD INTEGER
    case SBI: right = popi(); tosi() = toINT(tosi() - right); return;   // INTEGER SUBTRACT (a, b)
    case MPI: right = popi(); tosi() = toINT(tosi() * right); return;   // INTEGER MULTIPLY
    case DVI: right = popi(); if (right == 0) throw divbyzero(); tosi() = toINT(tosi() / right); return;  // INTEGER DIVIDE (a, b)
    // bool arithmetic
    // This is to be done bitwise rather than just on bit #0, according to Apple III TRM.
    // Example where this is used (EDITOR): IF CH=CHR(HT) THEN SPACES:=8-X+ORD(ODD(X) AND ODD(248)) ELSE SPACES:=1;
    case NOT: tosi() = tosi() ^ 0xffff; return;                         // LOGICAL NOT
    case AND: right = popi(); tosi() = tosi() & right; return;          // logical AND
    case IOR: right = popi(); tosi() = tosi() | right; return;          // logical OR
    // real arithmetic and conversions
    case FLT: push((float)(popi())); return;                            // convert tos to float
    case FLO: a = popr(); push((float)(popi())); push(a); return;       // convert tos-1 to float (where tos is already float)
UT  case NGR: tosr() = -tosr(); return;                                    // REAL NEGATION
UT  case ABR: tosr() = fabs(tosr()); return;                               // abs real
    case SQR: a = tosr(); tosr() = a * a; return;                          // square real
    case ADR: b = popr(); a = tosr(); tosr() = a + b; return;              // add real
    case SBR: b = popr(); a = tosr(); tosr() = a - b; return;              // real sub
    case MPR: b = popr(); a = tosr(); tosr() = a * b; return;              // real mul
    case DVR: b = popr(); a = tosr(); if (b == 0) throw divbyzero(); tosr() = a / b; return;   // real division
    case MOD:                                                           // INTEGER REMAINDER DIVIDE (a, b)
        // TODO: get definition of MOD for negative numbers
        // Z80 code:
        // "-7 DIV 3 = -3, -7 MOD 3 = 2
	//  -6 DIV 3 = -2, -6 MOD 3 = 0
	//  NOTE WELL. Does not return values as specified in J & W."
        right = popi();
        if (right == 0) throw divbyzero();
        tosi() = toINT(tosi() % right);
        if (tosi() < 0)
            tosi() += right;        // this makes it like the Z80 code; but is it according to spec?
        return;
    // branching
    // assertatstatement() was observed to fail for FJP, UJP (CASE), and NFJ (LIBMAP)
    // and EFJ for the Miller disk
    // FJP failed assertatstatement()
    case FJP: condbranchinstr(!pop<bool>()); return;                                // BRANCH IF FALSE ON TOS
    // the CASE statement inserts UJP after loading something on the stack, so we cannot assertatstatement()
    case UJP: condbranchinstr(true); return;                                        // BRANCH UNCONDITIONAL
    case EFJ: condbranchinstr(!(popi() == popi())); return;                         // EFJ: INTEGER = THEN FJP
    case NFJ: condbranchinstr(!(popi() != popi())); return;                         // NFJ: INTEGER <> THEN FJP        
    case XJP:                                                                       // case jump
        {
            alignwordparam();   // the subsequent params come out of GENWORD and are thus word-aligned in the p-code
            int minindex = intparam();
            int maxindex = intparam();
            const RELPTR<CODE> * jumptable = (const RELPTR<CODE> *) (IP + 2);   // table follows 2 instructions for else branch
            int index = popi();
            if (index >= minindex && index <= maxindex)
                IP = jumptable[index - minindex];
            // else leave IP where it is, that's the 'else' branch
            assertatstatement();
            return;
        }
    // procedure calls
    // lex levels:
    //  -1: PROGRAM PASCALSYSTEM, that's the outer BEGIN/END, the loop in system.c.text
    //       - system globals live on this lex level, cached in GBP0
    //       - OS functions get to the system globals through intermediate access a level up (tested INITHEAP: two levels up)
    //       - outer BEGIN/END block is seg 0 proc1, with a fake lex level 0 instead of -2
    //   0: "base" OS procedures and USERPROGRAM
    //       - main PROGRAM BEGIN/END and program globals live on this lex level, cached in GBP
    //       - outer scope of this is GBP0
    //       - main program BEGIN/END accesses globals through GBP; while inner procs acess it through intermediate access
    //       - called through CBP
    //   1: "global" global procedures in USERPROGRAM
    //       - called through CGP
    //  2+: procedures defined inside USERPROGRAM
    // startup sequence:
    //  - seg 0 proc 1 lexlevel -1: PROGRAM PASCALSYSTEM
    //  - seg 4 proc 0 lexlevel 0:  INITIALIZE
    //  - seg 4 proc 7 lexlevel 1:  INITHEAP
    // types of procedures:
    //  - local: one lex level in
    //     - BP points to the procedure's local variables
    //  - intermediate: same lex level or some lex level out, but not all the way to global
    //     - static link (BP of one lex level up) is found by traversing the outerframeaddr chain out N times
    //  - global: a procedure defined inside user PROGRAM; lev lexel 1
    //     - BP->outer is the activation record that has user prog's global vars as locals, that is GBP
    //     - called through CGP, but could also be called as CIP, really
    //  - base: lex level 0 (and -1), variables defined in GLOBALS.TEXT, top-level procedures in SYSTEM e.g. FWRITEINT
    //     - BP->outer = GBP0 -> GBP is the activation record that has these as locals.
    //     - BP will be cached in GBP while inside such a proc
    //     - BP->outer is cached in GBP0
    case CBP:                                                           // CALL BASE PROCEDURE
        validatepcodelogic(ptr(GBP->outerframeaddr) == GBP0);
        enterproc(*SEG, *IP++, GBP0);
        return;
    case CGP: enterproc(*SEG, *IP++, GBP); return;                      // CALL GLOBAL PROCEDURE
    case CLP: enterproc(*SEG, *IP++, BP); return;                       // CALL LOCAL PROCEDURE
    case CIP:                                                           // CALL INTERMEDIATE PROCEDURE
        {
            procid = *IP++;
            const pproctable & proctable = *SEG;
            pstackframe * outerstackframe = getouterstackframeforcallinglexlevel(proctable[procid].lexlevel);
            enterproc(proctable, procid, outerstackframe);
            return;
        }
    case CXP:                                                           // CALL EXTERNAL PROCEDURE
        {
            segid = *IP++; procid = *IP++;          // has an extra arg: seg id
            // intercept emulated units
            auto unitemulator = segtable[segid].unitemulator;
            if (unitemulator)
            {
                if ((this->*unitemulator)(procid))  // emulate it
                {
                    lastjumpedtoIP = IP;            // break dead-loop detector
                    return;                         // was handled by emulator
                }
                // this was not handled, meaning that the emulator knows that there is a valid p-code implementation that should just be run now
            }
            // regular p-code segment
            const pproctable & proctable = getcodesegment(segid);       // page in the code if needed (not referenced yet)
            const pprocdescriptor & procdesc = proctable[procid];
            pstackframe * outerstackframe = getouterstackframeforcallinglexlevel(procdesc.lexlevel);
            enterproc(proctable, procid, outerstackframe);
            return;
        }
    case CSP: csp(); return;                                            // call special procedure
    // TODO: RBP and RNP throw on assertatstatement(), why?
    // Z80: "RBP,RNP: number of words to return (0..2)"
    case RBP: /*assertatstatement();*/ exitproc(*IP++, true); return;       // Return from base procedure (lex level 0; will -1 ever return?)
    case RNP: /*assertatstatement();*/ exitproc(*IP++, false); return;      // Return from normal procedure (incl. global procs on level 1)
    // other
    case BPT:
        {
            syscom->HLTLINE = bigparam();
            if (syscom->BUGSTATE >= 3)  // Z80 code and Miller test this; what's the definition of this variable? One value means stepping mode, another means break at BPT
            {
UT              for (size_t k = 0; k < _countof(syscom->BRKPTS); k++)
                    if (syscom->HLTLINE == syscom->BRKPTS[k])
                        throw breakpointhit();
            }
            return;
        }
UT  case XIT: throw systemexit(); return;                           // exit the system
    case NOP: return;                                               // NO OPERATION (Z80: 'BACK'), e.g. used to align string literals to WORD boundaries
    default: opcode; throwfatalerror(NOTIMP);                                  // We really shouldn't get here anymore
    }
}

static int breaksegno = 1, breakprocno = 25, breakip = 30;
static bool breakenabled = false;

// returns ms to yield (0=yield and come back immediately)
int pmachine::run()
{
    yieldrequest = -1;
startover:
    try
    {
        while(yieldrequest < 0)
        {
#ifdef _DEBUG
            auto relip = IP - PROC->entryIP();
            if (breakenabled && SEG->segmentid == breaksegno && PROC->procnum == breakprocno && relip == breakip)
            {
               singlestepping = true;
               fflush(0);
            }
            // breaking on calls and returns
            opcodes opcode = (opcodes) *IP;
            if (opcode == CBP || opcode == CGP || opcode == CLP || opcode == CIP || opcode == CXP)
                sin(1.0);
            if (opcode == RNP || opcode == RBP)
                sin(1.0);
            // some debugging facilities
            //if (PSP == statementPSPs.top())     // we reached a statement end (all elements on TOS cleared)
            {
                CODE * IP0 = PROC->entryIP();
                curRelIP = IP - IP0;
                curSEG = SEG;
                curPROC = PROC;
                curBP = BP;
                curBPUp1 = ptr(curBP->outerframeaddr);
                curBPUp2 = ptr(curBPUp1->outerframeaddr);
                curLocals = &curBP->locals<WORD>(1);
                curLocalsUp1 = &curBPUp1->locals<WORD>(1);
                curLocalsUp2 = &curBPUp2->locals<WORD>(1);
                curSystemGlobals = &GBP0->locals<WORD>(1);
                // setting breakpoints
                int bptinstr = -1;      // change this in the debugger; >= 0 means 'set breakpoint'
                if (bptinstr >= 0)
                {
                    if (bptinstr >= (int) breakpoints.size())
                        breakpoints.resize(bptinstr+1);
                    auto & bpt = breakpoints[bptinstr];
                    bpt.relIP = curRelIP;
                    bpt.segid = curSEG->segmentid;
                    bpt.procid = curPROC->procnum;
                    bpt.IP = IP;
                    bpt.lexlevel = curPROC->lexlevel;
                    bpt.SEG = curSEG;
                    bpt.PROC = curPROC;
                }
                // breaking
                for (size_t k = 0; k < breakpoints.size(); k++)
                {
                    const auto & bpt = breakpoints[k];
                    if (bpt.relIP == curRelIP && bpt.segid == curSEG->segmentid && bpt.procid == curPROC->procnum)
                        break;      // debug here
                }
            }
#endif
#if _DEBUG
            traceinst();
            tracesource();
#endif
            step();
#if _DEBUG
            if (syscom->SEGTABLE[31].CODEDESC.CODELENG)
                sin(1.0);
#endif
        }
    }
    catch (const invalidpcodeerror & e)
    {
        // kept separate so we can set a breakpoint here
        e; enterEXECERROR(xeqerror("invalid p-code encountered", NOTIMP));
        goto startover;
    }
    catch (const xeqerror & e)
    {
        // note: this also includes BPTHLT and HLT, which are not actually errors
        enterEXECERROR(e);
        goto startover;
    }
    catch (const psystemexception & e)      // unmapped exception?
    {
        e; throw;      // debug here
    }
    catch (const exception & e)             // C++ exception
    {
        // We just throw, assuming we cannot handle it. Need to make this code exception-safe.
        e; throw;      // debug here
    }
    // we only get here in case of an interrupt
    assert(yieldrequest >= 0);
    return yieldrequest;
}

// frame stack

// are we in function (segno, procno)?
// Used for special-knowledge hacks.
bool pmachine::infunction(size_t segno, size_t procno) const
{
    const pstackframe * fp = BP;
    auto thisSEG = SEG;
    auto thisPROC = PROC;
    while (fp != GBP0)
    {
        if (segno == thisSEG->segmentid && procno == thisPROC->procnum)
            return true;
        thisSEG = ptr(fp->proctableaddr);
        thisPROC = ptr(fp->procdescraddr);
        fp = ptr(fp->callerframeaddr);
    }
    return false;
}

bool pmachine::insysfunction(size_t procno) const
{
    if (procno >= _countof(systemproccallstate))
        return infunction(0, procno);
    bool in = systemproccallstate[procno] > 0;
    if (procno != systemprocedures::syspnUSERPROG && in != infunction(0, procno))
        throw logic_error("systemproccallstate[] out of sync!");
    return in;
}

bool pmachine::incodefile(const char * codefile) const
{
    const pstackframe * fp = BP;
    auto thisSEG = SEG;
    while (fp != GBP0)
    {
        if (segtable[thisSEG->segmentid].codefile == codefile)
            return true;
        validateptr(fp->callerframeaddr);
        validateptr(fp->proctableaddr);
        fp = ptr(fp->callerframeaddr);
        thisSEG = ptr(fp->proctableaddr);
    }
    return false;
}
// get the stack frame for a procedure that is n lexical levels up from the current
pstackframe * pmachine::getstackframenlexlevelsup(size_t n)
{
    pstackframe * fp = BP;
    for ( ; n > 0; n--)
    {
        validatepcodelogic(GBP0 == NILPTR/*in reset()*/ || fp != GBP0);
        fp = ptr(fp->outerframeaddr);
    }
    return fp;
}

// get the outerstackframe value for calling into a function a 'lexlevel' from where we are currently
// If we call a local procedure, we will increase the level by 1, otherwise it will decrease.
// When calling a local procedure, the outerstackframe is the current function's, that is, BP.
pstackframe * pmachine::getouterstackframeforcallinglexlevel(int lexlevel)
{
    int leveldiff = lexlevel - PROC->lexlevel;  // 1 = calling my own inner proc, possible with CXP (otherwise that would be encoded as CLP)
    validatepcodelogic(leveldiff <= 1);
    return getstackframenlexlevelsup(1 - leveldiff);
}

// check the static link chain
void pmachine::validatestaticlinks()
{
    if (GBP0 == NILPTR) // we're in reset()
        return;
    pstackframe * fp = getstackframenlexlevelsup(PROC->lexlevel+1);
    validatepcodelogic(fp == GBP0);         // we must have arrived exactly here
}

// allocate memory on the frame stack (MP, growing downwards)
template<typename T> T * pmachine::alloca(size_t size)
{
    auto cSP = (char*) SP;
    if (cSP < size + (char*) NP)
        throw framestackoverflow();
    cSP -= size;
    SP = cSP;
    return (T*) cSP;
}

#if 0
// currently unused
void pmachine::freea(size_t size)
{
    auto cSP = (char*) SP;
    if (cSP + size > (void*)GBP0)
        throw framestackoverflow();
    cSP += size;
    SP = cSP;
}
#endif

// print a horizontally centered string at given row
void pmachine::centerstring(size_t row, const char * message)
{
    size_t width = syscom->CRTINFO[1];
    size_t col = (width - strlen(message)) / 2;
    if (col < 0) col = 0;
    char gotoxy[3] = { 30/*gotoxy*/, col+32, row+32 };
    getunit(1)->write(gotoxy, 0, _countof(gotoxy), 0, 0);
    getunit(1)->write(message, 0, strlen(message), 0, 0);
}

// show our boot screen
void pmachine::bootscreen(bool firstbootmessage)
{
    resetterminal();
    // clear the screen and show welcome message; looks better in case of a soft reset
    getunit(1)->write("\x0c", 0, 1, 0, 0);
#if 0   // not showing anything, leaving it to the Pascal system; looks better (message is too quick anyway)
    if (firstbootmessage)
    {
        size_t height = 24;//syscom->CRTINFO[0];
        // Using actual height will cover the copyright message by the on-screen keyboard.
        size_t row = height * 2/5;
        size_t row2 = max(height - 4, row + 4);
        centerstring(row2,   "P-SYSTEM INTERPRETER AND MACHINE FOR WINDOWS RT COPYRIGHT 2013 FRANK SEIDE. ALL RIGHTS RESERVED.");
        // The machine is copyrighted by me; the UCSD copyright comes off the booted disk, which is appropriate.
        //centerstring(row2+1, "UCSD PASCAL COPYRIGHT 1978, 1979 REGENTS OF THE UNIVERSiTY OF CALIFORNIA. ALL RIGHTS RESERVED.  ");
        centerstring(row + 2, "WELCOME TO 1979");
        centerstring(row, "RETRO PASCAL ][");         // done last so that the cursor will stay here
    }
#endif
}

/*
;     Six easy steps toward the realization of Pascal.
;  1: initialize all I/O drivers
;  2: read directory, find 'SYSTEM.PASCAL'
;  3: read block zero and set up SEGTBL; note: that probably meant SYSCOMREC.SEGTABLE
;  4: read in segment zero
;  5: set up machine state for seg 0 proc 1
;  6: GO FOR IT.
The Apple system also seems to require to read the SYSTEM.LIBRARY, it seems not done in p-code.
 */
// To be called on the UI thread; while run() should be called on interpreter thread.
// This does not need to be called when resuming.  --BUGBUG: setup of unit emulations must be serialized; so better use an id instead of a direct pointer
// 'firstbootmessage' is 'true' for the initial boot, during which we will show a welcome message (like a ROM message during hard-boot of the computer)
bool pmachine::reset(bool firstbootmessage)
{
    try
    {
        // reset the thing
        pmemory::reset();
        PSP = cpustacktop;
        //statementPSPs.push(PSP);  // for debugging
        NP = heapbottom;
        SP = ramtop;
        BP = (pstackframe*) NILPTR;             // current proc's frame pointer
        GBP = BP;                               // and global frame pointers
        GBP0 = GBP;
        lastjumpedtoIP = (CODE*) NILPTR;
        lastjumpedtoiterations = 0;
        for (size_t k = 0; k < _countof(segtable); k++)
            segtable[k] = segtableentry();  // clear it
        // BIOS reset
        for (size_t u = 0; u < _countof(devices); u++)
        {
            if (devices[u].get())
                devices[u]->clear();
        }
        // SYSCOM
        // TODO: locate SYSCOM at address 2, since we cannot use addresses < 256 anyway (LCA forbids it)
        // If we get rid of the CPU stack, then we could literally allocate it with NEW, except that NEW checks syscom->GDIRP...
        syscom = alloca<SYSCOMREC>(sizeof (SYSCOMREC));
        memset(syscom, 0, sizeof(*syscom)); // (note: not really necessary since we just cleared the memory)
        /*
        MISCINFO: PACKED RECORD
        NOBREAK,STUPID,SLOWTERM,
        HASXYCRT,HASLCCRT,HAS8510A,HASCLOCK: BOOLEAN;
        USERKIND:(NORMAL, AQUIZ, BOOKER, PQUIZ);
        WORD_MACH, IS_FLIPT : BOOLEAN
        */
        syscom->MISCINFO =  // note: the bits are assigned backwards!
            (1 << 3)        // HASXYCRT
            + (1 << 2)      // HASLCCRT
            + (1 << 0)      // HASCLOCK
            + (1 << 10);    // WORD_MACH
        /*
        CRTINFO: PACKED RECORD
        WIDTH,HEIGHT: INTEGER;
        */
        bootscreen(firstbootmessage);       // this sets up syscom->CRTINFO[0,1] =(height,width)

        // create a dummy ABORT opcode to return to from the main procedure
        IP = alloca<CODE>(2);               // put it on the stack, above segment #0, so that we have a valid IPoffset from the GBP
        IP[0] = XIT;
        syscom->STKBASE = addr((void*)IP);
        syscom->MEMTOP = syscom->STKBASE;
        // find the system files
        // We load:
        //  1. SYSTEM.PASCAL (mandatory)
        //  2. SYSTEM.LIBRARY (optional), intrinsic units only, for Apple compatibility
        for (size_t whichfile = 1; whichfile <= 2; whichfile++)
        {
            const char * codefile = whichfile == 1 ? "SYSTEM.PASCAL" : "SYSTEM.LIBRARY";
            diskperipheral * foundondisk = nullptr;
            size_t beginblock = SIZE_MAX;       // start block for the file is filled in here
            size_t u;                           // unit
            for (u = 0; !foundondisk && u <= MAXUNIT; u++)
            {
                // TODO: use getdiskunit()
                foundondisk = dynamic_cast<diskperipheral *> (devices[u].get());
                if (foundondisk == nullptr)     // not a disk
                    continue;
                size_t sizedummy;
                auto ioresult = foundondisk->findfile(codefile, beginblock, sizedummy);
                if (!ioresult)                  // file not found
                    break;
                foundondisk = nullptr;          // file not there
            }
            if (!foundondisk)                   // no disk with the file on it found
                if (whichfile == 1)
                    return false;               // SYSTEM.PASCAL is mandatory
                else
                    break;                      // SYSTEM.LIBRARY is optional
            if (whichfile == 1)
                syscom->SYSUNIT = (WORD) u;     // remember the boot drive
            // load the segment dictionary
            // TODO: rename header to segdict and SEGTBL to SEGDICT
            SEGTBL & header = *(SEGTBL*) heapbottom;                                        // load into virgin VM memory
            auto ioresult = devices[u]->read(&header, 0, sizeof(header), beginblock, 0);    // read header block
            if (ioresult)
                return false;
            // try to guess whether this is the extended Apple header or the basic one
            bool isappleheader = false;
            for (size_t i = 0; i < _countof(header.DISKINFO) && !isappleheader; i++)
            {
                if (header.DISKINFO[i].CODELENG == 0)               // empty entry
                    continue;
                isappleheader |= header.SEGINFO[i].SEGNUM > 0 || header.SEGINFO[i].MTYPE > 0 || header.SEGINFO[i].VERSION > 0;
            }
            // set up segment table for this file
            for (size_t i = 0; i < _countof(header.DISKINFO); i++)
            {
                if (header.DISKINFO[i].CODELENG == 0)               // empty entry (acc. to Apple III Technical Reference Manual)
                    continue;
                header.DISKINFO[i].DISKADDR += beginblock;          // make abs block, as needed in SYSCOM
                // load the segment
                size_t s = isappleheader ? header.SEGINFO[i].SEGNUM : i;    // Apple headers use the mapping table
                if (whichfile == 2/*SYSTEM.LIBRARY*/ && header.SEGKIND[i] != LINKED_INTRINS && header.SEGKIND[i] != DATASEG)
                    continue;                                       // SYSTEM.LIBRARY: only register intrinsic units
                segtable[s].isSYSTEM = true;
                segtable[s].segname = string((const char*)header.SEGNAME[i].begin(), (const char*)header.SEGNAME[i].end()); // for debugging
                tracesegchanged(s);
                // regular code segments are just registered
                if ((!isappleheader && s > 0)
                    || (isappleheader && (header.SEGINFO[i].MTYPE != 0 || i > 0)))  // Apple: second half-seg is MTYPE=0; PASCALIO is, too
                {
                    if (isappleheader && header.SEGINFO[i].MTYPE == 7)  // Apple UNIT emulations of 6502 code
                    {
                        // we mark them, but leave it to the Apple shell to actually load them
                        if (segtable[s].segname == "TURTLEGR")
                            segtable[s].unitemulator = &pmachine::emulateturtlegraphics;
                        else if (segtable[s].segname == "APPLESTU")
                            segtable[s].unitemulator = &pmachine::emulateapplestuff;
                        else if (segtable[s].segname == "LONGINTI")
                            segtable[s].unitemulator = &pmachine::emulatelongintintrinsics;
                    }
                    else        // regular SYSTEM segment: set it up
                    {
                        syscom->SEGTABLE[s].CODEDESC = header.DISKINFO[i];
                        if (header.SEGKIND[i] == DATASEG)
                            syscom->SEGTABLE[s].CODEDESC.DISKADDR = 0;  // this is how we detect that it's a data segment
                        syscom->SEGTABLE[s].CODEUNIT = u;
                    }
                }
                if (s != 0)                                         // no: we just register it
                    continue;
                segtable[s].codefile = codefile;
                segtable[s].codeunit = syscom->SYSUNIT;
                // SEGNUM=0 is the system segment and will be loaded now
                if (segtable[s].segbase == NULL)
                {
                    if (!pushsegment(s, u, header.DISKINFO[i].DISKADDR, header.DISKINFO[i].CODELENG))
                        return false;
                }
                else        // special case: Apple Pascal has segment split in two
                {
                    // Apple disks have a split segment with a gap (based on reverse-engineering FixUpSeg0 in the KM system). TODO: Any better doc?
                    // This seems to be due to some memory gap in the Apple RAM (e.g. the IO area between main RAM and language card?).
                    // We don't have that gap, and thus load both segments tightly next to each other.
                    // As a consequence, the proc table entries are inaccurate and need to be updated.
                    assert(SP == segtable[s].segbase);
                    CODE * segbase = alloca<CODE>(header.DISKINFO[i].CODELENG);
                    auto ioresult = devices[u]->read(segbase, 0, header.DISKINFO[i].CODELENG, header.DISKINFO[i].DISKADDR, 0);
                    if (ioresult)
                        return false;
                    // heuristic (rev-engineered from the KM system)
                    //  - find the highest procdescriptor outside (=below) the first segment
                    //  - that would be at the supposed end of the second half in Apple RAM
                    //  - correct all entries pointing into that second half to the actual load location which has no gap
                    const void * splitsegend = nullptr;
                    const pproctable & proctable = *segtable[s].proctable;
                    for (size_t n = 1; n <= proctable.numprocedures; n++)
                    {
                        auto & procdescriptor = proctable[n];
                        if (&procdescriptor >= (void*) segtable[s].segbase)    // a valid descriptor inside the first segment
                            continue;
                        // outside: find the max over them
                        const void * splitsegendcandidate = 1 + &procdescriptor;
                        if (splitsegendcandidate > splitsegend)
                            splitsegend = splitsegendcandidate;
                    }
                    // Now we know where the Apple system expected the segment to be ending.
                    // But we actually loaded to end at segtable[s].segbase, so we need to correct the procdescriptor offsets by the difference.
                    size_t gap = intptr_t(segtable[s].segbase) - intptr_t(splitsegend);
                    // adjust all those procdescriptor pointers in the proctable upwards by this gap value
                    for (size_t n = 1; n <= proctable.numprocedures; n++)
                    {
                        auto & procdescriptor = proctable[n];
                        if (&procdescriptor >= (void*) segtable[s].segbase) // a valid descriptor inside the first segment
                            continue;
                        // outside: patch it up
                        RELPTR<pprocdescriptor> * procdescoffsets = (RELPTR<pprocdescriptor> *) &proctable;
                        procdescoffsets[-(int)n].offset -= gap;
                    }
                    // done, seal it up
                    segtable[s].segbase = segbase;  // both segments are now merged
                }
            }
        }
        if (!segtable[0].proctable)             // no segment #0 found??
            return false;
        segtable[1].isSYSTEM = false;           // USERPROG is a segment stub, not really a system segment
        SEG = segtable[0].proctable;            // set up as if we are in this segment (this fakes the ABORT code to be in here)
        segtable[SEG->segmentid].refcount++;    // reference it (as if it was loaded by CXP)
        // call S0P1(syscom)
        PROC = (pprocdescriptor *) NILPTR;      // current proc's proc table
        enterproc(*segtable[0].proctable, 1, BP);
        // trick: IP points to ABORT, which is right above segment #0; this IPoffset is valid
        // remember BP for global variables
        assert (GBP == BP);                     // this is for global calls and global variables (it can change)
        GBP0 = GBP;                             // this is for base calls (never changes)
        // post-patch, based on Z80 code:
        //  - "STAT and DYN must be self referencing"
        //  - 'SYSCOM: ^SYSCOMREC; (*MAGIC PARAM...SET UP IN BOOT*)' is in first position
        //  - UNITTABLE[7] -> 'CLIP' (formerly: 'REMIN'), initialized lazily in UCLEAR
        //  - remember some more global variables (e.g. DKVID (prefix) and THEDATE) we'd like to access as well
        GBP->outerframeaddr = addr(GBP0);
        GBP->callerframeaddr = GBP->outerframeaddr;
        GBP->callerbaseframeaddr() = GBP->outerframeaddr;
        GBP0->locals<PTR<SYSCOMREC>>(1) = addr<SYSCOMREC>(syscom);  // SYSCOM is first VAR ever in GLOBALS.TEXT, i.e. offset 1. Any other not accessed outside Pascal code
        SYVID = &GBP0->locals<pfixedstring<8>>(63);
        DKVID = &GBP0->locals<pfixedstring<8>>(59);
        // TODO: rename these two to uppercase as well
        thedate = &GBP0->locals<DATEREC>(67);
        return true;
    }
    catch(const psystemexception & e)
    {
        e; return false;   // debug here
    }
    catch(const exception & e)
    {
        e; return false;   // a C++ exception, e.g. bad_alloc
    }
}

// load a system file
// Error code will also be written to syscom->IORSLT
IORSLTWD pmachine::loadsystemfile(const char * name, vector<char> & readbuffer) // helper to get SYSTEM.CHARSET
{
    readbuffer.clear();
    size_t u = syscom->SYSUNIT;     // system unit
    // TODO: should we scan all disks?
    auto foundondisk = getdiskunit(u);
    syscom->IORSLT = IBADUNIT;      // assume failed
    if (foundondisk == nullptr)     // not a disk
        goto done;
    size_t beginblock;
    size_t size;
    syscom->IORSLT = foundondisk->findfile(name, beginblock, size);
    if (syscom->IORSLT)
        goto done;
    readbuffer.resize(size);
    syscom->IORSLT = devices[u]->read(readbuffer.data(), 0, size, beginblock, 0);
    if (syscom->IORSLT)
        readbuffer.clear();     // undo resize
done:
    traceIORSLT();
    return (IORSLTWD) syscom->IORSLT;
}


/* what EXECERROR does:
    WITH SYSCOM^,SYSCOM^.BOMBP^ DO
      BEGIN
	WRITE(OUTPUT,'S# ',MSSEG^.BYTE[0],          // these are referring to BOMBP, not MP
		   ', P# ',MSJTAB^.BYTE[0],
		   ', I# ');
	IF MISCINFO.IS_FLIPT THEN
	  WRITELN(MSIPC)
	ELSE
	  WRITELN(MSIPC - (ORD(MSJTAB) - 2 - MSJTAB^.WORD[-1]));
      END;
      BOMBP^.MSIPC := BOMBIPC;  // unless STKOVR
	  EXIT(COMMAND)
          BOMBIPC := IORESULT;  // huh?
 */
// set up call into EXECERROR from the xeqerr exception object
// EXECERROR in the end does a EXIT(COMMAND) to go back to the shell;
// and in some situations it comes right back to the crash location.
// BUGBUG: a stack overflow leads to infinite loop.
void pmachine::enterEXECERROR(const xeqerror & e)
{
    fflush(0);              // debugging (tracefile)
    forcekillmultimedia();  // force TEXTMODE so we can see an error message --TODO: this sometimes does not seem to work
    XEQERRWD xeqerr = e.xeqerr;
    // pass the error information to SYSCOM
    if (xeqerr != UIOERR)
        syscom->IORSLT = (IORSLTWD) (xeqerr + XEQERRbase);    // Apple TechNote #10
    if (xeqerr >= invalidpcode)
        xeqerr = NOTIMP;                                // so that the shell can report it
    syscom->XEQERR = xeqerr;
    // our current execution context [Apple III Technical Reference Manual; they are called differently there]
    // (TODO: but we also put this into the context of ENTERPROC)
    syscom->LASTMP = addr(BP);  // difference to BOMBP?
    syscom->SEG = addr(SEG);    // Apple III: BOMBSEG
    syscom->JTAB = addr(PROC);  // Apple III: BOMBPROC
    syscom->BOMBIPC = IP - PROC->entryIP();
    // set up all to call into EXECERROR
    size_t segid = 0;                   // EXECERROR is seg 0 proc 2
    size_t procid = 2;
    const pproctable & proctable = getcodesegment(segid);
    pstackframe * outerstackframe = getouterstackframeforcallinglexlevel(proctable[procid].lexlevel);
    if (xeqerr == STKOVR)
        PSP = cpustacktop;  // clear eval stack; EXECERROR tests first for STKOVR and terminates early
    enterproc(proctable, procid, GBP0);
    // EXECERROR accesses a BOMBP; we just use the MP of EXECERROR itself
    syscom->BOMBP = addr(BP);           // this gives EXECERROR access to seg, proc, and program counter
    // BUGBUG: ^^ according to Apple III Technical Reference Manual: "pointer to activation record of proc that caused the error"
}

// throw an xeqerror (runtime errors)
// This will be caught and turned into a call to enterEXECERROR.
void pcpu::throwruntimeerror(XEQERRWD xeqerr) const
{
    fflush(0);  // debugging (tracefile)
    abouttothrow();
    throw xeqerror("runtime error", xeqerr);
}

// throw an xeqerror (fatal errors)
// This will be caught and turned into a call to enterEXECERROR.
// Beyond this point, we actually don't distinguish between runtime and logic error,
// but it's good for debugging to keep it separate as long as we can.
void pcpu::throwfatalerror(XEQERRWD xeqerr) const
{
    fflush(0);  // debugging (tracefile)
    abouttothrow();
    throw xeqerror("fatal error", xeqerr);
}

static string logIndent;

// enter a procedure--set up its stack frame and program counter
void pmachine::enterproc(const pproctable & proctable, size_t procid, pstackframe * outerframe)
{
#ifdef _DEBUGX
    // tracing for debugging
    traceenterexitproc(proctable.segmentid, procid, true);
    dprintf("%sentering\tS #%d  p #%d\n", logIndent.c_str(), proctable.segmentid, procid);
    logIndent.push_back('.');
#endif
    const pprocdescriptor & procdesc = proctable[procid];
    if (procdesc.procnum == 0)          // assembly routines; we don't allow assembly
        throwbadprocedureid();
    validatepcode(procdesc.procnum == procid);
    validatepcode(procdesc.lexlevel <= PROC->lexlevel + 1);
    // p-code intercept
    // keep track and emulate of system procedures
    if (proctable.segmentid == 0)
        emulatesystem(procid, true);
    else if (proctable.segmentid == 1 && procid == 1)
        emulatesystem(syspnUSERPROG, true);
    // end p-code intercept
#ifdef _DEBUG
    validatestaticlinks();
#endif
    // build the stack frame
    // It consists of the stack frame itself, the parameters, and the local variables:
    //   (pframestack, function return value [functions only], passed parameters, local variables)  [Apple III Technical Reference Manual]
    // Parameters are locals(1..) procedures, and locals(3..) for functions (locals(1) = return value).
    // Questions: locals(2..) possible? So far never observed); function return value > 4 bytes possible?
    // I confirmed that function value occupies 4 bytes also for functions that return an INT or a BOOLEAN.
#ifdef _DEBUG
    size_t ds = procdesc.datasize();    // debug here
    size_t ps = procdesc.parmsize();    // bytes for parameters plus space for return values (which we unnecessarily have to copy here because we don't know how many)
#endif
    pstackframe * frame = alloca<pstackframe>(sizeof(pstackframe) + procdesc.datasize() + procdesc.parmsize());
    WORD * locals = &frame->locals<WORD>(1);
    if (procdesc.parmsize() > 0)
        popbytes(locals, procdesc.parmsize());   // parameters are first locals  --TODO: bytes or words??
    //if (procdesc.datasize() == 0)           // no data: means no return args, it's a proc, so we can assert we are at a statement boundary
    //    assertatstatement();
    // fill the stack frame
    assert(outerframe == getouterstackframeforcallinglexlevel(proctable[procid].lexlevel));
    frame->outerframeaddr = addr<pstackframe>(outerframe);
    frame->callerframeaddr = addr<pstackframe>(BP);
    frame->proctableaddr = addr<pproctable>(SEG);
    frame->procdescraddr = addr<pprocdescriptor>(PROC);
    CODE * IP0 = PROC->entryIP();
    frame->IPoffset = IP - IP0;
    if (PSP < cpustackbottom)
        throwstackoverflow();
    frame->SPoffset = intptr_t(PSP) - intptr_t(cpustackbottom);        // keep our PSP, in case we don't restore it properly (TODO: is that needed at all? EXIT?)
    // for a global procedure, there is an additional pointer that is the previous GBP, to be pushed on the stack as well
    if (procdesc.isbaseproc())
    {
        validatepcodelogic(outerframe == GBP0);
        // read as: getcallerbaseframeaddr() const { return ((const PTR<pstackframe>*)this)[-1]; }
        PTR<pstackframe> * pGBP = alloca<PTR<pstackframe>>(sizeof (PTR<pstackframe>));
        // this pushes another value onto the frame stack
        assert(pGBP == &frame->callerbaseframeaddr());
        frame->callerbaseframeaddr() = addr(GBP);   // keep it also on the (extended) stack frame
        // NOTE: Apple Pascal pushes this onto the evaluation stack instead (cf. Apple III TRM)
        GBP = frame;                                // this is the new GBP
    }
    // cross-seg call: reference the segment
    // TODO: need to understand this properly
    if (SEG->segmentid != proctable.segmentid)
        segtable[proctable.segmentid].refcount++;
    // now we have saved all that's needed for return
    // set up all those variables for the new procedure
    IP = procdesc.entryIP();
    lastjumpedtoIP = IP;
    SEG = &proctable;
    PROC = &procdesc;
    BP = frame;
    // keep track of statement-level PSP for debugging
    //statementPSPs.push(PSP);
#ifdef _DEBUG
    validatestaticlinks();
#endif
}

// RxP
// retwords = number of words to return (0..2)
void pmachine::exitproc(size_t retwords, bool isRBP)
{
#ifdef _DEBUGX
    // some tracing for debugging
    logIndent.pop_back();
    dprintf("%sexiting \tS #%d  p #%d\n", logIndent.c_str(), SEG->segmentid, PROC->procnum);
    traceenterexitproc(SEG->segmentid, PROC->procnum, false);
#endif

#ifdef _DEBUG
    validatestaticlinks();
#endif
    // p-code intercept
    // notify emulation that we are exiting (e.g. used for keeping track of segment #0 procedures)
    if (SEG->segmentid == 0)
        emulatesystem(PROC->procnum, false);
    else if (SEG->segmentid == 1 && PROC->procnum == 1)
        emulatesystem(syspnUSERPROG, false);
    // end p-code intercept
    // unroll program counter caller
    auto thisPROC = PROC;
    auto thisSEG = SEG;
    auto thisBP = BP;
    PROC = ptr(BP->procdescraddr);
    SEG = ptr(BP->proctableaddr);
    IP = PROC->entryIP() + BP->IPoffset;
    lastjumpedtoIP = IP;
    // if cross-segment return then deref the code, and unload if done
    // get result words from frame stack to CPU stack
    // Result is the first locals.
    // TODO: are ret args included in params, or counted extra? Looks like they are extra, but they are at locals(1), so they must be counted. To find out.
    if (PSP > cpustacktop)
        throwstackunderflow();
    auto prevSP = thisBP->SPoffset + (BYTE*) cpustackbottom;
    PSP = prevSP;                       // goes to before the stack frame but after code loading if newly loaded segment
#ifdef _DEBUG
    size_t ds = thisPROC->datasize();    // debug here
    size_t ps = thisPROC->parmsize();
#endif
    validatepcode(retwords * sizeof(WORD) <= thisPROC->parmsize());    // we must have at least these locals
    if (retwords > 0)
        push(&BP->locals<WORD>(1), retwords);
    // unroll stack frame to caller
    // if it's a global call then need to restore the GBP
    validatepcode(thisPROC->isbaseproc() == isRBP);
    if (isRBP)
    {
        GBP = ptr(BP->callerbaseframeaddr());
        validatepcodelogic(GBP <= GBP0 && GBP >= BP);
    }
    BP = ptr(BP->callerframeaddr);  // pop the stack frame
    SP = thisPROC->datasize() + thisPROC->parmsize() + (char*)(thisBP + 1);
    validatepcodelogic(BP >= SP);   // caller's BP must be living in the current stack
    if (thisSEG->segmentid != SEG->segmentid)
        derefsegment(thisSEG->segmentid);
    // keep track of statement-level PSP for debugging
    //statementPSPs.pop();
}

// load a code segment onto the stack
// Code just lives on the frame stack.
// TODO: detect program changes; i.e. push of segment #1. Then call a callback, e.g. to switch to TEXTMODE and notify file system on referenced code segments (may not be moved).
bool pmachine::pushsegment(size_t s, size_t u, size_t beginblock, size_t size)
{
    if (segtable[s].refcount != 0)
        throw std::logic_error("pushsegment: called for segment that is already loaded");
    auto prevSP = SP;
    CODE * segbase = alloca<CODE>(size);                // will adjust SP downwards
    segtable[s].segend = prevSP;
    segtable[s].segbase = segbase;
    if (beginblock > 0)                                 // regular code segment
    {
        syscom->IORSLT = devices[u]->read(segbase, 0, size, beginblock, 0);
        traceIORSLT();
        if (syscom->IORSLT)
            goto fail;
        segtable[s].proctable = ((pproctable*)segtable[s].segend) - 1;  // it is located at the end of the segment
        if (segtable[s].proctable->segmentid != s)
            goto fail;
        segtable[s].variables = nullptr;
    }
    else                                                // it's a data segment (Apple)
    {
        memset(segbase, 0, size);                       // Apple: data segment
        segtable[s].proctable = nullptr;                // has no proctable
        segtable[s].variables = ((WORD*)segbase) -1;    // instead we got this; note: variables[] is 1-based
    }
    // for special purposes, we try to know the file name; e.g. terminal can do code highlighting etc.
    segtable[s].codefile = dynamic_cast<const diskperipheral *>(devices[u].get())->findfilebyblock(beginblock);
    // some sanity checks
    if (segtable[s].isSYSTEM && segtable[s].codefile != "SYSTEM.PASCAL"
        && segtable[s].codefile != "SYSTEM.LIBRARY" // when pre-setting up the Apple units from SYSTEM.LIBRARY
        && segtable[s].codefile != ""                   // when loading them later
        && s != 20  // Apple SYSTEM.COMPILER uses seg 20, which also is TURTLEGRAPHICS
        )
        throw badprocedureid();     // system segment got overwritten by non-system segment
    else if (!segtable[s].isSYSTEM && segtable[s].codefile != segtable[1].codefile)
        throw badprocedureid();     // non-system segment from file different as segment 1
    // (if no GDIRP, we leave it; needed at boot time, possibly wrong at other times; we just don't want to read the DIR ourselves here)
    segtable[s].codeunit = u;
    // inform tracing
    tracesegchanged(s);
    return true;
fail:
    prevSP = SP;        // undo
    return false;
}

// load a segment if it is not in memory (but don't reference it yet)
// Problem if we want to do TOS arithmetic on main stack: This will conflict since the parameters are already on the stack.
// But then we can just copy them, it'll be fast and low mem cost compared to loading the segment.
// Note that we can CXP into our own segment.
void pmachine::pushsegment(size_t segid)
{
    // get the code (if needed) and reference it
    if (segtable[segid].refcount == 0)
    {
        if (syscom->SEGTABLE[segid].CODEDESC.DISKADDR <= 6 || syscom->SEGTABLE[segid].CODEDESC.CODELENG <= 0)   // broken file
            throwruntimeerror(NOPROC);

        dprintf("pushsegment #%2d BLK=%4d LENG=%4d from '%s'\n", segid, syscom->SEGTABLE[segid].CODEDESC.DISKADDR, syscom->SEGTABLE[segid].CODEDESC.CODELENG,
                dynamic_cast<const diskperipheral *>(devices[syscom->SEGTABLE[segid].CODEUNIT].get())->findfilebyblock(syscom->SEGTABLE[segid].CODEDESC.DISKADDR).c_str());

        if (!pushsegment(segid, syscom->SEGTABLE[segid].CODEUNIT, syscom->SEGTABLE[segid].CODEDESC.DISKADDR, syscom->SEGTABLE[segid].CODEDESC.CODELENG))
            throwruntimeerror(SYIOER);
    }
}

// get a code segment
const pproctable & pmachine::getcodesegment(size_t segid)
{
    pushsegment(segid);
    auto proctable = segtable[segid].proctable;
    if (!proctable)
        throwbadprocedureid();
    return *proctable;
}

// reference a segment; load it if not yet in memory
void pmachine::refsegment(size_t segid)
{
    pushsegment(segid);
    segtable[segid].refcount++;

    dprintf("refsegment #%2d of '%s' -> %d\n", segid, segtable[segid].codefile.c_str(), segtable[segid].refcount-1);
}

// pop code segment from frame stack
// Must be unreferenced.
void pmachine::popsegment(size_t s)
{
    dprintf("popsegment #%2d of '%s'\n", s, segtable[s].codefile.c_str());

    validatepcodelogic(segtable[s].refcount == 0);  // must be unreferenced
    validatepcodelogic(segtable[s].segbase == SP);  // stack should be back at this point
    SP = segtable[s].segend;                        // pop it off the stack
    segtable[s].proctable = NULL;                   // and clear out the setable entry
    segtable[s].segbase = NULL;
    segtable[s].segend = NULL;
    //segtable[s].segname[0] = 0;   // note: don't clear it out--when will it actually be rewritten? Can we switch to other code files?
    segtable[s].codefile[0] = 'x';  // damage it but keep it recognizable
    segtable[s].codeunit = 0;       // (but keep the codefile for debugging)
}

// deref a segment; unload if ref count is down to 0
void pmachine::derefsegment(size_t s)
{
    //dprintf("derefsegment #%2d of '%s' -> %d\n", s, segtable[s].codefile.c_str(), segtable[s].refcount-1);

    validatepcodelogic(segtable[s].refcount > 0);   // must be loaded
    segtable[s].refcount--;
    if (segtable[s].refcount == 0)
    {
        validatepcodelogic(s != 0);                 // we should never get to attempt to unload segment 0
        popsegment(s);                              // we will check in here that the SP is at the bottom of the segment
    }
    validatepcodelogic(BP >= SP);                   // caller's BP must be living in the current stack}
}

// initialization
bool pmachine::mount(const wchar_t * diskpath)  // mount disk
{
    settracefile(diskpath);
    for (size_t u = 0; u <= MAXUNIT; u++)
    {
        auto disk = dynamic_cast<class diskperipheral *> (devices[u].get());
        if (disk == nullptr)        // not a disk
            continue;
        if (disk->mounted())        // already in use
            continue;
        // mount it here
        return disk->mount(diskpath);
    }
    return false;
}

// note: this is constructed before the terminal is ready
pmachine::pmachine(iterminal & term) :
    tracefile(NULL), syscom(nullptr), thedate(nullptr),
    lastxmodeprocno(0), lastxmodesegno(0), lastxmode(0),
    FREADSTRING_S(nullptr), completepathnames(false)
{
    dprintf("test\n");
    shared_ptr<pperipheral> nulldev(createnullperipheral());
    shared_ptr<pperipheral> terminaldev(createterminalperipheral(term));
    shared_ptr<pperipheral> echoingterminaldev(createechoingterminalperipheral(terminaldev));
    // TODO: we can use devices for hack calls, such as devices[0] for system-related (e.g. free memory) and APPLESTUFF, and devices[3] for graphics
    devices[0].reset(createsystemperipheral(*this));    // SYSTEM:
    devices[1] = echoingterminaldev;                    // CONSOLE: (SYSTERM with echo of read characters)
    devices[2] = terminaldev;                           // SYSTERM:
    devices[3].reset(creategraphicsperipheral(*this));   // GRAPHIC:
    devices[4].reset(creatediskperipheral());           // disk
    devices[5].reset(creatediskperipheral());           // disk
    devices[6] = nulldev;                               // PRINTER:
    //devices[7] = nulldev;                               // REMIN: = RS-232 interface, not supported
    devices[7].reset(createclipboardperipheral());      // CLIP: (originally REMOTE: = RS-232 interface, not supported) This approach was a dead end.
    devices[8].reset(createaudioperipheral());          // AUDIO:
    devices[9].reset(creatediskperipheral());           // disk
    devices[10].reset(creatediskperipheral());          // disk
    devices[11].reset(creatediskperipheral());          // disk
    devices[12].reset(creatediskperipheral());          // disk
}

pmachine::~pmachine()
{
}

// special emulation to guard against I/O polling loops to make sure we don't drain the battery in an I/O-polling program
// Call at end of any Pascal-called instruction or function that polls, like KEYPRESS.
void pmachine::beingpolled()
{
    lastjumpedtoiterations++;
    if (lastjumpedtoiterations >= 1000)
    {
        lastjumpedtoiterations = 0;
        yield(10);
    }
}

// a special peripheral to allow some units to communicate directly with the system
// E.g. needed for our APPLESTUFF implementation.
class systemperipheral : public pperipheral
{
    pmachine & themachine;
    enum specialmodes
    {
        // note: CHAINSTUFF is unit 28 in the Apple system
        // UNITREAD/UNITWRITE modes for interaction between UNITs and system
        ASIMPLMODE = 2200,
        ASIMPLRANDOMIZE = ASIMPLMODE + 6,
        ASIMPLRANDOM = ASIMPLMODE + 7,
        SYSIMPLYIELDCPU = 25502,    // direct call to yield()
        SYSIMPLNEWARRAY = 25503,    // calloc()
        SYSIMPLRAMAVAIL = 25504,    // available DATA RAM in bytes
    };
public:
    systemperipheral(pmachine & m) : themachine (m) { }
    // We communicate from our UNIT by writing the parameters out through UNITREAD and UNITWRITE
    // with mode = 0x100 + proc number (same no as Apple), and size = #parameter bytes (for sanity checking).
    // Function return values are returned through read().
    virtual IORSLTWD write(const void * p, int offset, size_t size, int block, size_t mode)
    {
        if (mode == 0)
            return IBADMODE;
        const INT * args = (size > 0) ? (const INT *) (offset + (const char*)p) : nullptr;
        switch (mode)
        {
        case ASIMPLRANDOMIZE:
            if (size != 0)
                return IBADMODE;
#ifdef _DEBUG
            srand(0);  // have repeatable behaviour in when debugging
#else
            srand((unsigned)time(NULL));
#endif
            return INOERROR;
        case SYSIMPLYIELDCPU:
            if (size != 2 || args[0] < 0)
                return IBADMODE;
            themachine.yield(args[0]);
            return INOERROR;
        }
        return IBADMODE;
    }
    virtual IORSLTWD read(void * p, int offset, size_t size, int block, size_t mode)
    {
        if (mode == 0)
            return IBADMODE;
        INT * args = (size > 0) ? (INT *) (offset + (const char*)p) : nullptr;
        switch (mode)
        {
        case ASIMPLRANDOM:
            if (size != 2)
                return IBADMODE;
            args[0] = rand();   // note: RAND_MAX = 32767, same as specified range for RANDOM function
            return INOERROR;
        case SYSIMPLNEWARRAY:
            {
                if (size != 6)
                    return IBADMODE;
                int bytesize = args[0];
                int count = args[1];
                args[2] = themachine.NEWARRAY(count, bytesize).offset;
                return INOERROR;
            }
        case SYSIMPLRAMAVAIL:
            {
                if (size != 4)
                    return IBADMODE;
                *(float*) p = (float) themachine.RAMAVAIL();
                return INOERROR;
            }
        }
        return IBADMODE;
    }
};

systemperipheral * pmachine::createsystemperipheral(pmachine & m) { return new systemperipheral(m); }

};
