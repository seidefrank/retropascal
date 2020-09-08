//
// DisplaySettings.xaml.h
// Declaration of the DisplaySettings class
//

#pragma once

#include "DisplaySettings.g.h"
#include "winrt/term_winrt.h"

namespace ReTro_Pascal
{
    [Windows::Foundation::Metadata::WebHostHidden]
    public ref class DisplaySettings sealed
    {
        ReTro_Pascal::TermCanvas^ termCanvas;
    public:
        DisplaySettings(ReTro_Pascal::TermCanvas^);
        static void LoadPersistedSettings(ReTro_Pascal::TermCanvas^);
        static void ApplySettings(ReTro_Pascal::TermCanvas^);
    private:
        void SettingsBackClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void ResolutionPopup_24x80(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void ResolutionPopup_Big(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void ResolutionPopup_Medium(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void ResolutionPopup_Small(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void colorPopup_Amber(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void colorPopup_Green(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void colorPopup_Paperwhite(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void BoldOn_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void BoldOff_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void RevMenuOn_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void RevMenuOff_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void HandleCheck(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnSetColorScheme(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnSetTerminalSize(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void BoldfaceToggled(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void RevMenuToggled(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void EnableRevMenuToggled(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void PascalSyntaxHighlightingToggled(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
    };
}
