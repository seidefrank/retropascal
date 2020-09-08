//
// MainPage.xaml.cpp
// Implementation of the MainPage class.
//

// AppBar icon images: http://msdn.microsoft.com/en-us/library/windows/apps/xaml/jj841127.aspx

// TODO:
//  - resume after suspend without termination--what will happen? Will I handle it correctly, or need code?
//  - need to do the Resume() at a later stage, once the screen dimensions are known

#define _CRT_SECURE_NO_WARNINGS 1

#include <collection.h>
#include "await.h"
#include "winrt/interpreter_thread.h"
#include "App.xaml.h"
#include "MainPage.xaml.h"
#include "SettingsAbout.xaml.h"
#include "DisplaySettings.xaml.h"
#include <stdio.h>
#include <io.h>
#include <sys/stat.h>
#include <string>
#include <vector>

#include <ppltasks.h>       // for playing a sound file
using namespace concurrency;

using namespace std;

using namespace ReTro_Pascal;

using namespace Platform;
using namespace Windows::ApplicationModel;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage;
using namespace Windows::UI::ApplicationSettings;
using namespace Windows::UI::Popups;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Media::Imaging;
using namespace Windows::UI::Xaml::Navigation;

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=234238

MainPage::MainPage()
{
    InitializeComponent();

    // use textured background
    //auto brush = ref new ImageBrush();
    //brush->ImageSource = ref new BitmapImage(ref new Uri("ms-appx:///Assets/Manual Background.png"));
    //BottomAppBar->Background = brush;

    // add the canvas
    termCanvas = ref new ReTro_Pascal::TermCanvas(ref new ReTro_Pascal::UndoEnabledDelegate([this](bool canUndo, bool canRedo) -> void
    {
        // we pass a lambda that is called when the Undo-enabled state changes
        UndoButton->IsEnabled = canUndo;
        RedoButton->IsEnabled = canRedo;
    }));
    Content = termCanvas;

    // set visual options from persisted settings
    DisplaySettings::LoadPersistedSettings(termCanvas);

    // settings
    //this->commandsRequestedEventRegistrationToken =
    SettingsPane::GetForCurrentView()->CommandsRequested += ref new TypedEventHandler<SettingsPane^, SettingsPaneCommandsRequestedEventArgs^>(this, &MainPage::onSettingsRequested);

    // set resuming/suspending handlers
    Application::Current->Resuming += ref new EventHandler<Object^>([this](Object^, Object^) -> void
    {
        ResumeInterpreter(0);
    });
    Application::Current->Suspending += ref new SuspendingEventHandler([this](Object^, Windows::ApplicationModel::ISuspendingEventArgs^ e) -> void
    {
        auto deferral = e->SuspendingOperation->GetDeferral();
        Suspend(ref new VoidOfVoidDelegate([this,deferral]() -> void
        {
            deferral->Complete();
        }));
    });
}

// settings
void MainPage::onSettingsRequested(Windows::UI::ApplicationSettings::SettingsPane^ settingsPane, Windows::UI::ApplicationSettings::SettingsPaneCommandsRequestedEventArgs^ eventArgs)
{
    auto generalCommand = ref new SettingsCommand("display", "Display", ref new UICommandInvokedHandler(this, &MainPage::ShowDisplaySettingsFlyout));
    eventArgs->Request->ApplicationCommands->Append(generalCommand);

    auto helpCommand = ref new SettingsCommand("about", "About", ref new UICommandInvokedHandler(this, &MainPage::ShowSettingsAboutFlyout));
    eventArgs->Request->ApplicationCommands->Append(helpCommand);
}

void MainPage::NewSettingsFlyout(UserControl^ child)
{
    const double settingsWidth = 346;
    auto windowBounds = Window::Current->Bounds;
    auto popup = ref new Popup();
    popup->IsLightDismissEnabled = true;
    popup->Width = settingsWidth;
    popup->Height = windowBounds.Height;
    child->Width = settingsWidth;
    child->Height = windowBounds.Height;
    popup->Child = child;
    popup->SetValue(Canvas::LeftProperty, windowBounds.Width - settingsWidth);
    popup->SetValue(Canvas::TopProperty, 0.0);
    popup->IsOpen = true;
}

