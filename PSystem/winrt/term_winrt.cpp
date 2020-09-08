// implementation of terminal for UCSD Pascal in ucsd-psystem-vm
// implements interface in term.h; i.e. replace term.c with this for building for WinRT
// Copyright (C) 2013 Frank Seide

// TODO:
//  - spaces are slightly narrower than other chars for this font  --how to fix it?
//  - better font: http://www.levien.com/type/myfonts/inconsolata.html (open-font license: http://scripts.sil.org/cms/scripts/render_download.php?format=file&media_id=OFL_plaintext&filename=OFL.txt)

#include <collection.h>
#undef min  // macro pollution from Windows header
#undef max
#include "term_winrt.h"
#include "psnapshot.h"

#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <deque>
#include <regex>
#include <set>

using namespace std;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Documents;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Media::Imaging;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::UI::Xaml::Shapes;

namespace ReTro_Pascal
{

// ---------------------------------------------------------------------------
// AutoLock
// ---------------------------------------------------------------------------

class CritSec
{
    CRITICAL_SECTION critsec;
public:
    CritSec() { InitializeCriticalSectionEx(&critsec, 0, 0); }
    ~CritSec() { DeleteCriticalSection(&critsec); } 
    void Enter() { EnterCriticalSection(&critsec); }
    void Leave() { LeaveCriticalSection(&critsec); }
};

class AutoLock
{
    CritSec & critsec;
public:
    AutoLock(CritSec & critsec) : critsec (critsec) { critsec.Enter(); }
    AutoLock(CritSec * critsec) : critsec (*critsec) { this->critsec.Enter(); }
    ~AutoLock() { critsec.Leave(); }
};

// ---------------------------------------------------------------------------
// TerminalRow
// ---------------------------------------------------------------------------

static int makeupper(int c) { if (c >= 'a' && c <= 'z') c -= 'a'-'A'; return c; }

template<typename STRING1, typename STRING2>
static bool ends_with_i(const STRING1 & s, const STRING2 & withwhat)
{
    size_t ssize = s.size();
    size_t withwhatsize = withwhat.size();
    if (ssize < withwhatsize)
        return false;
    for (size_t k = 1; k <= withwhatsize; k++)
        if (makeupper(s[ssize-k]) != makeupper(withwhat[withwhatsize-k]))
            return false;
    return true;
}

template<typename C1, typename C2>
bool begins_with(const C1 * s, const C2 * pfx, size_t slen = SIZE_MAX)
{
    for (size_t k = 0; k < slen; k++)
    {
        if (pfx[k] == 0)
            return true;
        else if (s[k] == 0)
            return false;
        else if (s[k] != pfx[k])
            return false;
    }
    return false;   // should never get here
}

static bool isinneridchar(wchar_t ch) { return ch == '_' || isalnum(ch); }
static bool isfirstidchar(wchar_t ch) { return ch == '_' || isalpha(ch); }

// note: to access this object from multiple threads, it's caller's responsibility to do locking
class TerminalRow
{
    volatile int dirty;
    wstring line;
    vector<TextClass> textClasses;
public:
    // call these from the thread that writes to the terminal and reads from it
    TerminalRow()
    {
        dirty = true;
    }
    TerminalRow(size_t width)
    {
        line.reserve(width);
        textClasses.reserve(width);
        ClearToEOL(0);
        dirty = true;
    }
    TerminalRow(const TerminalRow & other)
    {
        line = other.line;
        textClasses = other.textClasses;
        dirty = true;
    }
    void Resize(size_t width)   // resize--cut if too long
    {
        if (width < line.size())
        {
            line.resize(width);
            textClasses.resize(width);
            dirty = true;
        }
    }
    void Put(int ch, size_t col)
    {
        if (col >= line.size())     // growing the line
        {
            if (col > line.size())
                line.append(col - line.size(), ' ');    // pad with spaces
            line.push_back(ch);
            while (textClasses.size() < line.size())
                textClasses.push_back(TextClass::text);
            dirty = true;
        }
        else if (line[col] != ch || textClasses[col] != TextClass::text)   // not growing, but replacing existing character
        {
            line[col] = ch;
            textClasses[col] = TextClass::text;
            dirty = true;
        }
    }
    void ClearToEOL(size_t from)
    {
        if (from >= line.size())    // line is already shorter than requested
            return;
        dirty = true;               // actually shortening the line
        line.resize(from);
        textClasses.resize(from);
    }
    // call these from another thread under lock
    bool GetAndClearDirty() { bool d1 = (dirty != 0); dirty = false; return d1; }
    bool PeekDirty() { return dirty != 0; }
    void SetDirty() { dirty = true; }
    const wstring & GetLine() const { return line; }
    bool Empty() const { return line.empty(); }
    const vector<TextClass> & GetLineCharClasses() const { return textClasses; }
    TextClass GetLineCharClass(size_t c) const
    {
        if (c < textClasses.size())
            return textClasses[c];
        else
            return TextClass::text;
    }
    void PutLineCharClasses(const vector<TextClass> & c/*can be empty or too long*/)
    {
        for (size_t k = 0; k < line.size(); k++)
        {
            TextClass textClass = k < c.size() ? c[k] : TextClass::text;
            if (textClass != textClasses[k])
            {
                textClasses[k] = textClass;
                dirty = true;
            }
        }
    }
    void PutLineCharClass(size_t begin, size_t end, TextClass textClass)
    {
        for (size_t k = begin; k < end && k < line.size(); k++)
        {
            if (textClass != textClasses[k])
            {
                textClasses[k] = textClass;
                dirty = true;
            }
        }
    }
    wchar_t GetCharAt(size_t col)   // get what's visible here; 0 if outside the line
    {
        if (col >= line.size())
            return 0;
        else
            return line[col];
    }

    bool BeginsWith (const char16 * prefix)
    {
        return begins_with(line.c_str(), prefix);
    }
};

// ---------------------------------------------------------------------------
// TerminalContent
// ---------------------------------------------------------------------------

enum inputFIFOspecial   // non-ASCII keys are routed through input FIFO with these special codes
{
    homekey = 256,
    endkey = 257,
    pageup = 258,
    pagedown = 259,
    controlright = 260,
    controlleft = 261
};

// this class manages the terminal content (the characters) but not the drawing
class TerminalContent : public CritSec
{
private:
    size_t row, col, inrow;             // inrow used for vertical scrolling in case of occlusion
    size_t prevRow, prevCol, prevInRow; // for CursorGetDirty()
    size_t desiredWidth, desiredHeight; // size of character matrix as requested by UI
    size_t width;                       // actual size of character matrix; must be set by interpreter based on desired dimensions
    size_t height() const { return rows.size(); }
    vector<TerminalRow> rows;
    int gotoXYState;
    int DLEState;
public:
    TerminalContent()
    {
        prevRow = prevCol = prevInRow = SIZE_MAX;   // for cursor-dirty tracking
        resize(80, 24);                 // initial setup; first welcome screen may use this
        Reset();
    }
private:
    // routines for manipulating the content, without changing the cursor
    // Call these under lock

    // change the size of the terminal
    // Note: Caller must keep these in sync with SYSCOM^.CRTINFO, so Pascal programs can pick it up.
    void resize(size_t newWidth, size_t newHeight)
    {
        rows.resize(newHeight);
        for (size_t i = 0; i < rows.size(); i++)
            rows[i].Resize(newWidth);   // needed because lines may have gotten shorter
        width = newWidth;

        if (row >= height())  // force cursor to always be inside
            row = height() -1;
        if (col >= width)
            col = width -1;
        if (inrow >= height())
            inrow = height() -1;
    }
    void cleol(size_t r, size_t from)
    {
        rows[r].ClearToEOL(from);
    }
    void cleos()
    {
        cleol(row, col);
        for (size_t i = row + 1; i < height(); i++)
            cleol(i, 0);
    }
    void eraseLine(size_t firstRow)
    {
        // erase a line
        rows.erase(rows.begin() + firstRow, rows.begin() + firstRow+1);
        for (size_t i = firstRow; i < rows.size(); i++)
            rows[i].SetDirty();     // we moved
        rows.resize(rows.size()+1); // add a new line at the end
        if (inrow > firstRow)
            inrow--;
    }
    void insertLine(size_t firstRow)
    {
        // move all rows up
        rows.insert(rows.begin() + firstRow, 1);
        rows.resize(rows.size()-1);
        for (size_t i = firstRow+1; i < rows.size(); i++)
            rows[i].SetDirty();     // we moved
        if (inrow >= firstRow)
            inrow++;
        if (inrow >= height())
            inrow = height()-1;
    }
    // routines that implement cursor-position changing TermWrite()s
    void home()
    {
        col = row = inrow = 0;
    }
    void clear()
    {
        home();
        cleos();
    }
    void lf()
    {
        row++;
        while (row >= height())
        {
            eraseLine(0);   // this scrolls up
            row--;
        }
    }
    void up()
    {
        if (row > 0)
            row--;
        else
            insertLine(0);  // this scrolls down
    }
    void cr()
    {
        lf();
        col = 0;
    }
public:
    // implementation of TermXXX() functions
    // Call these from p-code interpreter thread.
    /*
    * erase line=0
    * erase to end of screen=11
    * erase screen=12
    * insert line=22
    * move cursor home=25
    * move cursor right=28
    * erase to end of line=29
    * gotoxy=30
    * move cursor up=31
    * DLE=16
    */
    // what is mode8 (mode & 0x08)? Klebsch/Miller's term.c interprets it as follows:
    //  - suppress DLE prefix (do not recognize it)
    //  - the NUL character will delete the current row (scrolling content below it up by one row)
    void TermWrite(unsigned char ch, bool mode8)
    {
        AutoLock lock(this);
        if (gotoXYState == 1)
        {
            if (ch >= 32)           // ignore out-of-bounds values
            {
                col = ch - 32;      // i.e. our valid range is 0..223
                if (col >= width)   // force cursor to always be inside
                    col = width -1;
            }
            gotoXYState = 2;
        }
        else if (gotoXYState > 1)
        {
            if (ch >= 32)
            {
                row = ch - 32;
                if (row >= height())
                    row = height() -1;
            }
            gotoXYState = 0;
        }
        else if (DLEState > 0)
        {
            DLEState = 0;
            // implement this by recursively calling TermWrite with spaces :)
            for (size_t i = 32; i < ch; i++)    // minimum number is 32, which means 0 spaces
                TermWrite(' ', mode8);
        }
        else if (ch == 0)
        {
            if (mode8)
                eraseLine(row);
        }
        else if (ch == 8)
        {
            if (col > 0)
                col--;
            else if (row > 0)
            {
                row--;
                col = width - 1;
            }
        }
        else if (ch == 10)
            lf();
        else if (ch == 11)
            cleos();
        else if (ch == 12)
            clear();
        else if (ch == 13)
            cr();
        else if (ch == 16)  // DLE prefix
        {
            if (!mode8)
                DLEState = 1;
        }
        else if (ch == 22)  // insert line
            insertLine(row);
        else if (ch == 25)
            home();
        else if (ch == 29)
            cleol(row, col);
        else if (ch == 30)
            gotoXYState = 1;
        else if (ch == 31)
            up();
        else if (ch == 28 || ch >= ' ')  // regular character
        {
            if (col >= width)
                cr();
            if (ch >= ' ')
                rows[row].Put(ch, col);
            col++;
            // special hack so we can show some version information
            const wchar_t * triggerString = L"(C) U.C. Regents 1979";
            auto len = wcslen(triggerString);
            if (rows[row].BeginsWith(triggerString) && col == len)
            {
                // we are no longer using the Klebsch/Miller interpreter
                //const char * addMessage = "\r\rP-code interpreter (C) 2000 Mario Klebsch and (C) 2010 Peter Miller\rWindows RT port (C) 2013 Frank Seide";
                const char * addMessage = "\r(C) Frank Seide 2013 (WinRT p-machine)";
                for (size_t k = 0; addMessage[k]; k++)
                    TermWrite(addMessage[k], mode8);
            }
        }
        else
            ch++;
    }
    deque<int> inputFIFO;
    // TermRead() goes through a FIFO that is fed by the event loop
    // This is the only blocking function in the system. If it blocks, it will return -1,
    // leaving it to the p-code interpreter to restart it.
    int TermRead(bool emulateHomeEnd, bool emulatePageUpDown)
    {
        AutoLock lock(this);
        inrow = row;    // we adjust the screen vertically for this if the on-screen keyboard appears
        if (inputFIFO.empty())
            return -1;
        int ch = inputFIFO.front();
        inputFIFO.pop_front();
        // special emulation of HOME and END keys and ctrl-left/right
        // These are making specific assumptions on behaviour of the default SYSTEM.EDITOR.
        size_t repeat = 1;
        if (ch == homekey || ch == endkey || ch == controlleft || ch == controlright)
        {
            if (!emulateHomeEnd)
                return -1;
            const wstring & line = GetRowString(row);
            int end = line.size();
            int begin = line.find_first_not_of(' ');
            if (begin == wstring::npos) // editor won't back up across leading spaces --BUGBUG: unless auto-indent is off, I presume?
                begin = end;
            int actualcol = col;     // actual cursor position (editor allows to move vertically outside range)
            if (actualcol > end)
                actualcol = end;
            else if (actualcol < begin)
                actualcol = begin;
            int targetcol;
            if (ch == homekey)
                targetcol = begin;
            else if (ch == endkey)
                targetcol = end;
            else if (ch == controlleft)
            {
                targetcol = actualcol;
                targetcol--;        // move at least one char (possibly backing up to prev line if at start)
                if (targetcol > begin)
                {
                    // skip trailing spaces; i.e. get onto the last non-space character
                    while (targetcol > begin && isspace(line[targetcol]))
                        targetcol--;
                    // we moved at least one and are now on the last non-space character: skip across all chars of the same kind
                    bool inword = isinneridchar(line[targetcol]);
                    while (targetcol > begin && inword == isinneridchar(line[targetcol-1]))
                        targetcol--;
                    // we are now at the first char of the same kind
                }
            }
            else if (ch == controlright)
            {
                targetcol = actualcol;
                if (targetcol < end)
                {
                    if (!isspace(line[targetcol]))      // (on space: just skip to first non-space character)
                    {
                        // skip to first character not of the same kind as the current
                        bool inword = isinneridchar(line[targetcol]);
                        do { targetcol++; }
                        while (targetcol < end && inword == isinneridchar(line[targetcol]));
                        // and now skip to first non-space
                    }
                    while (targetcol < end && isspace(line[targetcol]))
                        targetcol++;
                }
                else
                    targetcol++;        // at end: need to move one further
            }
            if (targetcol < actualcol)
            {
                ch = 8;
                repeat = actualcol - targetcol;     // wee need that many keypresses to get to the target
            }
            else
            {
                ch = 21;                // move cursor right
                repeat = targetcol - actualcol;
            }
        }
        else if (ch == pageup || ch == pagedown)
        {
            repeat = height() - 1;        // TODO: should use the actual visible height rather than what the EDITOR remembered
            if (ch == pageup)
                ch = 15;
            else
                ch = 12;
        }
        // we repeat a mapped key, for emulating home, end, page up/down
        if (repeat == 0)
            return -1;
        for (size_t k = 1; k < repeat; k++)
            PushKey(ch);
        return (unsigned char) ch;
    }
    // TermStat()
    // returns 1 if a character is available, else 0
    int TermStat(void) { AutoLock lock(this); return inputFIFO.empty() ? 0 : 1; }

    int term_width(void) { return width; }
    int term_height(void) { AutoLock lock(this); return height(); }
    int term_is_batch_mode(void) { return 0/*no*/; }

