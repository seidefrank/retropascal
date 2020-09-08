// WinRT Canvas that implements a p-system compatible terminal
// Copyright (C) 2013 Frank Seide

#pragma once

#include <vector>
#include <string>
#include <set>
#include "iterminal.h"

namespace ReTro_Pascal
{
    public enum class TerminalColorScheme { Amber, Green, Paperwhite, NumSchemes };

    enum TextClass { background, text, keyword, knowntype, identifier, stringliteral, comment, link, numClasses };

    public delegate void UndoEnabledDelegate(bool canUndo, bool canRedo);

    public ref class TermCanvas sealed : public ::Windows::UI::Xaml::Controls::Canvas
    {
        // display elements:
        //  - textDisplay       --terminal content goes here
        //  - drawingRect       --TURTLEGRAPHICS goes here
        //  - suggestionPopup   --completion box
        //  - inputTextBox      --non-special typed text is sent here and pulled out
        ref class TextDisplay^ textDisplay;                         // text display lives in here
        double renderedRowHeight, renderedCharWidth;                // size of stuff that's on-screen in textDisplay
        Windows::Foundation::Rect inputPanelOccludedRect;           // area occluded by input panel
        double extraPanelLeft;                                      // our additional key panel: left edge, or -1 if none
        double occlusionShift, occlusionUnshift0;                   // current occlusion shift
        Windows::UI::Xaml::DispatcherTimer^ DisableDetectTapForKeyboardTimer;
        void EnableDetectTapForKeyboard();
        void DisableDetectTapForKeyboard(Object^ a = nullptr, Object^ b = nullptr);
#define TEXTINPUTOFFSCREENBY 200                                    // we keep the box offscreen to avoid a visible blinking cursor / selection handle
        ::Windows::UI::Xaml::Controls::TextBox^ inputTextBox;       // we receive keyboard input here
        //std::wstring newTextBuffer;                                 // buffer for use in timer tick
        //std::wstring currentText;                                   // saved text on screen
        size_t currentCursorOffset;                                 // current cursor offset
        // TODO: should I do these callbacks with lambda syntax?
        Windows::UI::Xaml::DispatcherTimer^ RefreshTimer;           // used for periodic screen refresh, started once layout s known
        void OnRefreshTimerTick(Object^ sender, Object^ e);
        void OnKeyDownHandler(Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e);
        void OnKeyUpHandler(Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e);
        void OnTextChangedHandler(Object^ sender, Windows::UI::Xaml::Controls::TextChangedEventArgs^ e);
        void OnTappedHandler(Object^ sender, Windows::UI::Xaml::Input::TappedRoutedEventArgs^ e);
        // PADDLE() and BUTTON() support
        int mainPointer, secondPointer;                          // -1 if none; second if two pointers
        void OnPointerEventHandler(Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e, int inout);
        void OnPointerPressedHandler(Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e) { OnPointerEventHandler(e, 1); }
        void OnPointerReleasedHandler(Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e) { OnPointerEventHandler(e, -1); }
        void OnPointerExitedHandler(Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e) { OnPointerEventHandler(e, -1); }
        void OnPointerCanceledHandler(Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e) { OnPointerEventHandler(e, -1); }
        void OnPointerCaptureLostHandler(Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e) { OnPointerEventHandler(e, -1); }
        void OnPointerMovedHandler(Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e) { OnPointerEventHandler(e, 0); }
        void SetPointer(int id, double x, double y, bool left, bool middle, bool right);
        Windows::UI::Xaml::Controls::Primitives::Popup^ extraKeys;
        void CreateExtraKeys();
        void InvalidateTerminal();
        void FocusInputTextBox();
    public:
        TermCanvas(UndoEnabledDelegate^ undoEnabledDelegate);
        virtual ~TermCanvas();
        // calls from the UI
        bool Ready();       // will return false until layout is known; don't start interpreter until this returns true
        void SetColorScheme(TerminalColorScheme scheme);
        void SetBoldface(bool bold) { isBold = bold; currentFontWeight = isBold ? Windows::UI::Text::FontWeights::Bold : Windows::UI::Text::FontWeights::Normal; InvalidateTerminal(); }
        bool GetBoldface() { return isBold; }
        void SetEnableRevMenu(bool enable) { revMenuEnabled = enable; InvalidateTerminal(); }
        bool GetEnableRevMenu() { return revMenuEnabled; }
        void SetEnablePascalSyntaxHighlighting(bool enable) { syntaxHighlightingEnabled = enable; }
        bool GetEnablePascalSyntaxHighlighting() { return syntaxHighlightingEnabled; }
        void SetSizeByDimensions(size_t columns, size_t rows) { SetDesiredTerminalDimensions(columns, rows, 0); }
        void SetSizeByRowHeight(double rowHeight) { SetDesiredTerminalDimensions(0, 0, rowHeight); }
        void UndoRedoClicked(bool isUndo/*else redo*/);     // user clicked Undo or Redo button
    internal:   // terminal
        static TermCanvas^ theTerminal;
        class TerminalContent * theContent;
        int iterminal_readchar(iterminal::specialmodes emulationMode);
        void iterminal_flush();
        void iterminal_userprogchanged();
    internal:   // UNDO support (this is only about UX)
        UndoEnabledDelegate^ undoEnabledDelegate;
        int undoenabled_canundo;
        int undoenabled_canredo;
        void iterminal_undoenabled(bool canundo, bool canredo);
    internal:   // terminal, parameters for pathnamecompletion flag
        std::string setcompletioninfo_prefix;   // these two are handed through by rvalue ref
        std::vector<std::string> setcompletioninfo_completedstrings;
        bool setcompletioninfo_changed;         // set when the above were updated and need to be passed on
        void iterminal_setcompletioninfo(const std::string & prefix, std::vector<std::string> && completedstrings);
    private:    // emulation for character input
        iterminal::specialmodes currentEmulationMode;
        bool inReadChar;
    internal:   // terminal dimensions
        size_t desiredColumns, desiredRows;                                         // what user wants
        double desiredRowHeight;
        void SetDesiredTerminalDimensions(size_t columns, size_t rows, double rowHeight);
        void UpdateGrantedTerminalDimensions();
        size_t grantedColumns, grantedRows;                                         // what we can give user; this may change if screen or on-screen keyboard changes
        double grantedRowHeight;
        Platform::Array<size_t>^ GetGrantedTerminalDimensions();                    // called by interpreter to get the dimensions as asked for by the UI
        // TODO: ^^ how the f... to return a super simple (x,y) in fricking Windows 8?? Windows::Foundation::Size?
        size_t confirmedColumns, confirmedRows;                                     // terminal dimensions passed back to us from interpreter
        void ConfirmActualTerminalDimensions(size_t columns, size_t rows, size_t WIDTH, size_t HEIGHT); // called by interpreter to confirm desired uptake and resulting SYSCOM dimensions (must be values from SYSCOM)
    internal:   // terminal properties
        TerminalColorScheme currentColorScheme;                                     // colors are determined here
        bool isBold;
        Windows::UI::Text::FontWeight currentFontWeight;                            // user has requested bold/non-bold font
        bool revMenuEnabled;                                                        // menu inversion enabled?
    internal:   // size-change tracking
        double currentActualWidth, currentActualHeight;
    internal:   // syntax highlighting
        bool syntaxHighlightingEnabled;                                             // user has enabled syntax highlighting
        bool syntaxHighlightingRequested;                                           // we know from interpreter that we are possibly syntyax highlighting
        bool washighlightedpascal;                                                  // previous frame was highlighted Pascal
        bool inLiteral;                                                             // true if cursor is inside a literal or comment; do not intercept '?'
        std::vector<std::vector<TextClass>> allTextClasses;                         // buffer for syntax highlighting
        bool HighlightPascalSyntax(bool pascalCompletion, bool topRowIsMenu, std::vector<std::vector<TextClass>> & allTextClasses, std::string & idPrefix, std::set<std::string> & foundIds, bool & inLiteral);
        bool HighlightClickableLinks(size_t row);
    internal:   // predictive keyboard
        ref class SuggestionBox^ suggestionBox;                                     // suggestions are here
        void OnSuggestionBoxRowClicked(Platform::String^ token);                    // callback from button click
        std::set<std::string> idsSeenSoFar;                                         // ids seen so far in source-code display
    internal:   // command-bar highlighting
        void iterminal_setcommandbar(const std::string & commandbar);
    internal:   // suspend/resume support
        void iterminal_suspendresume(class psnapshot * s);
    internal:   // automation support
        void iterminal_pushkeys(const char * keys, size_t numkeys);
    private:
        std::wstring currentCommandBar;                                             // as determined by interpreter
    private:    // graphics functions
        Windows::UI::Xaml::Shapes::Rectangle^ drawingRect;                          // graphics shown here
        bool graphicsVisible;                                                       // graphics visible?
        Windows::UI::Xaml::Media::Imaging::SurfaceImageSource^ (*GraphicsTickCallBackFunction)(void *, size_t, size_t);
        void * GraphicsTickCallBackInstance;
        size_t PADDLEs[4];      // may be read directly
        bool BUTTONs[4];
    public:
        void TermCanvas::SetGraphicsTickCallback(intptr_t fn/*see GraphicsTickCallBackFunction()*/, intptr_t inst);
        size_t PADDLE(size_t k) { return PADDLEs[k]; }
        bool BUTTON(size_t k) { return BUTTONs[k]; }
    };