void MainPage::ShowDisplaySettingsFlyout(Windows::UI::Popups::IUICommand^ command)
{
    NewSettingsFlyout(ref new DisplaySettings(termCanvas));
}

void MainPage::ShowSettingsAboutFlyout(Windows::UI::Popups::IUICommand^ command)
{
    NewSettingsFlyout(ref new SettingsAbout());
}

/// <summary>
/// Invoked when this page is about to be displayed in a Frame.
/// </summary>
/// <param name="e">Event data that describes how this page was reached.  The Parameter
/// property is typically used to configure the page.</param>
void MainPage::OnNavigatedTo(NavigationEventArgs^ args)
{
    auto thisApp = dynamic_cast<App^> (Application::Current); // how to get our own app correctly?
    auto previousExecutionState = thisApp ? thisApp->FetchPreviousExecutionState() : Windows::ApplicationModel::Activation::ApplicationExecutionState::NotRunning;
    // create the interpreter objects; also copy the disk images
    CreatePMachine();
    // kick it off
    InitialResume(previousExecutionState == Windows::ApplicationModel::Activation::ApplicationExecutionState::Terminated);
    // TODO: can we use LayoutChanged event for the next steps?
}

void MainPage::InitialResume(bool restoreFromDisk)
{
#if 0   // disabled for now
    restoreFromDisk = false;
#endif
    // if we are not ready then we will delay a little
    // This is evil. Need to figure out the correct sequence of events to start the VM.
    // I need an event that is guaranteed to be before the first user input but after layout has been done.
    if (!termCanvas->Ready())
    {
        auto timer = ref new DispatcherTimer();
        TimeSpan ts;
        ts.Duration = 100000;   // 10 ms
        timer->Interval = ts;
        timer->Tick += ref new EventHandler<Object^>([this, timer, restoreFromDisk](Object^, Object^) -> void
        {
            timer->Stop();      // to stop timer
            InitialResume(restoreFromDisk);
        });
        timer->Start();         // to start timer
        return;
    }

    // OK, we are ready (TermCanvas has known size and is set up)
    Beep();                 // for true Apple feeling!
    if (restoreFromDisk)
        Resume(ref new ReTro_Pascal::VoidOfVoidDelegate([this]() -> void { ResumeInterpreter(0); }));
    else
    {
        interpreter->BootLoader();
#ifdef _DEBUG
        ResumeInterpreter(200);
#else
        ResumeInterpreter(3000);    // leave a few seconds for welcome message to be seen
#endif
    }
}

void MainPage::ResumeInterpreter(int waitms)
{
    interpreter->Resume(waitms, ref new ReTro_Pascal::HaltCallback([this](String^ message) -> void
    {
        OnHalted(message);     // we should not get here, but if the P system halts, we do; this function just terminates
    }));
}

// p-system halted--should never happen, so we crash
// We will give user 3 seconds to see the message, and then die.
// TODO: have an exception string here!
void MainPage::OnHalted(String^ message)
{
    auto timer = ref new DispatcherTimer();
    TimeSpan ts;
    ts.Duration = 30000000; // freeze the screen for 3 seconds
    timer->Interval = ts;
    timer->Tick += ref new EventHandler<Object^>([this, timer, message](Object^, Object^) -> void
    {
        timer->Stop(); // to stop timer
        String^ text = message;
        if (text)
            text = "Exception caught:\r\n\r\n" + text + "\r\n\r\n";
        else
            text = "";
        text += "Virtual machine halted. We exit.";
        Panic(text);
    });
    timer->Start(); // to start timer
}