    // interfacing with WinRT control
    bool GetAndClearDirty(size_t r) { AutoLock lock(this); return rows[r].GetAndClearDirty(); }
    const wstring & GetRowString(size_t r) { AutoLock lock(this); return rows[r].GetLine(); }
    bool IsRowEmpty(size_t r) { AutoLock lock(this); return rows[r].Empty(); }
    const vector<TextClass> & GetRowCharClasses(size_t r) { AutoLock lock(this); return rows[r].GetLineCharClasses(); }
    TextClass GetRowCharClass(size_t c, size_t r) { AutoLock lock(this); return rows[r].GetLineCharClass(c); }
    void PutRowCharClasses(size_t r, const vector<TextClass> & c) { AutoLock lock(this); return rows[r].PutLineCharClasses(c); }
    void PutRowCharClass(size_t r, size_t begin, size_t end, TextClass c) { AutoLock lock(this); return rows[r].PutLineCharClass(begin, end, c); }
    bool IsAnyDirty()
    {
        AutoLock lock(this);
        for (size_t r = 0; r < rows.size(); r++)
            if (rows[r].PeekDirty())
                return true;
        return false;
    }
    bool PeekCursorDirty() /*const*/ { AutoLock lock(this); return col != prevCol || row != prevRow || inrow != prevInRow; }
    bool GetCursorDirty()               // has cursor moved since last?
    {
        AutoLock lock(this);
        bool dirty = PeekCursorDirty();
        if (dirty)
        {
            prevCol = col;
            prevRow = row;
            prevInRow = inrow;
        }
        return dirty;
    }
    int GetCol() const { return col; }  // note: possible slight race condition, cursor flicker
    int GetRow() const { return row; }
    int GetInRow() const { return inrow; }
    void GotoXY(size_t c, size_t r) { AutoLock lock(this); col = min(c, width-1); row = min(r, height()-1); }
    int GetCharAt(size_t c, size_t r) { AutoLock lock(this); return rows[r].GetCharAt(c); }
    void PutCharAt(size_t c, size_t r, int ch) { AutoLock lock(this); rows[r].Put(ch, c); }
    void PushKey(int ch)                // send in a key press from the UI
    {
        AutoLock lock(this);
        inputFIFO.push_back(ch);
    }
    void PushUndo() { PushKey(26); }    // 26=Ctrl-Z is used to return the Undo command to UREAD
    void PushRedo() { PushKey(25); }    // 25=Ctrl-Y is used to return the Redo command to UREAD
    template<class STR>
    void PushKeys(const STR & s)        // send in a sequence of key presses from the UI
    {
        AutoLock lock(this);
        inputFIFO.insert(inputFIFO.end(), s.begin(), s.end());
    }
    void term_resize(size_t WIDTH, size_t HEIGHT)  // resize() under lock, to be called by interpreter
    {
        AutoLock lock(this);
        resize(WIDTH, HEIGHT);
    }
    void InvalidateTerminal()   // force all characters to be redrawn (we had a font or color change)
    {
        AutoLock lock(this);
        for (size_t i = 0; i < height(); i++)
            rows[i].SetDirty();
    }
    void Flush()
    {
        inputFIFO.clear();
        gotoXYState = 0;
        DLEState = 0;
    }
    void Reset()                // reinitialize after resume
    {
        Flush();
        home();
    }
    void suspendresume(psnapshot * s)
    {
        AutoLock lock(this);
        size_t height;
        string content;
        vector<int> input;
        // suspending: get data out
        if (!s->resuming)
        {
            height = (int) term_height();
            content.reserve(height * (width+1));
            for (size_t r = 0; r < height; r++)
            {
                for (size_t c = 0; c < width; c++)
                    content.push_back(GetCharAt(c, r));
                content.push_back('\n');    // for validation
            }
            input.assign(inputFIFO.begin(), inputFIFO.end());
        }
        else
        {
            prevRow = prevCol = prevInRow = SIZE_MAX;   // for cursor-dirty tracking
            Reset();
        }
        // build/read data
        s->setprefix("TerminalContent:", -1);
        s->scalar("width", width);
        s->scalar("height", height);
        s->scalar("desiredWidth", desiredWidth);
        s->scalar("desiredHeight", desiredHeight);
        s->scalar("col", col);
        s->scalar("row", row);
        s->scalar("inrow", inrow);
        s->str("content", content);
        s->vec("input", input);
        // resuming: put data back in
        if (s->resuming)
        {
            resize(width, height);
            string::const_iterator p = content.begin();
            if (content.size() != height * (width+1))
                throw std::runtime_error("suspendresume: invalid 'content' field (does not match dimensions)");
            for (size_t r = 0; r < height; r++)
            {
                for (size_t c = 0; c < width; c++)
                {
                    char ch = *p++;
                    if (ch)
                        PutCharAt(c, r, ch);
                }
                if (*p++ != '\n')
                    throw std::runtime_error("suspendresume: invalid 'content' field (missing newline tag)");
            }
            InvalidateTerminal();
            inputFIFO.assign(input.begin(), input.end());
        }
    }
};

// ---------------------------------------------------------------------------
// SuggestionBox
// ---------------------------------------------------------------------------

#if 1
#define FONTNAME "Consolas"
#define letterAspectRatio (0.54982) // aspect ratio of a letter, measured by EstimateRenderedCharWidth(), av. over heights 11, 15, and 21
#else   // Lucida Console is not suitable since the character width depends on whether it is bold or non-bold
#define FONTNAME "Lucida Console"
#define letterAspectRatio (15.0 / 24.0) // aspect ratio of a letter, pixel-counted
#endif

static const set<string> & getkwset()
{
    static set<string> themap;
    if (themap.empty())
    {
        // from pcompilersupport
        themap.insert("DO");themap.insert("WITH");themap.insert("IN");themap.insert("TO");themap.insert("GOTO");themap.insert("SET");
        themap.insert("DOWNTO");themap.insert("LABEL");themap.insert("PACKED");themap.insert("END");themap.insert("CONST");themap.insert("ARRAY");
        themap.insert("UNTIL");themap.insert("TYPE");themap.insert("RECORD");themap.insert("OF");themap.insert("VAR");themap.insert("FILE");
        themap.insert("THEN");themap.insert("PROCEDURE");themap.insert("USES");themap.insert("ELSE");themap.insert("FUNCTION");themap.insert("UNIT");
        themap.insert("BEGIN");themap.insert("PROGRAM");themap.insert("INTERFACE");themap.insert("IF");themap.insert("SEGMENT");themap.insert("IMPLEMENTATION");
        themap.insert("CASE");themap.insert("FORWARD");themap.insert("EXTERNAL");themap.insert("REPEAT");themap.insert("NOT");themap.insert("OTHERWISE");
        themap.insert("WHILE");themap.insert("AND");themap.insert("DIV");themap.insert("MOD");themap.insert("FOR");themap.insert("OR");themap.insert("SEPARATE");
        // known types
        themap.insert("INTEGER");themap.insert("REAL");themap.insert("CHAR");themap.insert("BOOLEAN");themap.insert("STRING");themap.insert("TEXT");themap.insert("INTERACTIVE");
        themap.insert("INPUT");themap.insert("OUTPUT");themap.insert("KEYBOARD");themap.insert("TRUE");themap.insert("FALSE");themap.insert("NIL");themap.insert("MAXINT");
        // TODO: flag these somehow
    }
    return themap;
}

public delegate void SuggestionCallback(String^ returnedString);

// one row in the suggestion box
ref class SuggestionBoxRow sealed : public Button
{
internal:
    size_t rowIndex;    // the entry row that this button refers to
public:
    SuggestionBoxRow(size_t rowIndex) : rowIndex(rowIndex) { }
};

enum SuggestionBoxContent { none, ids, pathnames };

ref class SuggestionBox sealed : public StackPanel
{
    // sets for matching
    const set<string> & kwset;
    set<string> idset;
    // constants
    double rowHeight;
    SolidColorBrush^ background, ^selected;
    ScrollViewer^ scrollViewer;     // in case popup is too long
    // content and state
    vector<string> entries;         // what's supposed to be inside that StackPanel
    string prefix;                  // the prefix, remembered
    // UI state
    int selectedEntry;              // -1: none selected
    SuggestionBoxContent whichContent;     // what are we showing
    enum paneid { off/*not showing*/, entrystrings, punctuation };
    paneid whichPane;               // punctuation, identifiers, etc.
    vector<string> entriesInPopup;  // what's currently inside that StackPanel
    string returnedString;          // buffer for what we send back
    SuggestionCallback^ suggestionCallback; // on button-click

    // event if user clicks or taps a row like a button
    void OnRowClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
    {
        auto button = dynamic_cast<SuggestionBoxRow^> (sender);
        size_t row = button->rowIndex;
        if (row < entriesInPopup.size())
        {
            // for ids, this will behave like typing <space>
            // for pathnames, it will accept the pathname, i.e. add a <cr>
            returnedString = FormSuggestionKeys(row,whichContent == SuggestionBoxContent::ids);
            wstring ws(returnedString.begin(), returnedString.end());
            suggestionCallback(ref new String(ws.c_str()));
        }
        SetContentType(SuggestionBoxContent::none); // done with it
        Hide(); // and hide it right away
    }

    // form the key sequence to send to the input FIFO, incl. backspaces to delete wrongly cased input
    string FormSuggestionKeys(size_t row, bool wasSpace)
    {
        bool addSpace = wasSpace & (whichContent == SuggestionBoxContent::ids); // don't append space for pathnames
        string entry = entries[row];
        size_t n = entry.find((char)160);         // comments in the suggestion are spaced off by 160 (not ' '); we cut at that point
        if (n != string::npos)
            entry = entry.substr(0, n);
        // special case: do not send backspaces for = and ? (wildcards) and $ (insert same name)
        if (whichContent == SuggestionBoxContent::pathnames
            && (entry == "=" || entry == "?" || entry == "$"))
            return entry;
        // do we need to take characters away, e.g. due to case change?
        size_t ok;
        for (ok = 0; ok < prefix.size(); ok++)
            if (ok >= entry.size() || entry[ok] != prefix[ok])
                break;
        string s(prefix.size() - ok, 8);                // backspaces to remove typed characters
        string rest (entry.begin() + ok, entry.end());  // remaining characters
        // need to add/remove space?
        if (rest.find_first_of(' ') != string::npos && _stricmp(entry.c_str(), "SET OF") != 0)    // space in string (punctuation): remove if not addSpace
        {   // UGH: this SET OF test is ugly--TODO: fix by instead checking for whole non-alnum string
            // not space bar: remove all spaces
            if (!addSpace)
                for (size_t k = 0; k < rest.size(); )
                    if (rest[k] == ' ')
                        rest.erase(k,k+1);
                    else
                        k++;
            // else space bar: keep spaces as they are
        }
        else        // entry has no space: append space if is space bar
        {
            if (addSpace && isalnum(rest[rest.size()-1]))
                rest.push_back(' ');
        }
        s.append(rest); // rest of string
        // if pathnames then Enter will input the string, so add the <cr> here
        // (disabled for volume names as they usually are part of a path;
        // TODO: engine knows whether we look for vol or file names, but we don't here)
        if (whichContent == SuggestionBoxContent::pathnames && !wasSpace
            && !s.empty() && s[s.size()-1] != ':' && s[s.size()-1] != '*')
            s.push_back(13);    // will enter the string
        return s;
    }

    // each entry is a StackPanel, which we need so that we can set the background color (which is different from the button background color)
    StackPanel^ NewRow(size_t rowIndex, String^ label, FontFamily^ fontFamily, double fontSize)
    {
#if 1
        auto button = ref new SuggestionBoxRow(rowIndex);
        button->Content = label;
        button->FontFamily = fontFamily;
        button->FontSize = fontSize;
        button->Background = background;
        button->Foreground = ref new SolidColorBrush(Windows::UI::ColorHelper::FromArgb(255, 213, 213, 213));
        button->Click += ref new RoutedEventHandler(this, &SuggestionBox::OnRowClicked);
        // Shit. This gets ignored:
        button->HorizontalAlignment = Windows::UI::Xaml::HorizontalAlignment::Left;
        auto panel = ref new StackPanel();
        panel->Orientation = Windows::UI::Xaml::Controls::Orientation::Horizontal;
        panel->HorizontalAlignment = Windows::UI::Xaml::HorizontalAlignment::Left;
        panel->Children->Append(button);
        //button->IsHitTestVisible = true;
        //panel->IsHitTestVisible = true;
#else
        auto textBlock = ref new TextBlock();
        textBlock->Foreground = ref new SolidColorBrush(Windows::UI::ColorHelper::FromArgb(255, 213, 213, 213));
        textBlock->FontSize = rowHeight;
        textBlock->Height = rowHeight;
        panel->Children->Append(textBlock);
#endif
        return panel;
    }

    // transfer a new set of entries into ourselves, i.e. the StackPanel
    void UpdateVisualFromEntries(FontFamily^ fontFamily, double fontSize)
    {
        // check if any change
        // TODO: we should also detect changes in font here; punt for now
        bool changed = entries.size() != entriesInPopup.size();
        for (size_t k = 0; !changed && k < entriesInPopup.size(); k++)
            changed |= entries[k] != entriesInPopup[k];
        // a change: update the popup-menu entriesInPopup
        if (changed)
        {
            entriesInPopup = entries;       // keep the new entry set

            // changed: we need to reset our UI cursor location etc.
            ClearSelection();

            // sync to the list
            size_t maxChars = 1;            // get maximum width; we will pad with spaces
            for (size_t k = 0; k < entriesInPopup.size(); k++)
                maxChars = max(maxChars, entriesInPopup[k].size());
            Orientation = (whichPane == paneid::punctuation) ? Windows::UI::Xaml::Controls::Orientation::Horizontal : Windows::UI::Xaml::Controls::Orientation::Vertical;
            Children->Clear();
            for (size_t k = 0; k < entriesInPopup.size(); k++)
            {
                const string & entry = entriesInPopup[k];
                wstring wentry(entry.begin(), entry.end());
                wentry.append(maxChars - wentry.size(), 160);       // pad with spaces so that we get a consistent width and left alignment
                for (size_t j = 0; j < wentry.size(); j++)
                    if (wentry[j] == ' ')
                        wentry[j] = 160;        // somehow spaces don't get correct spacing
                Children->Append(NewRow(k, ref new String(wentry.c_str()), fontFamily, fontSize));
            }
            UpdateSelectionVisual();
            // put it into the popup
            if (scrollViewer)
                scrollViewer->Content = nullptr;    // clean out left-over references
            scrollViewer = nullptr;
            popup->Child = nullptr;
            // scrollviewer always has min height of 2, i.e. ugly if only one entry
            // BUGBUG: touch-scrolling the scroll viewer hides the on-screen keyboard--sigh!
            if (entriesInPopup.size() <= 2 || Orientation == Windows::UI::Xaml::Controls::Orientation::Horizontal)
            {
                popup->Child = this;                // embed ourselves in the passed popup
            }
            else
            {
                scrollViewer = ref new ScrollViewer();
                scrollViewer->Content = this;       // wrap in a scroll viewer
                scrollViewer->HorizontalScrollBarVisibility = ScrollBarVisibility::Disabled;
                scrollViewer->HorizontalScrollMode = ScrollMode::Disabled;
                scrollViewer->VerticalScrollBarVisibility = ScrollBarVisibility::Auto;
                scrollViewer->VerticalScrollMode = ScrollMode::Auto;
                scrollViewer->ManipulationMode = ManipulationModes::TranslateY | ManipulationModes::TranslateInertia;
                popup->Child = scrollViewer;        // embed ourselves in a scroll viewer, which we embed in the popup
            }
        }
    }

    // update the highlight of the selection
    void UpdateSelectionVisual()
    {
        for (size_t k = 0; k < Children->Size; k++)
        {
            auto panel = dynamic_cast<StackPanel^> (Children->GetAt(k));
            auto bg = (k == selectedEntry) ? selected : background;
            panel->Background = bg;
        }
    }

    void ClearSelection()               // note: must be callable from interpreter thread (under lock), so no UI interaction here
    {
        selectedEntry = -1;
    }
internal:
    Popup^ popup;           // the Popup we are embedded in (set this right after constructing)
    // VS displays up to 9 entries; and moves to top if no space at bottom
    // suggestionBox->Height = 40; // class will remember its font
    SuggestionBox(Popup^ popup, SuggestionCallback^ suggestionCallback) : popup(popup), suggestionCallback(suggestionCallback), kwset (getkwset())
    {
        //Margin = Thickness(10);   // being ignored
        background = ref new SolidColorBrush(Windows::UI::ColorHelper::FromArgb(255, 48, 48, 55));
        selected = ref new SolidColorBrush(Windows::UI::ColorHelper::FromArgb(255, 48, 48, 255));

        Background = background;
        rowHeight = 20;
        // add something for now
        // we force-set the height, to sidestep these nasty layout delays
        // (hey, vertically it worked last time, so maybe no need!)
        idset.insert("MAXINT");
        // the following is converted from the Pascal compiler source code
        // standard Pascal procedures and functions
        idset.insert("READ(");idset.insert("READLN");idset.insert("WRITE(");idset.insert("WRITELN");idset.insert("EOF(");idset.insert("EOLN(");
        idset.insert("PRED(");idset.insert("SUCC(");idset.insert("ORD(");idset.insert("SQR(");idset.insert("ABS(");idset.insert("NEW(");
        idset.insert("UNITREAD(");idset.insert("UNITWRITE(");idset.insert("CONCAT(");idset.insert("LENGTH(");idset.insert("INSERT(");idset.insert("DELETE(");
        idset.insert("COPY(");idset.insert("POS(");idset.insert("MOVELEFT(");idset.insert("MOVERIGHT(");idset.insert("EXIT");idset.insert("IDSEARCH(");
        idset.insert("TREESEARCH(");idset.insert("TIME(");idset.insert("FILLCHAR(");idset.insert("OPENNEW(");idset.insert("OPENOLD(");idset.insert("REWRITE(");
        idset.insert("CLOSE(");idset.insert("SEEK(");idset.insert("RESET(");idset.insert("GET(");idset.insert("PUT(");idset.insert("SCAN(");
        idset.insert("BLOCKREAD(");idset.insert("BLOCKWRITE(");idset.insert("TRUNC(");idset.insert("PAGE(");idset.insert("SIZEOF(");idset.insert("STR(");
        idset.insert("GOTOXY(");
        // other functions
        idset.insert("ODD(");idset.insert("CHR(");idset.insert("MEMAVAIL");idset.insert("ROUND(");idset.insert("SIN(");idset.insert("COS(");
        idset.insert("LOG(");idset.insert("ATAN(");idset.insert("LN(");idset.insert("EXP(");idset.insert("SQRT(");idset.insert("MARK(");
        idset.insert("RELEASE(");idset.insert("IORESULT");idset.insert("UNITBUSY(");idset.insert("PWROFTEN(");idset.insert("UNITWAIT(");idset.insert("UNITCLEAR(");
        // UI state
        SetContentType(SuggestionBoxContent::none);
    }
    SuggestionBoxContent ContentType() const { return whichContent; }
    // We disable after successful return of TermRead(), i.e. upon entering something.
    // Navigation key strokes for this suggestion box do not lead to returns, so we will stay enabled.
    // To be called from interpreter thread (under lock); thus we don't do visual update here.
    void SetContentType(SuggestionBoxContent c)
    {
        if (c == whichContent && c != SuggestionBoxContent::none)
            return;
        // reset
        whichPane = paneid::off;
        ClearSelection();
        // We could clear entries[] but that would break detecting if it changed.
        whichContent = c;
    }
    // what we could do to improve:
    //  - disallow a second id after an id
    //  - know whether we are inside a statement block vs. declaration
    //     - selects the keywords that are possible
    //  - token bigrams based on syntax diagrams
    //  - know whether we are inside a procedure/function declaration
    //  - know whether we are inside an expression (or arg list)
    // short cut: user types '?'  --how to visualize that? As a third key on the right?
    //  - most likely punctuation at this point
    //  - after id inside an expression of arg list: )    because we do have a ,
    //  - after id in declaration block:  : or =
    //  - semicolon under various situations? when?
    //  - after first id in statement (at line start or after ; or after some keywords like begin, do, repeat): :=    (if it was a procedure, it would have a ( if we have seen it)
    //  - note: for math, we are quite good already with the numeric pad
    //  - highlight function could provide that string directly
    // OR: use ? to switch between identifier list and punctuation
    //  - also invokes the suggestions if not inside an identifier--or just have it open then always?
    // keyboard controls for suggestions:
    //  - right arrow: enter highlighting, go down
    //  - back arrow: go back, back out if at top
    //  - enter and space accept selected item
    //  - esc closes it
    //  - ? toggles with special-character table
    //  - any other key
private:
    static string cap (string s, bool firstcap, bool allcaps)
    {
        for (size_t k = 0; k < s.size(); k++)
        {
            bool cap = allcaps || (k == 0 && firstcap);
            if (!cap)
                s[k] = tolower(s[k]);
        }
        return s;
    }
internal:
    const string & Prefix() const { return prefix; }
    void PopulateEntriesForPascal(const string & idPrefix, const set<string> & foundIds)
    {
        // state transition
        if (whichPane == paneid::off && idPrefix == prefix)
            return;
        else if (whichPane == paneid::off && !idPrefix.empty())         // enable if not yet
            whichPane = paneid::entrystrings;
        else if (whichPane == paneid::entrystrings && idPrefix.empty()) // ids but empty: hide
            whichPane = paneid::off;
        const size_t maxEntries = 9;            // like VS
        // build the new entries
        entries.clear();
        if (whichPane == paneid::punctuation)   // punctuation
        {
            prefix.clear();
            entries.push_back("; ");
            entries.push_back(":=");
            entries.push_back("); ");
            entries.push_back(": ");
            entries.push_back("= ");
            entries.push_back("^");
            entries.push_back("(");
            entries.push_back(")");
            entries.push_back(" [");
            entries.push_back("] ");
            entries.push_back("(* ");
            entries.push_back(" *)");
        }
        else if (whichPane == paneid::entrystrings)
        {
            prefix = idPrefix;
            // guess the capitalization for keywords
            bool firstcap = prefix.size() > 0 && isupper(idPrefix[0]);
            bool allcaps = firstcap && (prefix.size() == 1 || isupper(idPrefix[1]));
            // matching ids
            for (auto iter = foundIds.begin(); entries.size() < maxEntries && iter != foundIds.end(); iter++)
                entries.push_back(*iter);
            // filter the keywords
            for (auto iter = kwset.begin(); entries.size() < maxEntries && iter != kwset.end(); iter++)
                if (idPrefix.size() < iter->size() && _strnicmp(iter->c_str(), idPrefix.c_str(), idPrefix.size()) == 0)
                {
                    if (*iter == "ARRAY")
                        entries.push_back(cap("ARRAY [", firstcap, allcaps));
                    else if (*iter == "SET")
                        entries.push_back(cap("SET OF", firstcap, allcaps));
                    else
                        entries.push_back(cap(*iter, firstcap, allcaps));
                }
            // and filter for known Pascal procedures and functions
            for (auto iter = idset.begin(); entries.size() < maxEntries && iter != idset.end(); iter++)
                if (idPrefix.size() < iter->size() && _strnicmp(iter->c_str(), idPrefix.c_str(), idPrefix.size()) == 0)
                {
                    string capped = cap(*iter, firstcap, allcaps);
                    if (foundIds.find(capped) == foundIds.end()) // drop dups
                        entries.push_back(capped);
                }
        }
        else
            prefix.clear();
    }
    // populate from array of strings
    void PopulateEntries(string && prefix, vector<string> && completions)
    {
        // state transition
        if (whichPane == paneid::off && prefix == this->prefix && completions == this->entries)
            return;
#if 1
        whichPane = paneid::entrystrings;
#else
        if (whichPane == paneid::punctuation)   // punctuation not applicable here
            whichPane = paneid::off;
        if (whichPane == paneid::off && !prefix.empty())      // enable if not yet
            whichPane = paneid::entrystrings;
        else if (whichPane == paneid::entrystrings && prefix.empty())  // ids but empty: hide
            whichPane = paneid::off;
#endif
        this->prefix = move(prefix);
        this->entries = move(completions);
    }
    // display menu as appropriate
    // No state change in this function (except visual location)
    // If it should not be shown, hide it.
    void Show(Windows::Foundation::Rect cursor, Windows::Foundation::Rect area,
              FontFamily^ fontFamily, double fontSize)
    {
        // move 'entries' into the popup menu
        UpdateVisualFromEntries(fontFamily, fontSize);
        // position and open popup as appropriate
        if (whichContent == SuggestionBoxContent::none || whichPane == paneid::off || Children->Size == 0)
        {
            Hide();
            return;
        }
        double x = cursor.Left - 20;    // margin --TODO: how to get Button's margin correctly?
        if (x < 0)
            x = 0;
        const double vertSkip = 4;  // some vertical space between cursor and text
        double y = cursor.Bottom + vertSkip;
        popup->Measure(Size(100000,100000));
        auto w = scrollViewer ? scrollViewer->ActualWidth : ActualWidth;
        auto h = scrollViewer ? scrollViewer->ActualHeight : ActualHeight;
        if (w > 0)
        {
            // WORKAROUND
            // We keep calling into Show; the first time, this is all 0, but the second timer tick, it will be good.
            // This is when it gets positioned, actually. Thus is the insanity of WinRT.
            if (y + h > area.Bottom)                // going too far dowm
            {
                if (y < area.Bottom / 2)            // upper half: restrict the ScrollViewer; WORKAROUND: ScrollViewer does not honor the keyboard
                {
                    scrollViewer->Height = area.Bottom - y;
                }
                else                                // lower half: move viewer up
                {
                    y = cursor.Top - vertSkip - h;
                    if (y < 0)                      // off as well: better run off at the end rather the beginning
                    {
                        y = 0;
                        if (y + h > area.Bottom)    // still going too far dowm
                            scrollViewer->Height = area.Bottom - y;
                    }
                }
            }
            if (x + w > area.Width)
                x = area.Width - w;
            if (x < 0)
                x = 0;
            // also make elements maximally wide
            if (Orientation == Windows::UI::Xaml::Controls::Orientation::Vertical)
            {
                double bw = 0;
                for (size_t k = 0; k < Children->Size; k++)
                {
                    auto s = dynamic_cast<StackPanel^> (Children->GetAt(k));
                    if (!s)
                        continue;
                    auto xx = s->Children->First();
                    auto b = dynamic_cast<SuggestionBoxRow^> (s->Children->GetAt(0));
                    if (!b)
                        continue;
                    if (b->ActualWidth > bw)
                        bw = b->ActualWidth;
                }
                if (bw > 0) for (size_t k = 0; k < Children->Size; k++)
                {
                    auto s = dynamic_cast<StackPanel^> (Children->GetAt(k));
                    if (!s)
                        continue;
                    auto b = dynamic_cast<SuggestionBoxRow^> (s->Children->GetAt(0));
                    if (!b)
                        continue;
                    b->Width = bw;
                }
            }
        }
        popup->HorizontalOffset = x;
        popup->VerticalOffset = y;
        popup->IsOpen = true;
    }
    // Show() for the case of hiding it
    // Note: we hide when we successfully exit TermRead(). That will reset the UI state.
    void Hide()
    {
        ClearSelection();
        if (whichPane != paneid::off || popup->IsOpen)
        {
            popup->IsOpen = false;
            whichPane = paneid::off;
        }
    }
    // filter keyboard event
    // Returns nullptr if not handled; otherwise a string to insert
    const char * HandleKeyEvent(Windows::System::VirtualKey vkc)
    {
        if (whichContent == SuggestionBoxContent::none)
            return nullptr;
        // if not open, then we intercept '?' and right arrow to open into punctuation
        if (whichPane == paneid::off)
        {
            if (whichContent == SuggestionBoxContent::ids
                && (vkc == Windows::System::VirtualKey::Right || vkc == (Windows::System::VirtualKey) '?'))
            {
                whichPane = paneid::punctuation;
                PopulateEntriesForPascal("", set<string>());  // update it right away
                selectedEntry = 0;          // and select the first directly
                UpdateSelectionVisual();
                return "";
            }
            else
                return nullptr;
        }
        // if already open then we intercept
        //  - arrow right/up
        //  - arrow left/down
        //  - enter/space
        //  - esc
        //  - ?
        else if (vkc == Windows::System::VirtualKey::Left || vkc == Windows::System::VirtualKey::Up)
        {
            if (vkc == Windows::System::VirtualKey::Up && selectedEntry == 0)   // Up wraps, Left does not
                selectedEntry += entriesInPopup.size();
            if (selectedEntry >= 0)
            {
                selectedEntry--;
                UpdateSelectionVisual();
                return "";
            }
            // not selected: fall through (don't handle the key)
        }
        else if (vkc == Windows::System::VirtualKey::Right || vkc == Windows::System::VirtualKey::Down)
        {
            if (selectedEntry+1 < (int) entriesInPopup.size())
                selectedEntry++;
            else
                selectedEntry = 0;
            UpdateSelectionVisual();
            return "";
        }
        else if (vkc == Windows::System::VirtualKey::Enter || vkc == Windows::System::VirtualKey::Space)
        {
            if (selectedEntry >= 0)
            {
                returnedString = FormSuggestionKeys(selectedEntry, vkc == Windows::System::VirtualKey::Space);
                SetContentType(SuggestionBoxContent::none);
                Hide();
                return returnedString.c_str();
            }
        }
        else if (vkc == Windows::System::VirtualKey::Tab)
        {
            // if there is only one choice left, then select it, so that we will pick it
            if (entriesInPopup.size() == 1)
                selectedEntry = 0;
            // if there is a selection then just accept it
            if (selectedEntry >= 0)
            {
                returnedString = FormSuggestionKeys(selectedEntry, true/*like space*/);
                SetContentType(SuggestionBoxContent::none);
                Hide();
                return returnedString.c_str();
            }
            // otherwise narrow down to common prefix
            else if (whichPane == paneid::entrystrings)
            {
                size_t p;
                string s;
                for (p = 0; ; p++)
                {
                    if (entriesInPopup[0].size() == p)
                        goto end;
                    for (size_t k = 1; k < entriesInPopup.size(); k++)
                    {
                        if (entriesInPopup[k].size() == p || toupper((unsigned char)entriesInPopup[k][p]) != toupper((unsigned char)entriesInPopup[0][p]))
                            goto end;
                    }
                    if (p >= prefix.size())  // append any extra character
                        s.push_back(entriesInPopup[0][p]);
                }
            end:
                if (!s.empty())  // there was an advancement
                {
                    // append chars to prefix and emit them
                    prefix += s;
                    returnedString = s;
                    return returnedString.c_str();
                }
            }
            // in any case, absorb it
            return "";
        }
        else if (vkc == Windows::System::VirtualKey::Escape)
        {
            //whichPane = paneid::suppress;   // we keep whichContent but suppress its display
            Hide();
            //whichPane = paneid::suppress;   // we keep whichContent but suppress its display
            return "";
        }
        else if (whichContent == SuggestionBoxContent::ids  // toggle to punctuation pane
                 && vkc == (Windows::System::VirtualKey) '?')
        {
            if (whichPane == paneid::entrystrings)
                whichPane = paneid::punctuation;
            else
                whichPane = paneid::entrystrings;
            // we expect PopulateXXX() to be called again
            // TODO: that's not nice. Better we remember both layers and switch ourselves. Later...
            return "";
        }
        return nullptr;
    }
};

// ---------------------------------------------------------------------------
// TextDisplay
// ---------------------------------------------------------------------------

#define TEXTMARGIN 10.0                                             // margin around text display
#define MENUOFFSET (TEXTMARGIN/3)

struct TextSegment  // one stretch of characters with the same font properties
{
    TextClass textClass;      // what to render it as
    wstring text;               // the characters
    TextSegment(TextClass c, wstring && t) : textClass(c), text(t) {}
    TextSegment() : textClass(TextClass::text) {}
};

// Note: This class is being factored out from TermCanvas, and not really a nice
// class so far since TermCanvas code parties on it at will.
ref class TextDisplay sealed : public Canvas
{
internal:
    FontFamily^ textFont;       // font is also used by suggestion box; we own it
    StackPanel^ textCursor;     // the cursor (stack panel so it can have a background)
    SolidColorBrush^ textBrushes[TextClass::numClasses];

internal:   // constructors
    TextDisplay()
    {
        textFont = ref new Windows::UI::Xaml::Media::FontFamily(FONTNAME);
        // text cursor
        textCursor = NewTextRowStackPanel();
        textCursor->Children->Append(NewTextBlock(false, false));
        // last child is always the textCursor
        AddTextCursorToChildren();
        // better: SetNumRows(0), and recreate all rows there
        topRowMenuOffset = 0;
    }
    
    void SetColorScheme(TerminalColorScheme scheme)
    {
        switch (scheme)
        {
        case TerminalColorScheme::Amber:
            textBrushes[TextClass::background] = ref new SolidColorBrush(Windows::UI::Colors::Black);
            //textBrushes[TextClass::text] = ref new SolidColorBrush(Windows::UI::Colors::Gold);
            textBrushes[TextClass::text] = ref new SolidColorBrush(Windows::UI::ColorHelper::FromArgb(255, 254, 224, 2));   // measured from a photo
            textBrushes[TextClass::keyword] = ref new SolidColorBrush(Windows::UI::Colors::LightBlue);
            textBrushes[TextClass::knowntype] = ref new SolidColorBrush(Windows::UI::Colors::LightSalmon);
            textBrushes[TextClass::identifier] = ref new SolidColorBrush(Windows::UI::Colors::LightCyan);
            textBrushes[TextClass::stringliteral] = ref new SolidColorBrush(Windows::UI::Colors::Goldenrod);
            textBrushes[TextClass::comment] = ref new SolidColorBrush(Windows::UI::Colors::LightGreen);
            textBrushes[TextClass::link] = ref new SolidColorBrush(Windows::UI::Colors::Goldenrod);
            break;
        case TerminalColorScheme::Green:
            textBrushes[TextClass::background] = ref new SolidColorBrush(Windows::UI::Colors::Black);
            textBrushes[TextClass::text] = ref new SolidColorBrush(Windows::UI::ColorHelper::FromArgb(255, 0, 255, 0));
            textBrushes[TextClass::keyword] = ref new SolidColorBrush(Windows::UI::Colors::Blue);
            textBrushes[TextClass::knowntype] = ref new SolidColorBrush(Windows::UI::Colors::LightSalmon);
            textBrushes[TextClass::identifier] = ref new SolidColorBrush(Windows::UI::Colors::LightCyan);
            textBrushes[TextClass::stringliteral] = ref new SolidColorBrush(Windows::UI::Colors::DarkGreen);
            textBrushes[TextClass::comment] = ref new SolidColorBrush(Windows::UI::Colors::Goldenrod);
            textBrushes[TextClass::link] = ref new SolidColorBrush(Windows::UI::Colors::DarkGreen);
            break;
        case TerminalColorScheme::Paperwhite:
            textBrushes[TextClass::background] = ref new SolidColorBrush(Windows::UI::Colors::White);
            textBrushes[TextClass::text] = ref new SolidColorBrush(Windows::UI::Colors::Black);
            textBrushes[TextClass::keyword] = ref new SolidColorBrush(Windows::UI::Colors::DarkBlue);
            textBrushes[TextClass::knowntype] = ref new SolidColorBrush(Windows::UI::Colors::DarkSalmon);
            textBrushes[TextClass::identifier] = ref new SolidColorBrush(Windows::UI::Colors::DarkCyan);
            textBrushes[TextClass::stringliteral] = ref new SolidColorBrush(Windows::UI::Colors::SaddleBrown);
            textBrushes[TextClass::comment] = ref new SolidColorBrush(Windows::UI::Colors::DarkGoldenrod);
            textBrushes[TextClass::link] = ref new SolidColorBrush(Windows::UI::Colors::DarkBlue);
            break;
        }
    }

internal:   // construction functions (convenience helpers) for our UI elements
    // create a new text block for text display
    TextBlock^ NewTextBlock(bool isRow0, bool isLeftMost)
    {
        auto textBlock = ref new TextBlock();
        // how to set fixed-width font?
        textBlock->FontFamily = textFont;
        textBlock->TextWrapping = TextWrapping::NoWrap;   // (needed? does it do anything?)
        if (isLeftMost || isRow0)
            textBlock->Margin = Windows::UI::Xaml::Thickness(isLeftMost ? TEXTMARGIN : 0, isRow0 ? TEXTMARGIN : 0, 0, 0);
        // the following has no effect: why?
        //textBlock->FontStretch = Windows::UI::Text::FontStretch::Expanded;    // Expanded--this does nothing
        //textBlock->IsHitTestVisible = false;  // will not get focus from mouse click
        return textBlock;
    }

    static StackPanel^ NewTextRowStackPanel()
    {
        auto panel = ref new StackPanel();
        panel->Orientation = Orientation::Horizontal;
        //panel->IsHitTestVisible = false;    // will not get focus from mouse click; focus shall remain on inputTextBox
        return panel;
    }

internal:   // row access
    StackPanel^ GetTextRowStackPanel(size_t row)
    {
        // note: assumes the first N entries to be the text-row panels
        auto rowElement = Children->GetAt(row);
        return dynamic_cast<StackPanel^> (rowElement);
    }

    // textCursor is always last child
    void AddTextCursorToChildren()
    {
        Children->Append(textCursor);
        Canvas::SetZIndex(textCursor, 2);
    }

    size_t GetNumRows() //const
    {
        return Children->Size-1;    // assumes last child is cursor
    }

    // make sure the vertical StackPanel has the correct number of rows
    void SetNumRows(size_t height)
    {
        // note: assumes the first N entries to be the text-row panels
        // TODO: Cursor is last, but now that we have a proper class, there is no need.
        //       Also, height does not change all the time, so we can just rebuild.
        //       But then we'd need to double-check the dirty control to have existing rows redone.
        if (Children->Size != height)
        {
            // last element is the cursor
            //auto textCursor = Children->GetAt(Children->Size-1);
            Children->RemoveAtEnd();           // pop textCursor for resizing
            // adjust the size
            while (Children->Size > height)
                Children->RemoveAtEnd();
            while (Children->Size < height)
                Children->Append(NewTextRowStackPanel());
            // put cursor back; must always be the last element
            AddTextCursorToChildren();
        }
    }

    // is this row a newly created one (created in SetNumRows())?
    bool IsVirginRow(size_t i)
    {
        auto panelElements = GetTextRowStackPanel(i)->Children;
        return panelElements->Size == 0/*new*/;
    }

internal:   // update width and font
    void UpdateRowVisuals(Windows::UI::Text::FontWeight currentFontWeight, double renderedRowHeight, double canvasWidth)
    {
        for (size_t i = 0; i < Children->Size; i++)
        {
            // adjust the stack panel width to maximum width
            if (i < Children->Size-1)   // last one is the cursor
                GetTextRowStackPanel(i)->Width = canvasWidth;//, textWidth + 2 * TEXTMARGIN);
            // adjust the font size--must do that even if no change to the text
            auto panelElements = GetTextRowStackPanel(i)->Children;
            for (size_t k = 0; k < panelElements->Size; k++)
            {
                auto textBlock = dynamic_cast<TextBlock^> (panelElements->GetAt(k));
                //if (textBlock == nullptr)
                //    textBlock = dynamic_cast<TextBlock^> ((dynamic_cast<Button^> (panelElements->GetAt(k)))->Content);
                //if ((int) textBlock->FontWeight != (int) currentFontWeight)   // how to compare these?
                    textBlock->FontWeight = currentFontWeight;
                if (textBlock->FontSize != renderedRowHeight)         // without this test, the screen sometimes goes dark--win bug!
                    textBlock->FontSize = renderedRowHeight;
            }
        }
    }

internal:   // support of mouse-click/tap on text
    // get full text of a row
    wstring GetRowText(size_t row)
    {
        auto panelElements = GetTextRowStackPanel(row)->Children;
        wstring text;
        for (size_t k = 0; k < panelElements->Size; k++)
        {
            auto textBlock = dynamic_cast<TextBlock^> (panelElements->GetAt(k));
            //if (textBlock == nullptr)
            //    textBlock = dynamic_cast<TextBlock^> ((dynamic_cast<Button^> (panelElements->GetAt(k)))->Content);
            text += textBlock->Text->Data();
        }
        return text;
    }

    // get coordinate of bottom-left corner of a row, for finding a mouse click
    // Coordinates are relative to 'this', and *not* the outer containing TermCanvas.
    Point GetRowBottomLeft(size_t row)
    {
        auto panel = GetTextRowStackPanel(row);
        auto h = panel->ActualHeight;       // in case of scaling, this is the unscaled height
        auto rowBottomLeft = panel->TransformToVisual(this)->TransformPoint(Point(0,(float)h));
        return rowBottomLeft;
    }

    Point GetRowTopLeft(size_t row)
    {
        auto panel = GetTextRowStackPanel(row);
        auto rowBottomLeft = panel->TransformToVisual(this)->TransformPoint(Point(0,0));
        return rowBottomLeft;
    }

internal:   // positioning
    void SetRowTop(size_t i, double y)
    {
        Canvas::SetTop(GetTextRowStackPanel(i), y);
    }

internal:   // display
    // this version renders different colors in a single TextBlock as runs
    void RenderSegmentsAsTextRuns(size_t i, const vector<TextSegment> & rowSegments, bool topRowIsMenu, bool revMenuEnabled)
    {
        auto panelElements = GetTextRowStackPanel(i)->Children;
        if (panelElements->Size == 0)
            panelElements->Append(NewTextBlock(i == 0, true));
        auto textBlock = dynamic_cast<TextBlock^> (panelElements->GetAt(0));
        auto inlines = textBlock->Inlines;
        inlines->Clear();
        for (size_t k = 0; k < rowSegments.size(); k++)
        {
            const auto & seg = rowSegments[k];
            auto run = ref new Windows::UI::Xaml::Documents::Run();
            run->Text = ref new String(seg.text.data(), seg.text.size());
            if (i == 0 && topRowIsMenu && revMenuEnabled)
                run->Foreground = textBrushes[TextClass::background];
            else
                run->Foreground = textBrushes[seg.textClass];
            inlines->Append(run);
        }
    }

    // this version renders segments as multiple text blocks in the StackPanel
    // This is known to have visible compounding round-off errors as color switch boundaries.
    void RenderSegmentsAsTextBlocks(size_t i, const vector<TextSegment> & rowSegments, bool topRowIsMenu, bool revMenuEnabled)
    {
        auto panelElements = GetTextRowStackPanel(i)->Children;
        while (panelElements->Size > rowSegments.size())    // create correct set of elements
            panelElements->RemoveAtEnd();
        while (panelElements->Size < rowSegments.size())
            panelElements->Append(NewTextBlock(i == 0, panelElements->Size == 0));
        for (size_t k = 0; k < rowSegments.size(); k++)
        {
            const auto & seg = rowSegments[k];
            auto textBlock = dynamic_cast<TextBlock^> (panelElements->GetAt(k));
            textBlock->Text = ref new String(seg.text.c_str());
            if (i == 0 && topRowIsMenu && revMenuEnabled)
                textBlock->Foreground = textBrushes[TextClass::background];
            else
                textBlock->Foreground = textBrushes[seg.textClass];
        }
    }

internal:   // cursor display
    wchar_t GetCharAt(size_t col, size_t row)
    {
        if (row >= GetNumRows())
            return 160;
        auto line = GetRowText(row);    // (inefficient; only need the char)
        if (col >= line.size())
            return 160;
        wchar_t ch = line[col];
        if (ch == 32)
            return 160;
        return ch;
    }

    // estimate rendered char width
    // Call this before calling SetCursor(), or doing anything based on renderedCharWidth.
    // This is so terrible! Why is there no proper measure function?? WORKAROUND
    double EstimateRenderedCharWidth(double deflt)
    {
        // estimate on longest text range
        TextBlock^ longest;
        size_t longestLen = 0;
        for (size_t row = 0; row < GetNumRows(); row++)
        {
            auto panelElements = GetTextRowStackPanel(row)->Children;
            for (size_t k = 0; k < panelElements->Size; k++)
            {
                auto textBlock = dynamic_cast<TextBlock^> (panelElements->GetAt(k));
                auto text = textBlock->Text;
                size_t n = text->Length();
                if (n > longestLen)
                {
                    longest = textBlock;
                    longestLen = n;
                }
            }
        }
        if (!longest)
            return deflt;
        // measure its width
        auto pos = longest->ContentEnd->GetPositionAtOffset(0, Windows::UI::Xaml::Documents::LogicalDirection::Forward);
        if (!pos)
            return deflt;
        auto rect = pos->GetCharacterRect(Windows::UI::Xaml::Documents::LogicalDirection::Forward);
        if (rect.X == 0.0)
            return deflt;
        return rect.X / longestLen;
    }

    // determine the top-left coordinates of a character
    Point CharLocation(size_t col, size_t row, bool topIsCommandBar, double renderedCharWidth)
    {
        if (row >= GetNumRows())    // oops?
            return Point(0,0);
        // get vertical position from the actual text row the cursor is in
        auto point = GetRowTopLeft(row);
        // horizontal position from estimated renderedCharWidth (call Estimate... function first)
        point.X += (float) renderedCharWidth * col;
        // margins and adjustments
        point.X += TEXTMARGIN;
        if (row == 0)
        {
            point.Y += (float) TEXTMARGIN;         // top row has a margin so we have space to shift for the menu
            point.Y += (float) topRowMenuOffset;   // set in SetTopRow()
        }
        return point;
    }

    // position the text cursor
    // We return the X coordinate.
    // Y can be gotten from the row (for suggestion box), but it will be off for a command bar
    double SetCursor(size_t col, size_t row, bool topIsCommandBar, bool isRowInverted, double renderedCharWidth)
    {
        Point cursorPos = CharLocation(col, row, topIsCommandBar, renderedCharWidth);
        textCursor->Background = isRowInverted ? textBrushes[TextClass::background] : textBrushes[TextClass::text];
        auto cursorTextBlock = dynamic_cast<TextBlock^> (textCursor->Children->GetAt(0));
        wchar_t cursorchar[] = { GetCharAt(col, row), 0 };
        cursorTextBlock->Text = ref new String(cursorchar);
        cursorTextBlock->Foreground = isRowInverted ? textBrushes[TextClass::text] : textBrushes[TextClass::background];
        Canvas::SetLeft(textCursor, cursorPos.X);
        Canvas::SetTop(textCursor, cursorPos.Y);
        return cursorPos.X;
    }

internal:   // support of command-bar visual highlighting
    // set position and color of top row
    // If it is a command bar, then adjust it a little up.
    // If reverse video for it is enabled, then do that, too.
    double topRowMenuOffset;
    void SetTopRow(bool topRowIsMenu, bool revMenuEnabled)
    {
        // update background color of top-most row
        Canvas::SetZIndex(GetTextRowStackPanel(0), 1);  // make sure it's on front of lower rows that might scroll over it
        GetTextRowStackPanel(0)->Background = (topRowIsMenu && revMenuEnabled) ? textBrushes[TextClass::text] : textBrushes[TextClass::background];

        // and vertically adjust the top row up just a little if it is a menu, to make it look like one
        topRowMenuOffset = topRowIsMenu ? -MENUOFFSET : 0;
        MatrixTransform^ transform = nullptr;
        if (topRowMenuOffset != 0)
        {
            transform = ref new Windows::UI::Xaml::Media::MatrixTransform();
            Windows::UI::Xaml::Media::Matrix m = Windows::UI::Xaml::Media::Matrix::Identity;
            m.OffsetY = topRowMenuOffset;
            transform->Matrix = m;
        }
        auto panelElements0 = GetTextRowStackPanel(0)->Children;
        for (size_t k = 0; k < panelElements0->Size; k++)
        {
            auto textBlock = dynamic_cast<TextBlock^> (panelElements0->GetAt(k));
            //if (textBlock == nullptr)
            //    textBlock = dynamic_cast<TextBlock^> ((dynamic_cast<Button^> (panelElements0->GetAt(k)))->Content);
            textBlock->RenderTransform = transform;
        }
    }
};

// ---------------------------------------------------------------------------
// TermCanvas
// ---------------------------------------------------------------------------

TermCanvas::TermCanvas(UndoEnabledDelegate^ undoEnabledDelegate) :
    currentEmulationMode(iterminal::specialmodes::none), inReadChar(false), inLiteral(false),
    currentCursorOffset(0), mainPointer(-1), secondPointer(-1), renderedRowHeight(0), renderedCharWidth(0),
    inputPanelOccludedRect(Rect::Empty), extraPanelLeft(-1), occlusionShift(0), occlusionUnshift0(0),
    syntaxHighlightingRequested(false), currentFontWeight(Windows::UI::Text::FontWeights::Bold), revMenuEnabled(true), syntaxHighlightingEnabled(true),
    washighlightedpascal(false),
    currentActualWidth(0), currentActualHeight(0),
    setcompletioninfo_changed(false),
    undoEnabledDelegate(undoEnabledDelegate), undoenabled_canundo(-1), undoenabled_canredo(-1)
{
    // there can be only one instance
    if (theTerminal != nullptr)
        throw std::logic_error("attempted to create multiple instances of terminal--there can only be one");
    theContent = new TerminalContent();

    // reset paddles
    SetPointer(1, 0, 0, false, false, false);
    SetPointer(2, 0, 0, false, false, false);

    // initialize our belief on the terminal
    SetDesiredTerminalDimensions(0, 0, 15);     // default is medium font size, auto-scale
    SetBoldface(true);
    SetEnableRevMenu(true);
    UpdateGrantedTerminalDimensions();          // this does not know the font size yet, and will default to something
    ConfirmActualTerminalDimensions(theContent->term_width(), theContent->term_height(), theContent->term_width(), theContent->term_height());

    // create all constant Canvas elements
    drawingRect = ref new Rectangle();
    drawingRect->Width = 0;
    drawingRect->Height = 0;
    graphicsVisible = false;
    //drawingRect->IsHitTestVisible = false;  // will not get focus from mouse click
    //drawingRect->Stretch = Stretch::Fill;
    // TODO: now we don't actually have letter boxing; the text is still visible at the borders
    drawingRect->Fill = ref new ImageBrush();
    Children->Append(drawingRect);
    Canvas::SetLeft(drawingRect, 0);
    Canvas::SetTop(drawingRect, 0);
    Canvas::SetZIndex(drawingRect, 100);

    // create the textDisplay
    // actual text rows live inside a Canvas
    textDisplay = ref new TextDisplay();
    // vertical orientation
    //textDisplay->IsHitTestVisible = false;  // will not get focus from mouse click; focus shall remain on inputTextBox
    // Background is transparent
    Children->Append(textDisplay);
    Canvas::SetLeft(textDisplay, 0);
    Canvas::SetTop(textDisplay, 0);

    // suggestions box
    auto suggestionPopup = ref new Popup();      // embed in a Popup
    suggestionBox = ref new SuggestionBox(suggestionPopup, ref new SuggestionCallback(this, &TermCanvas::OnSuggestionBoxRowClicked));
    Children->Append(suggestionPopup);

    // input box (the thing that receives the keyboard input)
    inputTextBox = ref new TextBox();
#ifdef _DEBUG
    inputTextBox->Foreground = ref new SolidColorBrush(Windows::UI::Colors::Pink);  // for debugging--hide it eventually
#else
    inputTextBox->Foreground = ref new SolidColorBrush(Windows::UI::Colors::Transparent);  // for debugging--hide it eventually
#endif
    inputTextBox->Background = ref new SolidColorBrush(Windows::UI::Colors::Transparent);
    inputTextBox->TextWrapping = TextWrapping::Wrap;  // no, need to run over; what is that one?
    inputTextBox->BorderThickness = Windows::UI::Xaml::Thickness(0);
    Children->Append(inputTextBox);
    Canvas::SetLeft(inputTextBox, 0);   // will be moved/sized in refresh timer
    Canvas::SetTop(inputTextBox, 0);
    FocusInputTextBox();

    // some color to work with
    SetColorScheme(ReTro_Pascal::TerminalColorScheme::Amber);

    // others
    currentCommandBar.reserve(224);     // pre-allocate memory

    // all set: remember the one instance, for the TermXXX() functions (called by P-code thread)
    theTerminal = this;

    // handling the on-screen keyboard
    // We are robust against this triggering even before Ready() has returned 'true' (can it?)
    CreateExtraKeys();
    Windows::UI::ViewManagement::InputPane::GetForCurrentView()->Showing += ref new TypedEventHandler<Windows::UI::ViewManagement::InputPane^,Windows::UI::ViewManagement::InputPaneVisibilityEventArgs^>([this](Object^, Windows::UI::ViewManagement::InputPaneVisibilityEventArgs^ args) -> void
    {
        // bookkeeping
        inputPanelOccludedRect = args->OccludedRect;
        args->EnsuredFocusedElementInView = true;
        // popup
        (dynamic_cast<StackPanel^> (extraKeys->Child))->Width = inputPanelOccludedRect.Top;
        extraKeys->IsOpen = true;
        extraPanelLeft = Window::Current->CoreWindow->Bounds.Right - extraKeys->ActualWidth;
        //extraKeys->Height = inputPanelOccludedRect.Top;
        extraKeys->HorizontalOffset = extraPanelLeft;
        //extraKeys->VerticalOffset = renderedRowHeight + TEXTMARGIN;
        extraKeys->VerticalOffset = inputPanelOccludedRect.Top - extraKeys->ActualHeight;
        DisableDetectTapForKeyboard();  // keyboard here--make tap-to-bring-out-on-screen-keyboard 'non-hot'
    });
    Windows::UI::ViewManagement::InputPane::GetForCurrentView()->Hiding += ref new TypedEventHandler<Windows::UI::ViewManagement::InputPane^,Windows::UI::ViewManagement::InputPaneVisibilityEventArgs^>([this](Object^, Windows::UI::ViewManagement::InputPaneVisibilityEventArgs^ args) -> void
    {
        // popup
        extraPanelLeft = -1;
        extraKeys->IsOpen = false;
        // bookkeeping
        inputPanelOccludedRect = Rect::Empty;
    });
};

// add additional keys for the keyboard
void TermCanvas::CreateExtraKeys()
{
#if 1
    const wchar_t * main[] = { L"<esc>", L"<ctrl-enter>", NULL };
    const double fontSize = 20;
#else
    // TODO: figure out char code for arrow up and down
    const wchar_t * main[] = { L"<esc>",L"<ctrl-enter>",L"#",NULL,L"1",L"2",L"3",L"+",NULL,L"4",L"5",L"6",L"-",L"^^"/*xE110? up*/,NULL,L"7",L"8",L"9",L"*",NULL,L"0",L"=",L"^",L"/",L"vv",NULL,
        L"(",L")",L"[",L"]",NULL,
        L";",L" := ",L"(* ",L" *)",NULL,
        L"const ",L"var ",L"type ",NULL,
        L"int",L"str",L" real",NULL,
        L"array ",L"record ",NULL,
        L"proc",L"func",NULL,
        L"begin ",L"end",NULL,
        L"if ",L" then ",L" else ",NULL,
        L"for ",L" to ",L" do",NULL,
        //L"while",L"repeat",NULL,
        //L"until",NULL,
    };
    const double fontSize = 15;
#endif
    auto background = ref new SolidColorBrush(Windows::UI::ColorHelper::FromArgb(255, 48, 48, 55));
    auto foreground = ref new SolidColorBrush(Windows::UI::ColorHelper::FromArgb(255, 213, 213, 213));
    auto panel = ref new StackPanel();
    panel->Width = 200;
    panel->Orientation = Orientation::Vertical;
    //panel->VerticalAlignment = Windows::UI::Xaml::VerticalAlignment::Stretch;
    panel->VerticalAlignment = Windows::UI::Xaml::VerticalAlignment::Center;
    panel->Background = ref new SolidColorBrush(Windows::UI::Colors::Black);
    panel->Transitions = ref new Windows::UI::Xaml::Media::Animation::TransitionCollection();
    panel->Transitions->Append(ref new Windows::UI::Xaml::Media::Animation::EntranceThemeTransition());
    StackPanel^ row = nullptr;
    for (size_t k = 0; k < _countof(main); k++)
    {
        const wchar_t * sequence = main[k];
        if (!sequence)  // new row (note: required at end)
        {
            panel->Children->Append(row);
            row = nullptr;
            continue;
        }
        if (!row)
        {
            row = ref new StackPanel();
            row->Orientation = Orientation::Horizontal;
            row->HorizontalAlignment = Windows::UI::Xaml::HorizontalAlignment::Left;
            row->Background = ref new SolidColorBrush(Windows::UI::Colors::Black);
        }
        auto button = ref new Button();
        wstring s;
        for (size_t n = 0; sequence[n]; n++)
            if (sequence[n] != ' ')
                s.push_back(sequence[n]);
        button->Content = ref new String(s.c_str());
        button->Background = background;
        button->Foreground = foreground;        // text color
        button->BorderThickness = Thickness(0);
        //button->Margin = Thickness(0);        // has no effect
        button->FontSize = fontSize;
        if (wcscmp(sequence, L"<esc>") == 0)
            sequence = L"\x1b";
        else if (wcscmp(sequence, L"<etx>") == 0)
            sequence = L"\x03";
        else if (wcscmp(sequence, L"<ctrl-enter>") == 0)
            sequence = L"\x03";
        else if (wcscmp(sequence, L"proc") == 0)
            sequence = L"procedure ";
        else if (wcscmp(sequence, L"func") == 0)
            sequence = L"function ";
        else if (wcscmp(sequence, L"int") == 0)
            sequence = L" integer";
        else if (wcscmp(sequence, L"string") == 0)
            sequence = L" string";
        else if (wcscmp(sequence, L"^^") == 0)
            sequence = L"\x0f"; // Ctrl-O
        else if (wcscmp(sequence, L"vv") == 0)
            sequence = L"\x0c"; // Ctrl-L
        button->Click += ref new RoutedEventHandler([this, sequence](Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e) -> void
        {
            if (theContent)
            {
                AutoLock lock(theContent);
                for (size_t k = 0; sequence[k]; k++)
                    theContent->PushKey(sequence[k]);
            }
            FocusInputTextBox();    // set focus back
        });
        row->Children->Append(button);
    }
    extraKeys = ref new Popup();
    extraKeys->IsLightDismissEnabled = false;   // TODO: does this make the keyboard go away?
    extraKeys->Child = panel;
    extraKeys->Width = panel->Width;
    extraKeys->Height = fontSize + 20;
    Children->Append(extraKeys);
}

// helper to fill in PADDLEs[] and BUTTONs[] values
void TermCanvas::SetPointer(int id, double x, double y, bool left, bool middle, bool right)
{
    if (x < 0) x = 0; if (x > 255) x = 255;
    if (y < 0) y = 0; if (y > 255) y = 255;
    PADDLEs[(id-1)*2] = (int) x;
    PADDLEs[(id-1)*2+1] = (int) y;
    BUTTONs[(id-1)*3] = left;
    if (id == 1)
    {
        BUTTONs[1] = middle;
        BUTTONs[2] = right;
    }
}

void TermCanvas::OnPointerEventHandler(Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e, int inout)
{
    Windows::UI::Input::PointerPoint^ ptrPt = e->GetCurrentPoint(this);
    auto ptrId = e->Pointer->PointerId;
    auto ptrPos = ptrPt->Position;

    // get it in native resolution first
    // Note: drawingRect is not scaled and positioned off (0,0), so no mapping needed.
    auto w = graphicsVisible ? drawingRect->ActualWidth : ActualWidth;
    auto h = graphicsVisible ? drawingRect->ActualHeight : inputPanelOccludedRect.IsEmpty ? ActualHeight : inputPanelOccludedRect.Top;
    if (w == 0 || h == 0)   // too early, ignore
        return;
    double x = ptrPos.X;
    double y = (h - 1 - ptrPos.Y);   // origin is lower left corner

    // Apple PADDLE positions are 0..255
    x = x / w * 256;
    y = y / h * 256;

    // coming in
    if (inout > 0)
    {
        if (mainPointer < 0)
            mainPointer = ptrId;
        else if (secondPointer < 0)
            secondPointer = ptrId;
    }
    // going out
    else if (inout < 0)
    {
        if (ptrId == mainPointer)
            mainPointer = secondPointer = -1;   // if second: stop tracking both (end of gesture)
        else if (ptrId == secondPointer)
            secondPointer = -1;
    }
    // now return the appropriate thingy
    //  - tracking state only changed by enter or exit events
    //  - we may be tracking this many pointers:
    //     0: return mouse pointer (ptrId = 1) in PADDLE(0,1) and PADDLE(2,3)
    //     1: return that pointer in PADDLE(0,1) and PADDLE(2,3) and buttons in BUTTON(0..2), where touch-tracking is button 0 (no pop-ups)
    //     2: return second pointer in PADDLE(2,3) and BUTTON(3) = TRUE
    if (mainPointer < 0)    // not tracking anything
    {
        SetPointer(1, x, y, false, false, false);
        SetPointer(2, x, y, false, false, false);  // second pointer identical
    }
    else
    {
        if (ptrId == mainPointer)
        {
            bool leftDown = (ptrPt->Properties->IsLeftButtonPressed);
            bool middleDown = (ptrPt->Properties->IsMiddleButtonPressed);
            bool rightDown = (ptrPt->Properties->IsRightButtonPressed);

            SetPointer(1, x, y, leftDown, middleDown, rightDown);
            if (secondPointer < 0)  // not tracking second pointer
                SetPointer(2, x, y, false, false, false);
        }
        else if (ptrId == secondPointer)
        {
            // second pointer: we don't update the first
            SetPointer(2, x, y, true, false, false);  // second pointer identical
        }
    }

    // TODO: verify that this has an effect, actually
    //if (drawingRect->ActualHeight != 0) // graphics visible: capture all pointer events away from text box
    //    e->Handled = true;
}

void TermCanvas::SetColorScheme(TerminalColorScheme scheme)
{
    // Colors: http://msdn.microsoft.com/en-us/library/system.windows.media.solidcolorbrush.color%28v=vs.95%29.aspx
    textDisplay->SetColorScheme(scheme);

    currentColorScheme = scheme;

    // invalidate terminal so that our render thread picks up the change
    InvalidateTerminal();
}

// tell terminal to invalidate all chars, so that our render thread will do a full redraw
void TermCanvas::InvalidateTerminal()
{
    theContent->InvalidateTerminal();
}

// set desired size
//  - rows = 0
//     - if rowHeight != 0 -> determine by rowHeight
//     - else determine by columns (which defaults to 80 if all args are 0)
//  - columns = 0 -> determine by rows vs. aspect ratio
// to be called from UI thread
// Can be called before Canvas size is known.
void TermCanvas::SetDesiredTerminalDimensions(size_t columns, size_t rows, double rowHeight)
{
    // here we only remember the new settings
    AutoLock lock(theContent);   // this will block the VM if it enters any terminal function
    desiredColumns = columns;
    desiredRows = rows;
    desiredRowHeight = rowHeight;
}

// determine the actual dimensions (fill in the blank)
// Call this from render tick.
void TermCanvas::UpdateGrantedTerminalDimensions()
{
    AutoLock lock(theContent);   // this will block the VM if it enters any terminal function

    // fill in the unspecified parameters
    // The normal use case will be the user specifying a comfortable font size.
    grantedColumns = desiredColumns;
    grantedRows = desiredRows;
    double rowHeight = desiredRowHeight;

    if (!textDisplay)               // oops, too early
    {
        grantedColumns = 80; grantedRows = 24;    // should not happen; anyway, give it something to chew on
        return;
    }

    // TODO: do ActualHeight/Width get adjusted when the user adjusts the screen size? Or do I need to do that myself?
    double screenHeight = ActualHeight - 2 * TEXTMARGIN;
    double screenWidth = ActualWidth - 2 * TEXTMARGIN;

    if (grantedRows == 0)
    {
        if (rowHeight == 0)         // determine rowHeight by grantedColumns
        {
            if(grantedColumns == 0)
                grantedColumns = 80;       // default: 80
            auto charWidth = screenWidth / grantedColumns;
            rowHeight = charWidth / letterAspectRatio;
        }
        // determine grantedRows by rowHeight
        grantedRows = (size_t) (screenHeight / rowHeight);
    }

    if (grantedRows < 24) grantedRows = 24;               // minimum range = smallest known p-system
    if (grantedRows > 224) grantedRows = 224;             // maximum range limited by TermWrite GOTOXY values (32..255)

    grantedRowHeight = floor(screenHeight / grantedRows);     // (here we round, to avoid unnecessary round-off errors)

    auto charWidth = grantedRowHeight * letterAspectRatio;

    if (grantedColumns == 0)   // set by row only
        grantedColumns = (size_t) (screenWidth / charWidth);
    if (grantedColumns < 40) grantedColumns = 40;
    if (grantedColumns > 224) grantedColumns = 224;
}

// reveal granted dimensions to interpreter
// called by interpreter; should be reasonably fast since it's called in all terminal I/O functions
Platform::Array<size_t>^ TermCanvas::GetGrantedTerminalDimensions()
{
    AutoLock lock(theContent);
    auto result = ref new Array<size_t>(4);     // TODO: get rid of ridiculous 'ref new' here, use a friggin' struct!
    result[0] = grantedColumns; result[1] = grantedRows;
    result[2] = grantedColumns; result[3] = grantedRows;
    // now correct for on-screen keyboard (unless we use a fixed terminal size)
    if (!inputPanelOccludedRect.IsEmpty && (grantedRows != desiredRows || grantedColumns != desiredColumns))
    {
        // chop away the keyboard area from bottom
        auto screenHeight = inputPanelOccludedRect.Top - TEXTMARGIN;
        auto usableRows = (size_t) ceil(screenHeight / grantedRowHeight) - 1;
        if (usableRows < result[3])
            result[3] = usableRows;
        // and the extra keys from the right
        if (extraPanelLeft >= 0)
        {
            auto screenWidth = extraPanelLeft - TEXTMARGIN;
            auto charWidth = grantedRowHeight * letterAspectRatio;
            auto usableCols = (size_t) (screenWidth / charWidth);
            if (usableCols < result[2])
                result[2] = usableCols;
        }
    }
    return result;
    // TODO: find out how to return a simple struct in C++/ZX...
}

// called by interpreter to confirm desired uptake and resulting SYSCOM dimensions (must be values from SYSCOM)
// WIDTH and HEIGHT is SYSCOM's belief on the terminal size, so we pass that to theContent.
// Columns and rows is TermCanvas belief on how big to make the characters; they may differ if interpreter clamped the width, e.g. for EDITOR.
void TermCanvas::ConfirmActualTerminalDimensions(size_t columns, size_t rows, size_t WIDTH, size_t HEIGHT)
{
    AutoLock lock(theContent);   // this will block the VM if it enters any terminal function
    bool contentSizeChanged = WIDTH != theContent->term_width() || HEIGHT != theContent->term_height();
    confirmedColumns = columns;
    confirmedRows = rows;
    if (contentSizeChanged)
        theContent->term_resize(WIDTH, HEIGHT);
}

// call this on main UI thread to finish off the initialization once the layout is known but before interpreter starts
bool TermCanvas::Ready()
{
    if (RefreshTimer)   // gets set up as result of this function, so we use this to check whether we ran already
        return true;

    if (ActualHeight == 0 || ActualWidth == 0)
        return false;   // too early

    // we are ready
    // P-code interpreter thread must only start after this.

    // install event handlers (we install them not earlier to avoid them to trigger before we know our dimensions)
    // We use AddHandler() instead of += to be able to see events that the inputTextBox eats. WORKAROUND
    //KeyDown += ref new Windows::UI::Xaml::Input::KeyEventHandler(this,&TermCanvas::OnKeyDownHandler);
    AddHandler(KeyDownEvent, ref new Windows::UI::Xaml::Input::KeyEventHandler(this,&TermCanvas::OnKeyDownHandler), true);
    KeyUp += ref new Windows::UI::Xaml::Input::KeyEventHandler(this,&TermCanvas::OnKeyUpHandler);
    //Tapped += ref new Windows::UI::Xaml::Input::TappedEventHandler(this,&TermCanvas::OnTappedHandler);
    AddHandler(TappedEvent, ref new TappedEventHandler(this,&TermCanvas::OnTappedHandler), true);

    // hook up to listen for pointer events so that we can report PADDLE(0..3) and BUTTON(0..3)
    AddHandler(PointerPressedEvent, ref new PointerEventHandler(this,&TermCanvas::OnPointerPressedHandler), true);
    AddHandler(PointerReleasedEvent, ref new PointerEventHandler(this,&TermCanvas::OnPointerReleasedHandler), true);
    AddHandler(PointerExitedEvent, ref new PointerEventHandler(this,&TermCanvas::OnPointerExitedHandler), true);
    AddHandler(PointerCanceledEvent, ref new PointerEventHandler(this,&TermCanvas::OnPointerCanceledHandler), true);
    AddHandler(PointerCaptureLostEvent, ref new PointerEventHandler(this,&TermCanvas::OnPointerCaptureLostHandler), true);
    AddHandler(PointerMovedEvent, ref new PointerEventHandler(this,&TermCanvas::OnPointerMovedHandler), true);

    //PointerPressed += ref new Windows::UI::Xaml::Input::PointerEventHandler(this,&TermCanvas::OnPointerReleasedHandler);
    //inputTextBox->RightTapped += ref new Windows::UI::Xaml::Input::RightTappedEventHandler(this,&TermCanvas::OnTappedHandler);
    //inputTextBox->PointerPressed += ref new Windows::UI::Xaml::Input::PointerEventHandler(this,&TermCanvas::OnPointerReleasedHandler);
    //textDisplay->RightTapped += ref new Windows::UI::Xaml::Input::RightTappedEventHandler(this,&TermCanvas::OnTappedHandler);
    //textDisplay->PointerPressed += ref new Windows::UI::Xaml::Input::PointerEventHandler(this,&TermCanvas::OnPointerReleasedHandler);
    IsTapEnabled = true;
    //this-> Clicked += ref new Windows::UI::Xaml::Input::TappedEventHandler(this,&TermCanvas::OnTappedHandler);
    //MouseUp += ref new Windows::UI::Xaml::Input::KeyEventHandler(this,&TermCanvas::OnKeyUpHandler);
    //StylusUp += ref new Windows::UI::Xaml::Input::KeyEventHandler(this,&TermCanvas::OnKeyUpHandler);
    inputTextBox->TextChanged += ref new Windows::UI::Xaml::Controls::TextChangedEventHandler(this,&TermCanvas::OnTextChangedHandler);
    inputTextBox->IsTextPredictionEnabled = false;
    //inputTextBox->IsHitTestVisible = false;  // will not get focus from mouse click

    // perform refresh timer function once, so that everything gets initialized
    // This makes sure that the graphics call back is called before the p-code engine starts.
    OnRefreshTimerTick(nullptr, nullptr);

    // now install refresh timer to keep executing the refresh function periodically
    RefreshTimer = ref new Windows::UI::Xaml::DispatcherTimer();
    TimeSpan ts;
    ts.Duration = 100000;   // 10 ms (in 100 ns units)
    RefreshTimer->Interval = ts;
    RefreshTimer->Start();
    RefreshTimer->Tick += ref new EventHandler<Object^>(this,&TermCanvas::OnRefreshTimerTick);
    return true;
}

// tricky one:
// We need to move the input text box out of the screen, otherwise we will get the wrong pop-up (Paste instead of AppBar)
// and the wrong cursor (text cursor instead of arrow).
// But that will cause tap-to-reveal-on-screen keyboard to fail.
// We assume the user will try again if it does not happen; and that the user will not often explicitly hide the keyboard.
// Thus, a single touch tap will move the input text box into view for 5 seconds, and then off again. During this time,
// we will have the wrong mouse/right-click (which is OK since we are tapping).
void TermCanvas::EnableDetectTapForKeyboard()
{
    if (inputPanelOccludedRect.IsEmpty)  // don't enable if on-screen kbd is already there
    {
        inputTextBox->Width = ActualWidth + TEXTINPUTOFFSCREENBY;   // note: just cannot be 0; that will disable it
        inputTextBox->Height = ActualHeight;
        Canvas::SetLeft(inputTextBox, -TEXTINPUTOFFSCREENBY);   // we still make it start left of screen to avoid making the key input visible
        Canvas::SetTop(inputTextBox, 0);
        //inputTextBox->Background = ref new SolidColorBrush(Windows::UI::Colors::Red); // enable during debugging to see it
        // kick off timer
        if (!DisableDetectTapForKeyboardTimer)
        {
            DisableDetectTapForKeyboardTimer = ref new Windows::UI::Xaml::DispatcherTimer();
            TimeSpan ts;
            ts.Duration = 50000000;   // 5 s (in 100 ns units)
            DisableDetectTapForKeyboardTimer->Interval = ts;
            DisableDetectTapForKeyboardTimer->Tick += ref new EventHandler<Object^>(this,&TermCanvas::DisableDetectTapForKeyboard);
        }
        DisableDetectTapForKeyboardTimer->Start();
    }
    else
        DisableDetectTapForKeyboard();
}

void TermCanvas::DisableDetectTapForKeyboard(Object^, Object^)
{
    if (DisableDetectTapForKeyboardTimer)
        DisableDetectTapForKeyboardTimer->Stop();
    inputTextBox->Height = 1;   // note: just cannot be 0; that will disable it
    inputTextBox->Width = 1;
    Canvas::SetLeft(inputTextBox, ActualWidth);
    Canvas::SetTop(inputTextBox, ActualHeight);
}

// called M times a second; we periodically transfer the TerminalContent to the textDisplay
// Must run on UI thread.
void TermCanvas::OnRefreshTimerTick(Object^, Object^)
{
    // transfer to the screen

    // pick up user request for dimension change, or environment changes (screen size, on-screen keyboard)
    UpdateGrantedTerminalDimensions();
    // Note that these are not effective yet. They still need to be picked up by the interpreter thread.
    // I.e. this will not affect rendering below.

    // change tracking
    bool windowSizeChanged = (currentActualHeight != ActualHeight || currentActualWidth != ActualWidth);
    if (windowSizeChanged)
    {
        currentActualHeight = ActualHeight;
        currentActualWidth = ActualWidth;
    }

    // update size of inputTextBox
    if (windowSizeChanged)
        DisableDetectTapForKeyboard();  // will position the input text box off-screen

    // render the text
    // simplistically concatenate all text
    // TODO: we could do better here by checking whether either the text is dirty or and the cursor position actually changed
    // TODO: change to one TextBlock per line
    {
        AutoLock lock(theContent);   // this will block the VM if it enters any terminal function

        bool terminalDirty = theContent->IsAnyDirty();      // did any text in the terminal change?
        bool cursorDirty = theContent->GetCursorDirty();    // did the cursor move? (incl. inrow)

        // implant background color right away; text colors will be updated in render thread
        if (terminalDirty)
            Background = textDisplay->textBrushes[TextClass::background];    // (avoid doing it all the time; not sure how cheap it is)

        // set size of textDisplay and get the font size
        // Note that we resize it w.r.t. the actualColumns/actualRows which were thrown back to us from the desired values.
        // Dimensions are not necessarily the same as SYSCOM, since interpreter may have clamped it (e.g. for EDITOR).
        double screenHeight = ActualHeight - 2 * TEXTMARGIN;
        double screenWidth = ActualWidth - 2 * TEXTMARGIN;

        // physically adjust the size of the TextBlock, and then set the internal size
        // TODO: we should use 'confirmedRowHeight' here... meh
        windowSizeChanged |= (renderedRowHeight != grantedRowHeight); // possible cause for size change
        renderedRowHeight = grantedRowHeight;     // (here we round, to avoid unnecessary round-off errors)
        renderedCharWidth = textDisplay->EstimateRenderedCharWidth(renderedRowHeight * letterAspectRatio);
        //double actualAspectRatio = renderedCharWidth / renderedRowHeight;
        // Consolas:
        // Large Font (21):  0.54984129042852492
        // Medium Font (15): 0.54977779388427739
        // Small Font (11):  0.54984129042852492
        // -> 0.54982

        auto textHeight = renderedRowHeight * confirmedRows;    // required size of the text rendering area
        auto textWidth = renderedCharWidth * confirmedColumns;

        windowSizeChanged |= (textDisplay->Height != textHeight || textDisplay->Width != textWidth); // possible cause for size change
        if (windowSizeChanged)
        {
            textDisplay->Height = textHeight;
            textDisplay->Width = textWidth;
        }

        // stretch if fixed size was requested
        bool scaling = (confirmedRows == desiredRows && confirmedColumns == desiredColumns); // dimensions were requested directly
        double scaleX = 1.0;
        double scaleY = 1.0;
        if (scaling)
        {
            auto effectiveScreenWidth = screenWidth; // (extraPanelLeft < 0) ? screenWidth : extraPanelLeft - TEXTMARGIN; // panel is small at the moment
            auto effectiveScreenHeight = inputPanelOccludedRect.IsEmpty ? screenHeight : inputPanelOccludedRect.Top - TEXTMARGIN;
            scaleX = effectiveScreenWidth / textWidth;
            scaleY = effectiveScreenHeight / textHeight;
        }
        // TODO: continue here for windowSizeChanged tracking if it has value
        if (scaleX != 1.0 || scaleY != 1.0)
        {
            auto transform = ref new Windows::UI::Xaml::Media::ScaleTransform();
            transform->ScaleX = scaleX;
            transform->ScaleY = scaleY;
            textDisplay->RenderTransform = transform;
        }
        else
            textDisplay->RenderTransform = nullptr;

        // now put the stuff into it
        size_t row = theContent->GetRow();          // current cursor position
        size_t col = theContent->GetCol();
        size_t inrow = theContent->GetInRow();      // row where cursor was at TermRead call
        size_t width = theContent->term_width();    // current terminal dimensions
        size_t height = theContent->term_height();

        // text cursor panel is the last entry--pop it off temporarily

        // make sure the vertical StackPanel has the correct number of rows
        textDisplay->SetNumRows(height);

        // is top row still the line that the interpreter classified as a command bar?
        // (we check for prefix only since characters may have been appended; FIND/REPLACE adds chars after the PROMPT call)
        bool topRowIsMenu = !currentCommandBar.empty() && begins_with(theContent->GetRowString(0).c_str(), currentCommandBar.c_str());

        // add special keys (Esc, Etx) if they are enabled
        if (!inputPanelOccludedRect.IsEmpty)
        {
            bool acceptsEtx = currentCommandBar.find(L"<ctrl-enter>") != wstring::npos
                           || currentCommandBar.find(L"<etx>") != wstring::npos;
            bool acceptsEsc = currentCommandBar.find(L"<esc>") != wstring::npos;
            // TODO: CONTINUE THIS
            sin(1.0);
        }

        // deal with occlusion (on-screen keyboard; font-size increase that is not picked up), by moving the textDisplay around
        if ((inputPanelOccludedRect.IsEmpty && textHeight <= screenHeight) || scaling)  // when scaling then we always force it all to be visible
            occlusionShift = 0; // no shift
        else
        {
            // this handles both an on-screen keyboard and a setting where the terminal is taller than the screen (user increased font, program did not confirm)
            auto maxY = inputPanelOccludedRect.IsEmpty ? screenHeight + TEXTMARGIN : inputPanelOccludedRect.Top;         // we assume the keyboard comes in from the bottom
            size_t effectivetermheight = inrow+1;           // determine effective use area of the screen; i.e. non-empty top region
            for (size_t i = effectivetermheight; i < height; i++)
            {
                if (!theContent->IsRowEmpty(i))
                    effectivetermheight = i+1;
            }
            if (effectivetermheight * renderedRowHeight <= maxY)  // keyboard visible, but terminal fits (e.g. EDITOR was started after keyboard has already come out)
                occlusionShift = 0;                         // no shift
            else if (inrow > 0 || !topRowIsMenu)
            {
                auto inCursorY = renderedRowHeight * inrow + TEXTMARGIN;
                if (inCursorY + 2*renderedRowHeight + occlusionShift > maxY)        // force it into view
                    occlusionShift -= renderedRowHeight * 1.3;  // non-even step to make sure it is visibly clear what happens
                else if (inCursorY - 2*renderedRowHeight + occlusionShift < (topRowIsMenu ? renderedRowHeight + TEXTMARGIN : 0))
                {
                    occlusionShift += renderedRowHeight * 1.3;
                    if (occlusionShift > 0)
                        occlusionShift = 0;
                }
            }
            // else in top row we are always visible since we shift for that below
        }
        occlusionUnshift0 = topRowIsMenu ? -occlusionShift : 0; // menu row gets moved up

        // update all vertical positions (which may have changed)
        for (size_t i = 0; i < height; i++)
            textDisplay->SetRowTop(i,
                      renderedRowHeight * i + ((i == 0) ? occlusionUnshift0 : TEXTMARGIN));
        Canvas::SetTop(textDisplay, occlusionShift);    // shift the whole thing up

        // gather ids for Pascal completion
        bool pascalCompletion = inReadChar && (currentEmulationMode & (iterminal::specialmodes::enableSuggestions));
        string idPrefix;
        set<string> matchingIds;                        // matching Pascal ids (prefix match of partial word typed against all seen identifiers so far)

        bool needtohighlight = syntaxHighlightingEnabled && syntaxHighlightingRequested;
        bool needtoparse = needtohighlight || pascalCompletion;  // parser used for both

        // TODO here: Highlight function also called for suggestionBox; takes both flags; builds prediction info
        // TODO: oh gee, this will process over and over again for suggestions while inputting; will drain the battery!
        bool ishighlightedpascal = false;   // we have highlighted code on the screen --TODO: rename this to something more clear
        bool ispascalsource = false;        // the code on the screen is Pascal
        inLiteral = false;                  // if true (inside '' or {}), this will block '?' to be intercepted
        if (needtoparse)
        {
            set<string> idsOnScreen;
            if (!washighlightedpascal || terminalDirty || pascalCompletion)
            {
                bool ignoreTopRow = topRowIsMenu;
                if (currentCommandBar.find(L"Type <sp>") != wstring::npos || currentCommandBar.find(L"Press <sp>"))  // showing an error or message
                    ignoreTopRow = true;    // otherwise when showing an error highlighting suppression may fire depending on the error message
                ispascalsource = HighlightPascalSyntax(pascalCompletion, ignoreTopRow, allTextClasses, idPrefix, idsOnScreen, inLiteral);
            }
            else
                ispascalsource = washighlightedpascal; // we keep what it was
            ishighlightedpascal = ispascalsource & needtohighlight;
            // merge ids into global set of seen ids
            if (ispascalsource)
                idsSeenSoFar.insert(idsOnScreen.begin(), idsOnScreen.end());
            // find id that matches the prefix at current cursor position (unless in comment/string or not Pascal source)
            if (pascalCompletion && ispascalsource && !idPrefix.empty())
            {
                const auto & line = theContent->GetRowString(row);
                // find it in the matchingIds set
                for (auto iter = idsSeenSoFar.begin(); iter != idsSeenSoFar.end(); iter++)
                {
                    const auto idstring = *iter;
                    if (idPrefix.size() < idstring.size() && _strnicmp(idstring.c_str(), idPrefix.c_str(), idPrefix.size()) == 0) // prefix match
                        matchingIds.insert(idstring);
                }
            }
        }
        // render
        for (size_t r = 0; r < height; r++)
        {
            if (ishighlightedpascal)    // transfer it if it is deemed Pascal source code; otherwise ignore the highlighting information and set to 'text'
                theContent->PutRowCharClasses(r, allTextClasses[r]);
            else if (washighlightedpascal)
                theContent->PutRowCharClasses(r, vector<TextClass>());
        if (terminalDirty || ishighlightedpascal || washighlightedpascal)
            for (size_t r = 0; r < height; r++)
                HighlightClickableLinks(r);  // clickable HTTP links
        }
        washighlightedpascal = ishighlightedpascal; // so we know to hide it next time if needed

        // note: now we operate with width and height of terminal content, as SYSCOM believes it is
        bool changed = false;
        size_t rowOffset = 0;
        vector<TextSegment> rowSegments;
        wstring line;
        line.reserve (200);
        for (size_t i = 0; i < height; i++)
        {
            // render row i
            // Each row consists of a horizontal StackPanel, so that we can stack text into it from left to right.
            auto dirty = theContent->GetAndClearDirty(i) || textDisplay->IsVirginRow(i);
            if (dirty) // we always do the cursor row--I am just too lazy now to keep old cursor position to detect the change...
            {
                // build segments
                line = theContent->GetRowString(i);
                for (size_t k = 0; k < line.size(); k++)
                    if (line[k] == ' ') line[k] = 160;  // use non-breaking space; space will be optimized away by TextBlock
                // Note: the space-width issue applies to both 32 and 160
                auto classes = theContent->GetRowCharClasses(i);
                rowSegments.clear();
                size_t b = 0;
                for (size_t e = 1; e <= line.size(); e++)
                {
                    if (e < line.size() && classes[e-1] == classes[e])
                        continue;
                    // found end of homogenous stretch
                    TextClass textClass = b < classes.size() ? (TextClass) classes[b] : TextClass::background;
                    rowSegments.push_back(TextSegment(textClass, line.substr(b, e-b)));
                    b = e;
                }
                if (rowSegments.empty())  // make sure we got at least one character in the TextBlock, for sizing etc.
                    rowSegments.push_back(TextSegment(TextClass::text, wstring(1, 160)));      // make sure we have a TextBlock

                // render segments
#if 0           // TODO: continue this, use buttons for touchable elements
                panelElements->Clear();
                for (size_t k = 0; k < rowSegments.size(); k++)
                {
                    const auto & seg = rowSegments[k];
                    auto textBlock = NewTextBlock(i == 0, k == 0);
                    textBlock->Text = ref new String(seg.text.c_str());
                    if (i == 0 && topRowIsMenu && revMenuEnabled)
                        textBlock->Foreground = textBrushes[TextClass::background];
                    else
                        textBlock->Foreground = textBrushes[seg.textClass];
                    if (i == 0 && k == 0)
                    {
                        auto button = ref new Button();
                        button->Content = textBlock;
                        button->IsHitTestVisible = true;
                        panelElements->Append(button);
                    }
                    else
                        panelElements->Append(textBlock);
                }
#else
                textDisplay->RenderSegmentsAsTextRuns(i, rowSegments, topRowIsMenu, revMenuEnabled);
#endif
            }
        }

        // place the cursor
        auto isRowInverted = row == 0 && topRowIsMenu && revMenuEnabled;    // if menu is inverted, so should be the cursor
        double cursorX = textDisplay->SetCursor(col, row, topRowIsMenu, isRowInverted, renderedCharWidth);

        // set width and font
        auto scaledCurrentActualWidth = TransformToVisual(textDisplay)->TransformPoint(Point((float)currentActualWidth,0)).X;
        textDisplay->UpdateRowVisuals(currentFontWeight, renderedRowHeight, scaledCurrentActualWidth);

        // adjust and invert the top-most row if it is a command bar
        textDisplay->SetTopRow(topRowIsMenu, revMenuEnabled);

        // the suggestion box
        if (pascalCompletion && ispascalsource)
        {
            suggestionBox->SetContentType(SuggestionBoxContent::ids);
            suggestionBox->PopulateEntriesForPascal(idPrefix, matchingIds);
        }
        else if (inReadChar
                 && (currentEmulationMode & iterminal::specialmodes::enablePathnameCompletion))
        {
            // update info if changed
            // This hands over the new info as an rvalue reference (we don't keep them in 'this').
            // Interpreter must call setcompletioninfo() before readchar().
            // I.e. _changed flag gets set first, while !inReadChar, then call readchar(), which sets inReadChar.
            // The first timer tick while inside readchar() will propagate it over to the suggestion box.
            if (setcompletioninfo_changed)
            {
                setcompletioninfo_changed = false;
                suggestionBox->SetContentType(SuggestionBoxContent::pathnames);
                suggestionBox->PopulateEntries(move(setcompletioninfo_prefix), move(setcompletioninfo_completedstrings));
            }
        }
        else    // outside readchar() or not enabled: clear it
        {
            suggestionBox->SetContentType(SuggestionBoxContent::none);
        }
        // display it appropriately
        if (suggestionBox->ContentType() != SuggestionBoxContent::none)
        {
            auto suggX = cursorX - suggestionBox->Prefix().size() * renderedCharWidth;  // (textDisplay coordinates)
            auto cursorOnCanvas = textDisplay->TransformToVisual(Window::Current->Content)->TransformPoint(Point((float)suggX,textDisplay->GetRowTopLeft(row).Y));
            auto cursorOnCanvas1 = textDisplay->TransformToVisual(Window::Current->Content)->TransformPoint(Point((float)suggX,textDisplay->GetRowBottomLeft(row).Y));
            // Note: Should suggestion box use the same transform as textDisplay?
            suggestionBox->Show(Windows::Foundation::Rect((float)cursorOnCanvas.X,(float)cursorOnCanvas.Y,0,(float)cursorOnCanvas1.Y - (float)cursorOnCanvas.Y),
                                Windows::Foundation::Rect(0,0,(float)ActualWidth,inputPanelOccludedRect.IsEmpty ? (float)ActualHeight : inputPanelOccludedRect.Top),
                                textDisplay->textFont, renderedRowHeight);
        }
        else
            suggestionBox->Hide();

#if 0
#if 1
        // position the input text box off-screen
        // On a system with Chinese keyboard, there was a little kbd-switch logo popping up.
        Canvas::SetTop(inputTextBox, ActualHeight);
        Canvas::SetTop(inputTextBox, ActualWidth);
#else
        // position the inputTextBox vertically as to ensure our visible cursor is still visible if the keyboard pops up
        // This seems to have no use. We now manage the vertical position ourselves.
        Canvas::SetTop(inputTextBox, cursorY);
#endif
#endif
    }
#if 0   // old text display code, unused
    size_t cursorOffset = SIZE_MAX;     // no cursor
    // push to display if nothing changed
    if (newTextBuffer != currentText || cursorOffset != currentCursorOffset)
    {
        wstring displayText = newTextBuffer;
        textDisplay->Inlines->Clear();
        if (cursorOffset == SIZE_MAX)
        {
            auto all = ref new Run();
            all->Text = ref new String(newTextBuffer.c_str());
            textDisplay->Inlines->Append(all);
        }
        else
        {
            // left of cursor
            auto left = ref new Windows::UI::Xaml::Documents::Run();
            left->Text = ref new String(newTextBuffer.substr(0, cursorOffset).c_str());
            textDisplay->Inlines->Append(left);
            // cursor
            auto cursorFormatter = ref new Windows::UI::Xaml::Documents::Underline();
            //cursorFormatter->Foreground = ref new SolidColorBrush(Windows::UI::Colors::White);
            // TODO: How to make it inverse? The underline is kind of hard to see
            auto cursor = ref new Windows::UI::Xaml::Documents::Run();
            wstring cursorChar = newTextBuffer.substr(cursorOffset, 1);
            if (cursorChar[0] == ' ')       // (somehow == and != don't work with wstrings!!)
                cursorChar[0] = 160;        // regular space at end of line gets no formatting...
            cursor->Text = ref new String(cursorChar.c_str());
            cursorFormatter->Inlines->Append(cursor);
            textDisplay->Inlines->Append(cursorFormatter);
            // right of cursor
            if (cursorOffset+1 < newTextBuffer.size())
            {
                auto right = ref new Windows::UI::Xaml::Documents::Run();
                right->Text = ref new String(newTextBuffer.substr(cursorOffset+1).c_str());
                textDisplay->Inlines->Append(right);
            }
        }
        ::swap(newTextBuffer, currentText);
        ::swap(cursorOffset, currentCursorOffset);
    }
#endif

    // handle graphics
    if (GraphicsTickCallBackFunction)
    {
        auto w = ActualWidth;
        // for now, we only use the area not covered by the on-screen keyboard
        // Later we may want to hide the keyboard when graphics is visible.
        auto h = inputPanelOccludedRect.IsEmpty ? ActualHeight : inputPanelOccludedRect.Top;

        // TODO: communicate back the actual size of the brush, for PADDLE()
        // TODO: do we need to keep the imageSource in the object?
        auto imageSource = GraphicsTickCallBackFunction(GraphicsTickCallBackInstance, (size_t) w, (size_t) h);
        if (imageSource)       // Visibility property is read-only... meh!
        {
            auto brush = dynamic_cast<ImageBrush^> (drawingRect->Fill);
            if (!imageSource->Equals(brush->ImageSource))
            {
                brush->ImageSource = imageSource;
                brush->Stretch = Stretch::Fill;     // maximize size by full stretch
            }
            drawingRect->Width = w;                 // note: this will adjust to any screen changes immediately
            drawingRect->Height = h;
            graphicsVisible = true;
        }
        else
        {
            drawingRect->Width = 0;                 // hide it --TODO: is this the right way to hide it?
            drawingRect->Height = 0;
            graphicsVisible = false;
        }
    }
}

#if 0
// why is this missing from WinRT??
// WORKAROUND
// TODO: This can be much faster by just calculating back from p using the size of a character, since we are mono-spaced (well, except for 'space'...).
Windows::UI::Xaml::Documents::TextPointer^ GetPositionFromPoint(TextBlock^ textDisplay, Point p)
{
    // iterate over all characters and look for the one that the pointer is in
    // This is a terrible way of doing this (linear complexity), but it happens only when the user taps.
    auto contentStart = textDisplay->ContentStart;
    auto contentEnd = textDisplay->ContentEnd;
    // determine character width--rect.Width is always 0... WORKAROUND
    double width = 0;
    for (size_t offset = 0; ; offset++)
    {
        // TextPosition has no 'Next' method, so we need to search by char pos... WORKAROUND
        // (it has one--advance  by 1--but there is no CompareTo to know whether we hit the end)
        auto pos = contentStart->GetPositionAtOffset(offset, Windows::UI::Xaml::Documents::LogicalDirection::Forward);
        if (pos == nullptr)
            break;
        auto rect = pos->GetCharacterRect(Windows::UI::Xaml::Documents::LogicalDirection::Forward);
        if (width == 0.0 && rect.X > 0.0)       // find the second character
        {
            width = rect.X;                     // and take its X coordinate as width
            break;
        }
    }
    if (width == 0)
        return nullptr;      // if it won't tell me the width then I can't do anything
    // now the main loop
    for (size_t offset = 0; ; offset++)
    {
        auto pos = contentStart->GetPositionAtOffset(offset, Windows::UI::Xaml::Documents::LogicalDirection::Forward);
        if (pos == nullptr)
            break;
        auto rect = pos->GetCharacterRect(Windows::UI::Xaml::Documents::LogicalDirection::Forward);
        if (rect.Y > p.Y)   // beyond point
            break;
        if (rect.Y + rect.Height < p.Y)
            continue;
        // we are in the correct line
        if (rect.X > p.X)
            break;          // beyond
        if (p.X < rect.X + width)
            return pos;
    }
    return nullptr;
}

// map offsets to character positions in the Text property
// There is no API for this... WORKAROUND
void GetPositionFromPoint(TextBlock^ textDisplay, vector<size_t> & posToCharMap)
{
    posToCharMap.clear();
    posToCharMap.reserve(textDisplay->Text->Length()*2);
    auto contentStart = textDisplay->ContentStart;
    auto contentEnd = textDisplay->ContentEnd;
    Windows::Foundation::Rect prevRect(0,0,0,0);
    size_t charIndex = 0;
    for (size_t offset = 0; ; offset++)
    {
        auto pos = contentStart->GetPositionAtOffset(offset, Windows::UI::Xaml::Documents::LogicalDirection::Forward);
        if (pos == nullptr)
            break;
        auto rect = pos->GetCharacterRect(Windows::UI::Xaml::Documents::LogicalDirection::Forward);
        if (rect != prevRect)
        {
            if (offset > 0) charIndex++;
            prevRect = rect;
        }
        posToCharMap.push_back(charIndex);
    }
}
#endif

// we read off a key press from the screen instructions
// formats:
//  - X(text text text          // no punctuation between click point; non-letter before key
//  - <key> text tex text       // same here
int MatchBracketedKeys(const wchar_t * s)
{
    if (begins_with (s, "<etx>"))
        return 3;
    else if (begins_with (s, "<ctrl-enter>"))
        return 3;
    else if (begins_with (s, "<bs>"))
        return 8;
    else if (begins_with (s, "<backspace>"))
        return 8;
    else if (begins_with (s, "<ret>"))
        return 13;
    else if (begins_with (s, "<return>"))
        return 13;
    else if (begins_with (s, "<cr>"))
        return 13;
    else if (begins_with (s, "<enter>"))
        return 13;
    else if (begins_with (s, "<esc>"))
        return 27;
    else if (begins_with (s, "<escape>"))
        return 27;
    else if (begins_with (s, "<sp>"))
        return 32;
    else if (begins_with (s, "<space>"))
        return 32;
    else if (begins_with (s, "<spacebar>"))
        return 32;
    else if (begins_with (s, "<esc-ret>"))
        return 0x1b0d;  // special code
    else if (begins_with (s, "<esc><enter>"))
        return 0x1b0d;  // special code
    //else if (begins_with (s, "<del>"))
    //    return 127;     // ?
    else
        return 0;
}
int ReadOffKeyPress(const wstring & text, size_t index)
{
    // search to the left for X) expression
    for (size_t i = index; ; i--)
    {
        int ch = text[i];
        // .L(x
        if (isalpha(ch)
            && (i == 0 || text[i+1] == '(')
            && i > 0 && i+2 < text.size() && !isalpha(text[i-1]) && isalpha(text[i+2]))
            return ch;
        else if (ch == '<')
            return MatchBracketedKeys(text.c_str() + i);
        // continuous stretch of letters or space, ', -
        // also read over ( and >
        else if (i == 0 || ch < 32
            || (!isalpha(ch) && ch != '\'' && ch != '-'
                && ch != '(' && ch != '>'\
                && !isspace(ch)))
            break;
    }
    return 0;
}

void TermCanvas::OnTappedHandler(Object^ sender, Windows::UI::Xaml::Input::TappedRoutedEventArgs^ e)
{
    // if touch tap then make input screen 'hot' for bringing out touch keyboard
    auto devtype = e->PointerDeviceType;
    bool wasTouch = (devtype == Windows::Devices::Input::PointerDeviceType::Touch || devtype == Windows::Devices::Input::PointerDeviceType::Pen);   // how to do that?
    if (wasTouch && inputPanelOccludedRect.IsEmpty)
        EnableDetectTapForKeyboard();   // now input text box will cover the whole screen and thus respond to yet another tap

    // move focus back to input text box in case it lost it
    FocusInputTextBox();

    // interpret touch event on menu-like text

    if (graphicsVisible)                        // GRAFMODE: no menu
        return;

    // we do all coordinates in the space of textDisplay
    // That is important, since it may be scaled.
    auto point = e->GetPosition(textDisplay);   // position relative to textDisplay (scaling compensated)

    // map to character position
    // The top row has a different height; so we actually scan for the row by row coordinates.
    if (point.Y < -40 || point.X < -40) // too far out
        return;
    if (point.Y >= textDisplay->ActualHeight + 40 || point.X >= textDisplay->ActualWidth + 40)
        return;
    if (point.Y < 0)
        point.Y = 0;
    if (point.Y > textDisplay->ActualHeight) // clip it
        point.Y = (float) textDisplay->ActualHeight;
    size_t rows = textDisplay->GetNumRows();
    if (rows == 0)  // no content
        return;
    size_t row;
    for (row = 0; row < rows-1/*last is fall-through*/; row++)
    {
        auto rowBottomLeft = textDisplay->GetRowBottomLeft(row);
        if (point.Y < rowBottomLeft.Y)
            break;
    }

    if (point.X < 0)
        point.X = 0;
    if (point.X > ActualWidth)
        point.X = (float)ActualWidth;
    size_t col = (size_t) (point.X / renderedCharWidth);

    // get the text out
    wstring text = textDisplay->GetRowText(row);
    if (text.empty())
        return;
    if (col >= text.size()+3)
        return;
    if (col >= text.size())
        col = text.size()-1;

    // interpret touch event on that string into a key sequence
    AutoLock lock(theContent);
    if (theContent->GetRowCharClass(col, row) == TextClass::link)
    {
        // find range
        size_t spos;
        size_t epos;
        for (spos = col; spos > 0 && theContent->GetRowCharClass(spos-1, row) == TextClass::link; spos--)
            ;
        for (epos = col+1; theContent->GetRowCharClass(epos, row) == TextClass::link; epos++)
            ;
        const auto url = text.substr(spos, epos-spos);
        auto uri = ref new Windows::Foundation::Uri(ref new Platform::String(url.c_str()));
        Windows::System::Launcher::LaunchUriAsync(uri);
        return;
    }
    int ch = ReadOffKeyPress(text, col);

    // and inject it into the input buffer
    for (int k = 24; k >= 0; k -= 8)
    {
        // ch can be multi-char constant, e.g. 'abcd'
        int ch1 = ((unsigned int) ch >> (unsigned int) k) & 0xff;
        if (ch1)
            theContent->PushKey(ch1);
    }
}

//void TermCanvas::OnPointerReleasedHandler(Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e)
//{
//    int x = 0;
//    x++;
//}

// I hate that I have to write this myself.
class MapVirtualKey
{
    bool ctrlDown;
    bool shiftDown;
public:
    MapVirtualKey() : ctrlDown (false), shiftDown (false) { }
    // -1 if not handled  --regular keys return this and are caught through a TextChangedEvent
    // 0 if modifier key
    // ASCII code else
    int Map(Windows::System::VirtualKey vkc, bool isDown)
    {
        int vk = (int) vkc;
        if (!isDown)
        {
            if (vkc == Windows::System::VirtualKey::Control)
            {
                ctrlDown = false;
                return 0;
            }
            else if (vkc == Windows::System::VirtualKey::Shift)
            {
                shiftDown = false;
                return 0;
            }
            return -1;
        }
        else if (vkc == Windows::System::VirtualKey::Control)
        {
            ctrlDown = true;
            return 0;
        }
        else if (vkc == Windows::System::VirtualKey::Shift)
        {
            shiftDown = true;
            return 0;
        }
        // ctrl-C is not accepted (means Copy), so use Ctrl-Enter instead
        else if (vkc == Windows::System::VirtualKey::Enter && ctrlDown)
            return 3;   // <etx>
        // Shift-DEL for forward-delete in editor?
        // BUGBUG: editor source defines RUBOUT (127) but does not interpret it
        else if (vkc == Windows::System::VirtualKey::Back && shiftDown)
            return 127; // RUBOUT (per EDITOR source code)
        else if (vk >= 'A' && vk <= 'Z' && ctrlDown)
            return vk - 64;
#if 0
        // for some, the code is the same as ASCII
        else if (vkc == Windows::System::VirtualKey::Escape
            || vkc == Windows::System::VirtualKey::Enter
            || vkc == Windows::System::VirtualKey::Back
            || vkc == Windows::System::VirtualKey::Tab
            || vkc == Windows::System::VirtualKey::Space)
            return vk;
#endif
        else switch(vkc)
        {
        case Windows::System::VirtualKey::Escape: return 27;
        case Windows::System::VirtualKey::Enter: return 13;
        case Windows::System::VirtualKey::Back: return 8;
        case Windows::System::VirtualKey::Tab: return 9;
        case Windows::System::VirtualKey::Up: return 15;        // editor responds to Ctrl-O
        case Windows::System::VirtualKey::Down: return 12;      // editor responds to Ctrl-L
        case Windows::System::VirtualKey::Left: return ctrlDown ? controlleft : 8;
        case Windows::System::VirtualKey::Right: return ctrlDown ? controlright : 21;     // editor responds to Ctrl-U
        case Windows::System::VirtualKey::End: return inputFIFOspecial::endkey;
        case Windows::System::VirtualKey::Home: return inputFIFOspecial::homekey;
        case Windows::System::VirtualKey::PageUp: return inputFIFOspecial::pageup;
        case Windows::System::VirtualKey::PageDown: return inputFIFOspecial::pagedown;
        }
        return -1;  // not handled
    }
};
MapVirtualKey mapper;

// force focus back on input text box
void TermCanvas::FocusInputTextBox()
{
    if (inputTextBox)
    {
        bool res = inputTextBox->Focus(Windows::UI::Xaml::FocusState::Programmatic);
        sin(1.0);
    }
}

void TermCanvas::OnSuggestionBoxRowClicked(String^ token)
{
    theContent->PushKeys(wstring(token->Data()));
    // move focus back to input text box in case it lost it (it will through a button click)
    FocusInputTextBox();
}

void TermCanvas::OnKeyDownHandler(Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e)
{
    if (suggestionBox)
    {
        const char * token = suggestionBox->HandleKeyEvent(e->Key);
        if (token)  // got something--yay! push it into the FIFO
        {
            theContent->PushKeys(string(token));
            e->Handled = true;
            return;
            // now TermRead() will return, and disable the suggestion box until it calls back in
        }
    }
    // process
    int ch = mapper.Map(e->Key, true);
    if (ch >= 0)
    {
        if (ch > 0) // got one: send
        {
            AutoLock lock(theContent);  // check for Ctrl-Z (keyboard shortcut for Undo)
            // BUGBUG: Ctrl-Z and Ctrl-Y do not make it here; editor interference?
            if (ch == 26/*Ctrl-Z*/)
            {
                if (undoenabled_canundo == 1)
                    theContent->PushUndo();
            }
            else if (ch == 25/*Ctrl-Y*/)
            {
                if (undoenabled_canredo == 1)
                    theContent->PushRedo();
            }
            else
                theContent->PushKey(ch);
        }
        e->Handled = true;
    }
}

// used to track modifier keys only
void TermCanvas::OnKeyUpHandler(Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e)
{
    int ch = mapper.Map(e->Key, false);
    if (ch >= 0)    // e.g. caught Control-Key up
        e->Handled = true;
}

void TermCanvas::OnTextChangedHandler(Object^ sender, Windows::UI::Xaml::Controls::TextChangedEventArgs^ e)
{
    DisableDetectTapForKeyboard();  // we got some input, so no need to have screen 'hot' for bringing out the keyboard

    String^ newText = inputTextBox->Text;
    inputTextBox->Text = "";
    // text was entered--we guess what was entered, and that's our input
    // This is a really bad solution, but WinRT has no better one, to my knowledge.
    // At least, we can paste text this way! (but the editor will screw up indented lines... :( )
    const wchar_t * input = newText->Begin();

    // intercept the '?' from the on-screen keyboard for the suggestion box
    // TODO: how to handle the Chinese punctuation correctly?
    // TODO: not when inside a comment!
    if ((input[0] == '?' || input[0] == 65311) && input[1] == 0 && !inLiteral)
        if (suggestionBox && suggestionBox->HandleKeyEvent((Windows::System::VirtualKey) '?'))
            return;

    AutoLock lock(theContent);
    bool previsnewline = false;
    for ( ; input[0]; input++)
    {
        int ch = input[0];
        switch(ch)  // Chinese Windows 8 gives us these
        {
        case 65306: ch = ':'; break;
        case 65311: ch = '?'; break;
        case 12290: ch = '.'; break;
        case 65292: ch = ','; break;
        case 65281: ch = '!'; break;
        }
        if ((ch < 32 || ch > 127) && ch != 13)   // anything but newlines seems to confuse it
            continue;
        // if previous was newline then insert DC1 which will undo the auto-indentation in the EDITOR
        bool inEDITORandINSERTING = true;           // TODO: are we? read out the xmode
        if (previsnewline && inEDITORandINSERTING)
            theContent->PushKey(17);    // DC1
        theContent->PushKey(ch);
        previsnewline = (ch == 13);
    }
}

int TermCanvas::iterminal_readchar(iterminal::specialmodes emulationMode)
{
    AutoLock lock(theContent);
    currentEmulationMode = emulationMode;
    inReadChar = true;
    // TODO: streamline the rest here; several mechanisms that do the same
    syntaxHighlightingRequested = (emulationMode & iterminal::specialmodes::syntaxHighlighting) != 0;

    bool emulateHomeEnd = (emulationMode & iterminal::specialmodes::emulateHomeEnd) != 0;
    bool emulatePageUpDown = (emulationMode & iterminal::specialmodes::emulatePageUpDown) != 0;
    int ch = theContent->TermRead(emulateHomeEnd, emulatePageUpDown);
    if (ch < 0)         // no character available
        return ch;
    // we will not get beyond here if no character available; i.e in that case, we
    // leave inReadChar true and currentEmulationMode set for use in refresh-timer tick

    // got a character
    suggestionBox->SetContentType(SuggestionBoxContent::none);  // new char, new game
    // Note that we cannot call Hide() since this is the wrong thread. Refresh tick will pick it up.
    inReadChar = false;
    return ch;
}

void TermCanvas::iterminal_flush()
{
    AutoLock lock(theContent);
    inReadChar = false;
    theContent->Flush();
}

void TermCanvas::iterminal_userprogchanged()
{
    AutoLock lock(theContent);
    idsSeenSoFar.clear();               // do not carry over symbols from last editing session
}

// We call this every time we poll the keyboard, so this better be fast (and lazy).
void TermCanvas::iterminal_undoenabled(bool canundo, bool canredo)
{
    AutoLock lock(theContent);
    if (undoenabled_canundo == (int) canundo && undoenabled_canredo == (int) canredo)
        return;
    // a state change: need to report to UX
    undoenabled_canundo = (int) canundo;
    undoenabled_canredo = (int) canredo;
    // Undo-enabled state has changed: call the UX so that it can reflect this in the AppBar's Undo button
    this->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this, canundo, canredo]() -> void
    {
        undoEnabledDelegate(canundo, canredo);
    }));
}