    // TODO: move all global functions in here
    // really TermCanvas should just implement this interface, but C++/ZX won't let us
    // so this is strictly a pImpl wrapper to workaround that C++/ZX limitation
    class Terminal : public ::iterminal
    {
        TermCanvas^ termCanvas;
    public:
        Terminal(TermCanvas^ termCanvas) : termCanvas(termCanvas) { }
        int readchar(specialmodes emulationmode) { return termCanvas->iterminal_readchar(emulationmode); }
        void setcommandbar(const std::string & commandbar) { termCanvas->iterminal_setcommandbar(commandbar); }
        void flush() { termCanvas->iterminal_flush(); }
        void userprogchanged() { termCanvas->iterminal_userprogchanged(); }
        void undoenabled(bool canundo, bool canredo) { termCanvas->iterminal_undoenabled(canundo, canredo); }
        void setcompletioninfo(const std::string & prefix, std::vector<std::string> && completedstrings)  { return termCanvas->iterminal_setcompletioninfo(prefix, std::move(completedstrings)); }
        void suspendresume(class psnapshot * s) { termCanvas->iterminal_suspendresume(s); }
        void pushkeys(const char * keys, size_t numkeys) { termCanvas->iterminal_pushkeys(keys, numkeys); }
        ~Terminal() { }
    };
};