void MainPage::Beep()     // an Apple beep :)
{
    // we use our await() hack, which does not work on the UI thread directly (deadlock)
    // so pass it to a different thread with a timer
    auto timer = ref new DispatcherTimer();
    TimeSpan ts;
    ts.Duration = 1000;   // 0.1 ms
    timer->Interval = ts;
    timer->Tick += ref new EventHandler<Object^>([this, timer](Object^, Object^) -> void
    {
        timer->Stop();      // to stop timer
        // play it
        auto package = Windows::ApplicationModel::Package::Current;
        auto installedLocation = package->InstalledLocation;
        auto storageFile = await(installedLocation->GetFileAsync("Assets\\applebeep.mp3"));
        auto stream = await(storageFile->OpenAsync(FileAccessMode::Read));
        auto snd = ref new MediaElement();
        snd->SetSource(stream, storageFile->ContentType);
        // BUGBUG: this is not playing anything; maybe because I am destructing snd?
        snd->Volume = 1.0;  // did not help
        snd->Play();
    });
    timer->Start();         // to start timer
}
        //auto sound = ref new Audio();
#if 0
public async void PlayLaserSound()
    {
        var package = Windows.ApplicationModel.Package.Current;
        var installedLocation = package.InstalledLocation;
        var storageFile = await installedLocation.GetFileAsync("Assets\\Sounds\\lazer.mp3");
        if (storageFile != null)
        {
            var stream = await storageFile.OpenAsync(Windows.Storage.FileAccessMode.Read);
            MediaElement snd = new MediaElement();
            snd.SetSource(stream, storageFile.ContentType);
            snd.Play();
        }
    }
#endif

void MainPage::MessageBox(String^ message, Windows::UI::Popups::UICommand^ uiCommand, Windows::UI::Popups::UICommand^ uiCommand2)
{
    auto msg = ref new Windows::UI::Popups::MessageDialog(message);
    msg->Commands->Append(uiCommand);
    if (uiCommand2)
        msg->Commands->Append(uiCommand2);
    msg->ShowAsync();
}

void MainPage::Terminate(Windows::UI::Popups::IUICommand ^) // why why why don't they give samples with lambdas??
{
    Application::Current->Exit();
}

void MainPage::Panic(String^ message)
{
#ifdef _DEBUG
    MessageBox(message + "\r\n\r\nThis application will now terminate. The Release build will not show this message, it will just crash as required per AppStore rules.",
        ref new Windows::UI::Popups::UICommand("Terminate Application", ref new Windows::UI::Popups::UICommandInvokedHandler(this, &MainPage::Terminate)));
#else   // I must follow the "Crash Experience"
    Application::Current->Exit();
#endif
}

void MainPage::Notify(String^ message)
{
    MessageBox(message, ref new Windows::UI::Popups::UICommand("Dismiss"));
}

bool copyfile(const wchar_t * from, const wchar_t * to)
{
    // not sure if WinRT has a synchronous copy-file API, so we just roll our own
    bool ok = true;
    FILE * fin = NULL;
    FILE * fout = NULL;
    wstring totmp = wstring(to) + L".tmp$$";
    fin = _wfopen(from, L"rb");
    ok &= (fin != NULL);
    if (ok)
        fout = _wfopen(totmp.c_str(), L"wb");
    ok &= (fout != NULL);

    // copy loop  --looks naive but is fast due to buffering
    while (ok)
    {
        int ch = fgetc(fin);
        if (ch == EOF)
        {
            ok &= !ferror(fin);
            break;
        }
        ch = fputc(ch, fout);
        ok &= ch != EOF;
    }

    // done
    ok &= (fflush(fout) == 0);      // fclose() also flushes, but we do it here to check for the error

    // done
    if (fin) fclose(fin);
    if (fout) fclose(fout);
    ok &= _wunlink(to) == 0 || errno == ENOENT; // now move: first delete existing if any
    ok &= _wrename(totmp.c_str(), to) == 0;     // now move it over
    if (!ok) _wunlink(totmp.c_str());           // delete so we can try agin; and if this fails, I cannot help it
    return ok;
}

