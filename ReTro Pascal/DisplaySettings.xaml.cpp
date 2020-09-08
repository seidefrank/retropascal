//
// DisplaySettings.xaml.cpp
// Implementation of the DisplaySettings class
//

#define _CRT_SECURE_NO_WARNINGS 1

#include "DisplaySettings.xaml.h"
#include <stdio.h>
#include <string>

using namespace ReTro_Pascal;
using namespace std;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Media::Imaging;
using namespace Windows::UI::Xaml::Navigation;

// The User Control item template is documented at http://go.microsoft.com/fwlink/?LinkId=234236

#define FS_BIG 21
#define FS_MEDIUM 15
#define FS_TINY 11
#define FS_CLASSIC 24080

struct PersistedSettings        // we save these settings to disk
{
    int fontSize;               // size of font; >= 1000 (e.g. 24080) to set by dimension
    ReTro_Pascal::TerminalColorScheme colorScheme;
    bool boldface;              // use bold font
    bool revCommandBar;         // command bar shown in reverse video
    bool syntaxHighlighting;    //

    static wstring getpath()    // pathname of persisted file
    {
        auto appDataDir = Windows::Storage::ApplicationData::Current->RoamingFolder->Path;
        wstring path(appDataDir->Data());
        path += L"\\Settings.dat";
        return path;
    }

    // defaults:
    PersistedSettings()
    {
        fontSize = FS_MEDIUM;
        colorScheme = ReTro_Pascal::TerminalColorScheme::Amber;
        boldface = true;
        revCommandBar = true;
        syntaxHighlighting = true;
    }

    void load()
    {
        // try to open:
        FILE * f = _wfopen(getpath().c_str(), L"rb");
        if (f)
        {
            PersistedSettings second;
            if (fread(&second, sizeof(second), 1, f) == 1)
                if (((fontSize >= 10 && fontSize < 30) || fontSize == FS_CLASSIC)    // validate
                    && colorScheme < TerminalColorScheme::NumSchemes)
                    *this = second; // successfully read
            fclose(f);
        }
    }

    void save()
    {
        FILE * f = _wfopen(getpath().c_str(), L"wb");
        if (f)
        {
            PersistedSettings second;
            fwrite(this, sizeof(*this), 1, f);  // we just ignore error codes, nothing else we can really do here
            fclose(f);
        }
    }
};

// we just store the persisted settings in a static variable
static PersistedSettings persistedSettings;

DisplaySettings::DisplaySettings(ReTro_Pascal::TermCanvas^ c) : termCanvas(c)
{
    InitializeComponent();
    //auto brush = ref new ImageBrush();
    //brush->ImageSource = ref new BitmapImage(ref new Uri("ms-appx:///Assets/Manual Background.png"));
    //brush->Stretch = Stretch::UniformToFill;
    //HeaderBackground->Background = brush;

    // load settings and set them up
    // Note: This may update sync'ed settings.
    LoadPersistedSettings(c);

    // set the controls
    CSAmber->IsChecked = persistedSettings.colorScheme == ReTro_Pascal::TerminalColorScheme::Amber;
    CSGreen->IsChecked = persistedSettings.colorScheme == ReTro_Pascal::TerminalColorScheme::Green;
    CSWhite->IsChecked = persistedSettings.colorScheme == ReTro_Pascal::TerminalColorScheme::Paperwhite;
    TSBig->IsChecked = persistedSettings.fontSize == FS_BIG;
    TSMed->IsChecked = persistedSettings.fontSize == FS_MEDIUM;
    TSTiny->IsChecked = persistedSettings.fontSize == FS_TINY;
    TSClassic->IsChecked = persistedSettings.fontSize == FS_CLASSIC;
    Boldface->IsOn = persistedSettings.boldface;
    EnableRevMenu->IsOn = persistedSettings.revCommandBar;
    PascalSyntaxHighlighting->IsOn = persistedSettings.syntaxHighlighting;
}

/*static*/ void DisplaySettings::LoadPersistedSettings(ReTro_Pascal::TermCanvas^ termCanvas)
{
    // load the file
    persistedSettings.load();

    // apply all settings
    ApplySettings(termCanvas);
}

/*static*/ void DisplaySettings::ApplySettings(ReTro_Pascal::TermCanvas^ termCanvas)
{
    // apply all settings
    if (persistedSettings.fontSize < 1000)
        termCanvas->SetSizeByRowHeight(persistedSettings.fontSize);
    else
        termCanvas->SetSizeByDimensions(persistedSettings.fontSize % 1000, persistedSettings.fontSize / 1000);
    termCanvas->SetColorScheme(persistedSettings.colorScheme);
    termCanvas->SetBoldface(persistedSettings.boldface);
    termCanvas->SetEnableRevMenu(persistedSettings.revCommandBar);
    termCanvas->SetEnablePascalSyntaxHighlighting(persistedSettings.syntaxHighlighting);

    // and persist them
    persistedSettings.save();
}