void TermCanvas::UndoRedoClicked(bool isUndo/*else redo*/)
{
    AutoLock lock(theContent);
    if (isUndo && undoenabled_canundo == 1)
        theContent->PushUndo();
    else if (!isUndo && undoenabled_canredo == 1)
        theContent->PushRedo();
}

void TermCanvas::iterminal_setcompletioninfo(const string & prefix, std::vector<std::string> && completedstrings)
{
    AutoLock lock(theContent);
    setcompletioninfo_prefix = prefix;
    setcompletioninfo_completedstrings = move(completedstrings);
    setcompletioninfo_changed = true;   // notify refresh timer to pick up the change and hand it over to the suggestion box
}

void TermCanvas::iterminal_setcommandbar(const std::string & commandbar)
{
    AutoLock lock(theContent);
    currentCommandBar = wstring(commandbar.begin(), commandbar.end());
}

void TermCanvas::iterminal_suspendresume(psnapshot * s)
{
    theContent->suspendresume(s);
    s->setprefix("TermCanvas:", -1);
    vector<wchar_t> cmdbar(currentCommandBar.begin(), currentCommandBar.end());
    s->vec("currentCommandBar", cmdbar);
    currentCommandBar.assign(cmdbar.begin(), cmdbar.end());
    s->str("setcompletioninfo_prefix", setcompletioninfo_prefix);
    s->strarray("setcompletioninfo_completedstrings", setcompletioninfo_completedstrings);
    if (s->resuming)
        setcompletioninfo_changed = true;
    s->scalar("currentEmulationMode", currentEmulationMode);
    s->scalar("inReadChar", inReadChar);
}