// returns the target disk folder path, or nullptr if the disk was not created
Platform::String^ MainPage::CreateDisk(size_t u, const wchar_t * name) const
{
    auto installDir = Package::Current->InstalledLocation->Path;
    //auto appDataDir = (u != 12) ? ApplicationData::Current->LocalFolder->Path : ApplicationData::Current->RoamingFolder->Path;
    auto appDataDir = ApplicationData::Current->LocalFolder->Path;
    auto srcDir = installDir + "\\Volumes";
    auto destDir = appDataDir + L"\\Disks";
    if (_wmkdir(destDir->Data()) != 0 && errno != EEXIST)
        return nullptr;     // failed to make Volumes dir
    auto source = srcDir + "\\" + ref new String(name);
    wchar_t numbuf[80];
    auto dest = destDir + "\\"+ ref new String(_itow(u, numbuf, 10));   // volume folder is the unit number
    struct _stat64i32 srcstat;
    // if input does not exist then we just create an empty folder
    if (_wmkdir(dest->Data()) != 0
//#if 1
//        && wcscmp(name, L"SOURCES")
//#endif
        )
    {
        if (errno == EEXIST)    // already exists--leave it, it was prepared before
            return dest;
        else
            return nullptr;     // an actual error
    }
    // write the $VOLNAME.volumename file  --this determines the vol name displayed inside the system
    auto volnamefile = dest + "\\$" + ref new String(name) + ".volumename";
    FILE * f = _wfopen(volnamefile->Data(), L"wb");
    if (f)
        fclose(f);
    // else ... not much we can do actually
    // if source does not exist, then we are done, nothing to copy
    if (_wstat64i32(source->Data(), &srcstat) != 0)
        return dest;
    // if source is a single file then it is a volume
    if (!(srcstat.st_mode & _S_IFDIR))
    {
        if (copyfile(source->Data(), dest->Data()))
            return dest;
        else
            return nullptr;
    }
    // if source is a folder, then we create it and copy it
    _wfinddatai64_t finddata;
    bool done = false;
    for(intptr_t h = _wfindfirsti64((source + L"\\*")->Data(), &finddata); h && !done; done = _wfindnexti64(h, &finddata) < 0)
    {
        if (finddata.attrib & (0x10/*_A_SUBDIR*/ | 2/*_A_HIDDEN*/ | 4/*_A_SYSTEM*/))
            continue;
        else
        {
#if 0
            if (finddata.name[0] != 'a' && finddata.name[0] != 'g' && finddata.name[0] != 'l'
                && finddata.name[0] != 't' && finddata.name[0] != 'd' && finddata.name[0] != 'o' && wcscmp(name, L"SOURCES") == 0)    // all, lib, globals
                continue;
#endif
            String^ suffix = L"\\" + ref new String(finddata.name);
            copyfile((source + suffix)->Data(), (dest + suffix)->Data());
        }
    }
    // BUGBUG: we don't recover if copying fails for some files
    return dest;
}

