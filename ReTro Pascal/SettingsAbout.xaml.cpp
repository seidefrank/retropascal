//
// SettingsAbout.xaml.cpp
// Implementation of the SettingsAbout class
//

#include "SettingsAbout.xaml.h"

using namespace ReTro_Pascal;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
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

// The User Control item template is documented at http://go.microsoft.com/fwlink/?LinkId=234236

SettingsAbout::SettingsAbout()
{
    InitializeComponent();
    //auto brush = ref new ImageBrush();
    //brush->ImageSource = ref new BitmapImage(ref new Uri("ms-appx:///Assets/Manual Background.png"));
    //brush->Stretch = Stretch::UniformToFill;
    //HeaderBackground->Background = brush;
    SupportLink->Tapped += ref new TappedEventHandler([this] (Object^ sender, Windows::UI::Xaml::Input::TappedRoutedEventArgs^ e) -> void
    {
        auto tb = dynamic_cast<TextBlock^> (sender);
        auto url = "mailto:" + tb->Text;
        auto uri = ref new Windows::Foundation::Uri(url);
        Windows::System::Launcher::LaunchUriAsync(uri);
    });
}

void ReTro_Pascal::SettingsAbout::SettingsBackClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    auto popup = dynamic_cast<Popup^>(Parent);
    if (popup)
        popup->IsOpen = false;
    // this crashes
    //SettingsPane::Show();   // this seems to just show the Settings charm page
}