void TermCanvas::iterminal_pushkeys(const char * keys, size_t numkeys)
{
    AutoLock lock(theContent);
    theContent->PushKeys(std::string(keys, keys + numkeys));
}

#if 0
// install a SurfaceImageSource that TURTEGRAPHICS will render into
void TermCanvas::SetImageSource(SurfaceImageSource^ is, size_t w, size_t h)
{
    imageSource = is;
    // remove existing rectangle
    if (!drawingRect)
    {
        drawingRect = ref new Rectangle();
        drawingRect->Width = w;
        drawingRect->Height = h;
        Children->Append(drawingRect);
        Canvas::SetLeft(drawingRect, 300);
        Canvas::SetTop(drawingRect, 200);
        Canvas::SetZIndex(drawingRect, 100);
    }
    // uncomment this will draw an orange rect; but the image brush is transparent
    //drawingRect->Fill = ref new SolidColorBrush(Windows::UI::Colors::Orange);
    ImageBrush^ brush = ref new ImageBrush(); 
    brush->ImageSource = imageSource; 
    drawingRect->Fill = brush;
}
#endif

// we use intptr_t because I have no time to figure out how to pass these properly through WinRT C++/ZX
void TermCanvas::SetGraphicsTickCallback(intptr_t fn, intptr_t inst)
//void TermCanvas::SetGraphicsTickCallback(void (*fn)(void *), void * inst)
{
    GraphicsTickCallBackFunction = (SurfaceImageSource^ (*)(void *, size_t, size_t)) fn;
    GraphicsTickCallBackInstance = (void*)inst;
}