void MainPage::CreatePMachine()
{
    auto installDir = Windows::ApplicationModel::Package::Current->InstalledLocation->Path;
    auto appDataDir = Windows::Storage::ApplicationData::Current->LocalFolder->Path;
#if 0
    auto dp1 = appDataDir + "\\APPLE";
    auto dp2 = appDataDir + "\\SAMPLES";
    auto dp3 = appDataDir + "\\USER";
    auto dp4 = appDataDir + "\\UTILITY"; // the Miller disk
#else
#if 0   // Apple disks
    auto dp1 = installDir + "\\pascal0.dsk";
    auto dp2 = installDir + "\\pascal1.dsk";
    auto dp3 = installDir + "\\pascal3.dsk";
    auto dp4 = nullptr;//appDataDir + "\\USER";
#else   // UCSD II.0 disks (by Miller)
    auto dp1 = installDir + "\\system.vol";
    auto dp2 = installDir + "\\utility.vol";
    //auto dp1 = appDataDir + "\\SYSTEM";
    //auto dp2 = appDataDir + "\\UTILITY";
    auto dp3 = appDataDir + "\\USER";
    auto dp4 = appDataDir + "\\SAMPLES";
#endif
#endif
    // UCSD II.0 disks from Miller's project
    //const wchar_t * disks[] = { L"SYSTEM.VOL", L"UTILITY.VOL", L"USER", L"SAMPLES", L"SOURCES" };
    //const wchar_t * disks[] = { L"SYSTEM.VOL", L"PASCAL3.DSK", L"USER", L"SAMPLES", L"SOURCES" };
    //const wchar_t * disks[] = { L"APPLE0", L"PASCAL3.DSK", L"USER", L"SAMPLES", L"SOURCES" };
    //const wchar_t * disks[] = { L"PASCAL0.DSK", L"PASCAL3.DSK", L"UTILITY.VOL", L"USER", /*L"SAMPLES",*/ L"SOURCES" };
    //const wchar_t * disks[] = { L"APPLE0", L"C", L"MILLER", L"USER", L"SOURCES" };
    //const wchar_t * disks[] = { L"PASCAL0.DSK", L"PASCAL1.DSK", L"PASCAL2.DSK", L"USER", L"PASCAL3.DSK" };
    const wchar_t * disks[] = { L"RETRO", L"USER", L"DATA", L"SAMPLES", L"SOURCES" };

    const size_t numDisks = _countof(disks);
    auto diskPaths = ref new Array<String^>(numDisks);
    size_t u = 4;
    for (size_t i = 0; i < numDisks; i++)
    {
        auto dp = CreateDisk(u, disks[i]);
        if (dp == nullptr)
        {
            ;   // message box  --or crash at this point? or just ignore? In deployed app, there should be no message
            continue;
        }
        diskPaths[i] = dp;
        u++;
        if (u == 6)         // units #6..#8 are not disks
            u = 9;
        else if (u == 13)   // too many
            break;
    }
    terminal.reset(new ReTro_Pascal::Terminal(termCanvas));
    interpreter.reset(new ReTro_Pascal::InterpreterThread(*terminal.get(), diskPaths));    // constructs p-code engine
}


// pop-up menus
static void setPopupState(MainPage^ canvas, Popup^ popup, StackPanel^ panel, Button^ button, bool visible)
{
    if (visible)
    {
        panel->Background = canvas->BottomAppBar->Background;
        panel->Transitions = ref new Windows::UI::Xaml::Media::Animation::TransitionCollection();
        panel->Transitions->Append(ref new Windows::UI::Xaml::Media::Animation::EntranceThemeTransition());
        auto buttonCenterX = button->TransformToVisual(Window::Current->Content)->TransformPoint(Point(0,0)).X + button->DesiredSize.Width/2;
        panel->Measure(Windows::Foundation::Size(100000, 20000));   // determine DesiredSize of the stackpanel that is our menu
        // Double.PositiveInfinity?
        auto panelSize = panel->DesiredSize;                        // BUGBUG: this does not give correct width; is always 40 (margins? not measured the actual text?)
        popup->HorizontalOffset = max(buttonCenterX - panelSize.Width/2 - 20, 0);
        popup->VerticalOffset = Window::Current->CoreWindow->Bounds.Bottom - canvas->BottomAppBar->ActualHeight - panelSize.Height - 4;
    }
    //popup->Visibility = visible ? Visibility::Visible : Visibility::Collapsed;  // TODO: needed?
    popup->IsOpen = visible;
    if (!visible)
        canvas->BottomAppBar->IsOpen = false;           // TODO: do this when the popup closes
}
#if 0
void ReTro_Pascal::MainPage::ResolutionPopup_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    setPopupState(this, resolutionPopup, resolutionPopupPanel, ResolutionPopup_Button, true);
    // TODO: a close function that replaces setPopupState(false,...) below, maybe pass the action as a lambda here
}