void ReTro_Pascal::DisplaySettings::SettingsBackClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    auto popup = dynamic_cast<Popup^>(Parent);
    if (popup)
        popup->IsOpen = false;
    // this crashes
    //SettingsPane::Show();   // this seems to just show the Settings charm page
}

#if 0
void ReTro_Pascal::DisplaySettings::ResolutionPopup_24x40(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    termCanvas->SetSizeByDimensions(40, 24);
}

void ReTro_Pascal::DisplaySettings::ResolutionPopup_24x80(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    persistedSettings.fontSize = FS_CLASSIC;
    ApplySettings(termCanvas);
}

void ReTro_Pascal::DisplaySettings::ResolutionPopup_Big(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    persistedSettings.fontSize = FS_BIG;
    ApplySettings(termCanvas);
}

void ReTro_Pascal::DisplaySettings::ResolutionPopup_Medium(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    persistedSettings.fontSize = FS_MEDIUM;  // this leaves 24 rows on half the screen (on-screen keyboard visible)
    ApplySettings(termCanvas);
}

void ReTro_Pascal::DisplaySettings::ResolutionPopup_Small(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    persistedSettings.fontSize = FS_TINY;
    ApplySettings(termCanvas);
}

void ReTro_Pascal::DisplaySettings::colorPopup_Amber(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    persistedSettings.colorScheme = ReTro_Pascal::TerminalColorScheme::Amber;
    ApplySettings(termCanvas);
}

void ReTro_Pascal::DisplaySettings::colorPopup_Green(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    persistedSettings.colorScheme = ReTro_Pascal::TerminalColorScheme::Green;
    ApplySettings(termCanvas);
}

void ReTro_Pascal::DisplaySettings::colorPopup_Paperwhite(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    persistedSettings.colorScheme = ReTro_Pascal::TerminalColorScheme::Paperwhite;
    ApplySettings(termCanvas);
}

void ReTro_Pascal::DisplaySettings::BoldOn_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    persistedSettings.boldface = true;
    ApplySettings(termCanvas);
}

void ReTro_Pascal::DisplaySettings::BoldOff_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    persistedSettings.boldface = false;
    ApplySettings(termCanvas);
}

void ReTro_Pascal::DisplaySettings::RevMenuOn_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    persistedSettings.revCommandBar = true;
    ApplySettings(termCanvas);
}

void ReTro_Pascal::DisplaySettings::RevMenuOff_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    persistedSettings.revCommandBar = false;
    ApplySettings(termCanvas);
}
#endif

void ReTro_Pascal::DisplaySettings::BoldfaceToggled(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    auto sw = dynamic_cast<ToggleSwitch^> (sender);
    persistedSettings.boldface = sw->IsOn;
    ApplySettings(termCanvas);
}

void ReTro_Pascal::DisplaySettings::EnableRevMenuToggled(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    auto sw = dynamic_cast<ToggleSwitch^> (sender);
    persistedSettings.revCommandBar = sw->IsOn;
    ApplySettings(termCanvas);
}

void ReTro_Pascal::DisplaySettings::PascalSyntaxHighlightingToggled(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    auto sw = dynamic_cast<ToggleSwitch^> (sender);
    persistedSettings.syntaxHighlighting = sw->IsOn;
    ApplySettings(termCanvas);
}

void ReTro_Pascal::DisplaySettings::OnSetColorScheme(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    auto butt = dynamic_cast<RadioButton^> (sender);
    auto colorScheme = TerminalColorScheme::NumSchemes;
    if (butt == CSAmber)
        colorScheme = TerminalColorScheme::Amber;
    else if (butt == CSGreen)
        colorScheme = TerminalColorScheme::Green;
    else if (butt == CSWhite)
        colorScheme = TerminalColorScheme::Paperwhite;
    if (colorScheme != TerminalColorScheme::NumSchemes)
    {
        persistedSettings.colorScheme = colorScheme;
        ApplySettings(termCanvas);
    }
}

void ReTro_Pascal::DisplaySettings::OnSetTerminalSize(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    auto butt = dynamic_cast<RadioButton^> (sender);
    int fontSize = 0;
    if (butt == TSBig)
        fontSize = FS_BIG;
    else if (butt == TSMed)
        fontSize = FS_MEDIUM;
    else if (butt == TSTiny)
        fontSize = FS_TINY;
    else if (butt == TSClassic)
        fontSize = FS_CLASSIC;
    if (fontSize != 0)
    {
        persistedSettings.fontSize = fontSize;
        ApplySettings(termCanvas);
    }
}