static wstring http(L"http");
static wstring https(L"https");

// overwrite text classes if we find a http link
bool TermCanvas::HighlightClickableLinks(size_t row)
{
    bool has = false;
    const auto & line = theContent->GetRowString(row);
    size_t nextpos = wstring::npos;
    for (size_t pos = 0; pos != string::npos; pos = nextpos)
    {
        // find candidate
        pos = line.find(L"://", pos);
        if (pos == wstring::npos)
            break;
        nextpos = pos +3;
        // check candidate and find start
        auto pfx = line.substr(0, pos);
        size_t spos = wstring::npos;
        if (ends_with_i(pfx, http)) spos = pos-4;
        else if (ends_with_i(pfx, https)) spos = pos-5;
        if (spos == wstring::npos)
            continue;
        // find end
        size_t epos = line.find(L" ", nextpos);
        if (epos == wstring::npos)
            epos = line.size();
        // strip trailing punctuation (e.g. the VS editor does that)
        while (epos > nextpos && !isalnum((int) line[epos-1]) && line[epos-1] != '/')
            epos--;
        if (epos == nextpos)
            continue;
        // set text class
        theContent->PutRowCharClass(row, spos, epos, TextClass::link);
        has = true;
        nextpos = epos;
    }
    return has;
}


enum parsestate { outside, incomment, instring, inidentifier };