void ReTro_Pascal::MainPage::ResolutionPopup_24x40(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    setPopupState(this, resolutionPopup, resolutionPopupPanel, ResolutionPopup_Button, false);
    termCanvas->SetSizeByDimensions(40, 24);
}

void ReTro_Pascal::MainPage::ResolutionPopup_24x80(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    setPopupState(this, resolutionPopup, resolutionPopupPanel, ResolutionPopup_Button, false);
    termCanvas->SetSizeByDimensions(80, 24);
}

void ReTro_Pascal::MainPage::ResolutionPopup_Big(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    setPopupState(this, resolutionPopup, resolutionPopupPanel, ResolutionPopup_Button, false);
    termCanvas->SetSizeByRowHeight(21);
}

void ReTro_Pascal::MainPage::ResolutionPopup_Medium(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    setPopupState(this, resolutionPopup, resolutionPopupPanel, ResolutionPopup_Button, false);
    termCanvas->SetSizeByRowHeight(15);  // this leaves 24 rows on half the screen (on-screen keyboard visible)
}

void ReTro_Pascal::MainPage::ResolutionPopup_Small(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    setPopupState(this, resolutionPopup, resolutionPopupPanel, ResolutionPopup_Button, false);
    termCanvas->SetSizeByRowHeight(11);
}

void ReTro_Pascal::MainPage::colorPopup_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    setPopupState(this, colorPopup, colorPopupPanel, colorPopup_Button, true);
}

void ReTro_Pascal::MainPage::colorPopup_Amber(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    setPopupState(this, colorPopup, colorPopupPanel, colorPopup_Button, false);
    termCanvas->SetColorScheme(ReTro_Pascal::TerminalColorScheme::Amber);
}

void ReTro_Pascal::MainPage::colorPopup_Green(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    setPopupState(this, colorPopup, colorPopupPanel, colorPopup_Button, false);
    termCanvas->SetColorScheme(ReTro_Pascal::TerminalColorScheme::Green);
}

void ReTro_Pascal::MainPage::colorPopup_Paperwhite(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    setPopupState(this, colorPopup, colorPopupPanel, colorPopup_Button, false);
    termCanvas->SetColorScheme(ReTro_Pascal::TerminalColorScheme::Paperwhite);
}
#endif

// suspend/resume
static bool PutBlob(const wchar_t * path, const vector<unsigned char> & blob)
{
    bool ok = true;
    FILE * f = _wfopen(path, L"wb");
    ok &= f != NULL;
    ok &= fwrite(blob.data(), blob.size(), 1, f) == 1;
    ok &= fflush(f) == 0;
    if (f) fclose(f);
    return ok;
}

static bool GetBlob(const wchar_t * path, vector<unsigned char> & blob)
{
    bool ok = true;
    FILE * f = _wfopen(path, L"rb");
    ok &= f != NULL;
    __int64 len = ok ? _filelengthi64(_fileno(f)) : -1;
    ok &= len >= 0;
    if (ok) try { blob.resize((size_t) len); } catch(...) { ok = false; }
    ok &= fread(blob.data(), blob.size(), 1, f) == 1;
    if (f) fclose(f);
    return ok;
}

void MainPage::Suspend(VoidOfVoidDelegate^ callback)
{
    interpreter->Suspend(ref new ReTro_Pascal::SuspendSaveStateCallback([this,callback](const Platform::Array<unsigned char>^ suspendBlob) -> void
    {
        auto appDataDir = Windows::Storage::ApplicationData::Current->LocalFolder->Path;
        auto suspendFile = appDataDir + "\\SuspendPackage.dat";
        // if nullptr then delete the file otherwise save it
        if (suspendBlob)
        {
            std::vector<unsigned char> blob(suspendBlob->Data, suspendBlob->Data + suspendBlob->Length);   // if we use array_ref, we can save the copy, but does not matter now
            if (!PutBlob(suspendFile->Data(), blob))
                Panic(L"error writing suspending data to disk--data is lost");  // unrecoverable, since this realy only happens during life cycle management
        }
        // no data to write or failed to write
        else
            _wunlink(suspendFile->Data());    // we ignore the return code since there's not much we can do anyway
        // call back
        if (callback)
            callback();
    }));
}