// BUGBUG: this will fail if at the start of the page we are inside a multi-line comment
// result is written to allTextClasses, but only valid if this function returns true
// Suggestions info generated if 'pascalCompletion'
bool TermCanvas::HighlightPascalSyntax(bool pascalCompletion, bool skipTopRow,
                                       vector<vector<TextClass>> & allTextClasses,
                                       string & idprefix, set<string> & foundids, bool & inLiteral)
{
    const auto & kwset = getkwset();
    AutoLock lock(theContent);
    size_t currow = theContent->GetRow();
    size_t curcol = theContent->GetCol();
    size_t rows = theContent->term_height();
    inLiteral = false;
    wstring line;
    line.reserve(224);
    idprefix.clear();           // current id prefix at cursor position
    foundids.clear();
    parsestate state = outside;
    char commenttype = 0;       // valid only if incomment; '(' for (* or '{'
    // TODO: move this ^^ into the class, to avoid reallocations
    allTextClasses.resize(rows);
    string idstring;            // buffer for matching identifiers
    idstring.reserve(224);
    size_t numwords = 0;
    size_t numidpairs = 0;      // number of 'word space word' patterns where word is an identifier
    bool lastwasid = false;     // previous token was an identifier
    bool pascalcommentseen = false;
    for (size_t r = 0; r < rows; r++)
    {
        const auto & line = theContent->GetRowString(r);
        auto & textClasses = allTextClasses[r];
        textClasses.assign(line.size() + 1, TextClass::text);   // +1 for trailing 0 character
        if (r == 0 && skipTopRow)   // do not check or syntax-highlight top row
            continue;
        for (size_t k = 0; k <= line.size(); k++)
        {
            wchar_t ch = k < line.size() ? line[k] : 0;
            wchar_t ch2 = (k+1 < line.size()) ? line[k+1] : 0;  // next char for easy access
            wchar_t chm2 = (k > 0) ? line[k-1] : 0;             // prev char
            bool iscursorpos = (r == currow && k == curcol);
            // special case: screen begins inside a comment
            // Note: We will miss a case like
            // ... That's all, folks! }
            // since it gets masked as a string due to the apostrophe.
            bool overrun = (ch == '!'   // EDITOR overrun ends strings and comments
                && k == theContent->term_width() -1
                && k == line.size() -1);
            if (state == outside && (ch == '}' || (ch == ')' && chm2 == '*')))
            {
                // back patch all rows and chars before this as comment
                for (size_t rr = skipTopRow ? 1 : 0; rr < r; rr++)
                    allTextClasses[rr].assign(theContent->GetRowString(rr).size()+1, TextClass::comment);
                for (size_t kk = 0; kk < k; kk++)
                    textClasses[k] = TextClass::comment;
                commenttype = ch == '}' ? '{' : '(';
                lastwasid = false;
                numwords = 0;
                numidpairs = 0;
                state = incomment;  // and pretend we've been in a comment all along
            }
            // state machine
            if (state == outside)
            {
                if (chm2 == '-' && ch == '-' && ch2 == '>') // --> in the line: it's a prompt
                    return false;
                else if (ch == '\'')
                {
                    textClasses[k] = TextClass::stringliteral;
                    state = instring;
                    lastwasid = false;
                }
                else if (ch == '{')
                {
                    textClasses[k] = TextClass::comment;
                    state = incomment;
                    commenttype = '{';
                    lastwasid = false;
                }
                else if (ch == '(' && ch2 == '*')
                {
                    textClasses[k] = TextClass::comment;
                    state = incomment;
                    commenttype = '(';
                    pascalcommentseen = true;   // strong indicator for Pascal source code
                    lastwasid = false;
                }
                else if (!isinneridchar(chm2) && isfirstidchar(ch))
                {
                    state = inidentifier;
                    idstring.assign(1, (char) line[k]);       // remember the word, in uppercase
                }
                else        // not in anything
                {
                    if (!isalnum(ch))       // punctuation is highlighted as keywords
                        textClasses[k] = TextClass::keyword;
                    if (!isspace(ch))       // extend last identifier across the space
                        lastwasid = false;
                }
            }
            else if (state == instring)
            {
                textClasses[k] = TextClass::stringliteral;
                if (ch == '\'' || k == line.size() || overrun)
                    state = outside;
                if (iscursorpos)            // block suggestions inside strings
                    inLiteral = true;
            }
            else if (state == incomment)
            {
                textClasses[k] = TextClass::comment;
                if ((commenttype == '{' && ch == '}')
                    || (commenttype == '(' && ch == ')' && chm2 == '*')
                    || overrun) // overrun: assume one-line comment (which may be wrong; one way or the other we will choose wrong)
                    state = outside;
                if (iscursorpos)            // block suggestions inside comments
                    inLiteral = true;
            }
            else if (state == inidentifier)
            {
                if (overrun)    // line overrun in EDITOR: cut the identifier here
                {
                    lastwasid = true;
                    state = outside;
                }
                if (isinneridchar(ch))
                    idstring.push_back((char) line[k]);                // remember the word
                else                                            // end detected
                {
                    // highlight it according to what kind of identifier it is
                    bool iskeyword = false;
                    TextClass textClass = TextClass::text;  // identifier; // a separate value does not lkoo good
                    if (idstring.size() <= 14)                  // 14=longest keyword (IMPLEMENTATION)
                    {
                        string idstringu (idstring);
                        for (size_t kk = 0; kk < idstringu.size(); kk++)
                            idstringu[kk] = toupper((unsigned char) idstringu[kk]);     // match in uppercase
                        iskeyword = kwset.find(idstringu) != kwset.end();   // look it up
                        if (iskeyword)
                            // ugh! nasty duplication, maybe use two sets?
                            // constants/formal vars shown as keywords: idstring == "INPUT" || idstring == "OUTPUT" || idstring == "KEYBOARD" || idstring == "TRUE" || idstring == "FALSE" || idstring == "NIL" || idstring == "MAXINT"
                            if (idstringu == "INTEGER" || idstringu == "REAL" || idstringu == "CHAR" || idstringu == "BOOLEAN" || idstringu == "STRING" || idstringu == "TEXT" || idstringu == "INTERACTIVE")
                                textClass = TextClass::knowntype; // we pretend key constants and types as keywords like in C++ although they are not in Pascal
                            else
                                textClass = TextClass::keyword;
                    }
                    // change appearance if not regular text
                    if (textClass != TextClass::text)
                        for (size_t k1 = k - idstring.size(); k1 < k; k1++)
                            textClasses[k1] = textClass;
                    // statistics for deciding whether it is Pascal or not
                    numwords++;
                    iskeyword |= iscursorpos;  // we may be inserting a keyword; so don't count current partial word to avoid screen flicker
                    if (iskeyword)
                        lastwasid = false;
                    if (lastwasid)                              // two adjacent identifiers
                        numidpairs++;
                    lastwasid = !iskeyword;
                    state = outside;
                    // get current prefix at cursor position (may be keyword and/or complete id)
                    if (iscursorpos)
                        idprefix = idstring;
                    // add to identifier list (not at cursor pos since it'd be a partial id)
                    if (!iskeyword && !iscursorpos)
                    {
                        // found one --massage it a little, e.g. include ( or [
                        string idToShow = idstring;
                        // if it is followed by a ( then we shall include it as well!
                        size_t k1 = k;
                        if (k1 < line.size() && isspace(line[k1]))    // skip one space
                            k1++;
                        if (k1 < line.size())
                        {
                            if (line[k1] == '(' || line[k1] == '[' || line[k1] == '^')
                                idToShow.push_back((char)line[k1]);
                        }
                        foundids.insert(idToShow);
                    }
                    // need to reprocess this character since it is not part of the id
                    k--;
                }
            }
        }
    }

    // decide whether we think it's source code
    bool ispascal = true;
    if (!pascalcommentseen) // (* is a strong indicator
    {
        if (numidpairs > 2) // Pascal does not allow for multiple identifiers in sequence; while English has lots of that!
            ispascal = false;
    }

    return ispascal;
}

TermCanvas::~TermCanvas()
{
    // keep track that we are gone
    theTerminal = nullptr;
}

/*static*/ TermCanvas^ TermCanvas::theTerminal = nullptr;

};  // namespace

// ---------------------------------------------------------------------------
// static wrapping stuff
// ---------------------------------------------------------------------------

static ReTro_Pascal::TerminalContent * GetContent()
{
    if (ReTro_Pascal::TermCanvas::theTerminal == nullptr)
        throw std::logic_error("terminal functions used before it was created");
    return ReTro_Pascal::TermCanvas::theTerminal->theContent;
}

// the functions that the interpreter calls into
void TermOpen(int UseXTerm, int BatchFd) { }    // only used by Klebsch/Miller interpreter
void TermClose(void) { }                        // only used by Klebsch/Miller interpreter
void TermWrite(char ch, unsigned short mode) { GetContent()->TermWrite((unsigned char) ch, (mode & 0x08) != 0); }
char TermRead(void) { return GetContent()->TermRead(false, false); }    // for KM system
int TermStat(void) { return GetContent()->TermStat(); }
int term_width(void) { return GetContent()->term_width(); }             // these are the actual values
int term_height(void) { return GetContent()->term_height(); }
int term_is_batch_mode(void) { return GetContent()->term_is_batch_mode(); }
int term_PADDLE(size_t k) { return ReTro_Pascal::TermCanvas::theTerminal->PADDLE(k); }
bool term_BUTTON(size_t k) { return ReTro_Pascal::TermCanvas::theTerminal->BUTTON(k); }
void term_desired_size(size_t & cols, size_t & rows, size_t & usableCols, size_t & usableRows)
{
    auto dims = ReTro_Pascal::TermCanvas::theTerminal->GetGrantedTerminalDimensions();   // determine what's a good size of the terminal
    cols = dims[0];
    rows = dims[1];
    usableCols = dims[2];
    usableRows = dims[3];
}
void term_confirm_size(size_t screenwidth, size_t screenheight, size_t WIDTH, size_t HEIGHT)
{
    ReTro_Pascal::TermCanvas::theTerminal->ConfirmActualTerminalDimensions(screenwidth, screenheight, WIDTH, HEIGHT);
}