void MainPage::Resume(VoidOfVoidDelegate^ callback)
{
    interpreter->Abort(ref new ReTro_Pascal::AbortCallback([this, callback]() -> void  // abort current execution
    {
        auto appDataDir = Windows::Storage::ApplicationData::Current->LocalFolder->Path;
        auto suspendFile = appDataDir + "\\SuspendPackage.dat";
        // load suspend package
        std::vector<unsigned char> blob;
        Platform::Array<unsigned char>^ suspendBlob;
        if (GetBlob(suspendFile->Data(), blob))
            suspendBlob = ref new Platform::Array<unsigned char>(blob.data(), blob.size());
        else
            suspendBlob = nullptr;
        _wunlink(suspendFile->Data());    // now delete it since we just consumed it
        if (suspendBlob == nullptr)
#ifdef _DEBUG
            Notify("Failed to restore the virtual machine from file. Starting virgin machine.");
#else
            ;   // not shown in Release
#endif
        else try    // restore machine from file
        {
            interpreter->Restore(suspendBlob);
        }
        catch (...)
        {
            Panic("Restoring the virtual machine failed, due to an invalid file. The app will now close.");
        }
        // continue execution
        if (callback)
            callback();
    }));
}

void ReTro_Pascal::MainPage::Undo_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    termCanvas->UndoRedoClicked(true);
}

void ReTro_Pascal::MainPage::Redo_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    termCanvas->UndoRedoClicked(false/*redo*/);
}

void ReTro_Pascal::MainPage::SuspendTestButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    Suspend(nullptr);
}

void ReTro_Pascal::MainPage::ResumeTestButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    Resume(ref new ReTro_Pascal::VoidOfVoidDelegate([this]() -> void { ResumeInterpreter(0); }));
}

void ReTro_Pascal::MainPage::SoftReset_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    MessageBox("'Halt Program' will terminate the currently running Pascal program and restart the Pascal system.\r\n\r\nIs this really what you want to do?",
        ref new Windows::UI::Popups::UICommand("Yes, Terminate Program", ref new Windows::UI::Popups::UICommandInvokedHandler([this](Windows::UI::Popups::IUICommand^) -> void
        {
            Beep();
            interpreter->SoftReset();   // issue a soft reset
        })),
        ref new Windows::UI::Popups::UICommand("No, Keep it Running", ref new Windows::UI::Popups::UICommandInvokedHandler([](Windows::UI::Popups::IUICommand^) -> void
        {
            // do nothing
        })));
}

#if 0
// insane--no toggle button for AppBar! WORKAROUND
void ReTro_Pascal::MainPage::BoldOn_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    auto button = dynamic_cast<Button^> (sender);
    BoldOn_Button->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    BoldOff_Button->Visibility = Windows::UI::Xaml::Visibility::Visible;
    termCanvas->SetBoldface(true);
}

void ReTro_Pascal::MainPage::BoldOff_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    auto button = dynamic_cast<Button^> (sender);
    BoldOn_Button->Visibility = Windows::UI::Xaml::Visibility::Visible;
    BoldOff_Button->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    termCanvas->SetBoldface(false);
}

void ReTro_Pascal::MainPage::RevMenuOn_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    auto button = dynamic_cast<Button^> (sender);
    RevMenuOn_Button->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    RevMenuOff_Button->Visibility = Windows::UI::Xaml::Visibility::Visible;
    termCanvas->SetEnableRevMenu(true);
}

void ReTro_Pascal::MainPage::RevMenuOff_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    auto button = dynamic_cast<Button^> (sender);
    RevMenuOn_Button->Visibility = Windows::UI::Xaml::Visibility::Visible;
    RevMenuOff_Button->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    termCanvas->SetEnableRevMenu(false);
}
#endif