// these are for graphics
//void SetImageSource(SurfaceImageSource^ imageSource, size_t w, size_t h) { ReTro_Pascal::TermCanvas::theTerminal->SetImageSource(imageSource, w, h); }
//void SetGraphicsTickCallback(bool (*cb)(void *, SurfaceImageSource^ bitmapImageSource, size_t w, size_t h), void * obj) { ReTro_Pascal::TermCanvas::theTerminal->SetGraphicsTickCallback(intptr_t(cb), intptr_t(obj)); }
void SetGraphicsTickCallback(SurfaceImageSource^ (*cb)(void *, size_t w, size_t h), void * obj) { ReTro_Pascal::TermCanvas::theTerminal->SetGraphicsTickCallback(intptr_t(cb), intptr_t(obj)); }

// ---------------------------------------------------------------------------
// suspend/resume support
// ---------------------------------------------------------------------------

int SuspendTerminal(void * buf, size_t bufsize)
{
    const size_t width = term_width();
    const size_t height = term_height();
    const size_t reqsize = 5 + height * width;
    if (bufsize < reqsize)
        return -1;
    unsigned char * p = (unsigned char *) buf;
    *p++ = width;
    *p++ = height;
    *p++ = GetContent()->GetCol();
    *p++ = GetContent()->GetRow();
    *p++ = (unsigned char) ReTro_Pascal::TermCanvas::theTerminal->currentColorScheme;
    for (size_t r = 0; r < height; r++)
        for (size_t c = 0; c < width; c++)
            *p++ = (unsigned char) GetContent()->GetCharAt(c, r);
    // Note: We don't suspend the inputFIFO. It will be cleared upon reload.
    return reqsize;
}
int RestoreTerminal(const void * buf, size_t savedsize)
{
    if (savedsize < 2)
        return -1;
    unsigned char * p = (unsigned char *) buf;
    size_t w = *p++;
    size_t h = *p++;
    size_t c = *p++;
    size_t r = *p++;
    ReTro_Pascal::TerminalColorScheme colorScheme = (ReTro_Pascal::TerminalColorScheme) *p++;
    size_t reqsize = 5 + h * w;
    if (savedsize != reqsize)
        return -1;
    GetContent()->Reset();
    GetContent()->term_resize(w, h);
    ReTro_Pascal::TermCanvas::theTerminal->SetColorScheme(colorScheme);
    GetContent()->GotoXY(c, r);
    for (size_t r = 0; r < h; r++)
        for (size_t c = 0; c < w; c++)
            GetContent()->PutCharAt(c, r, *p++);
    return 0;
}
