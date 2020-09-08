//
// MainPage.xaml.h
// Declaration of the MainPage class.
//

#pragma once

#include "MainPage.g.h"
#include "winrt/interpreter_thread.h"
#include "winrt/term_winrt.h"
#include <memory>

namespace ReTro_Pascal
{
    public delegate void VoidOfVoidDelegate(void);   // because I cannot figure out the lambda syntax
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public ref class MainPage sealed
    {
    private:
        void MessageBox(Platform::String^ message, Windows::UI::Popups::UICommand^ uiCommand, Windows::UI::Popups::UICommand^ uiCommand2 = nullptr);
        void Beep();
        void Terminate(Windows::UI::Popups::IUICommand ^);
        void Panic(Platform::String^ message);
        void Notify(Platform::String^ message);
        void OnHalted(Platform::String^ message);
        ReTro_Pascal::TermCanvas^ termCanvas;
        std::unique_ptr<ReTro_Pascal::Terminal> terminal;
        std::unique_ptr<ReTro_Pascal::InterpreterThread> interpreter;
        Platform::String^ CreateDisk(size_t u, const wchar_t * name) const;
        void CreatePMachine();
    public:
        MainPage();
        void Suspend(VoidOfVoidDelegate^ callback);
        void Resume(VoidOfVoidDelegate^ callback);
        void ResumeInterpreter(int waitms);

    protected:
        virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;
    private:
        void InitialResume(bool restoreFromDisk);
#if 0
        void ResolutionPopup_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void ResolutionPopup_24x80(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void ResolutionPopup_Big(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void ResolutionPopup_Medium(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void ResolutionPopup_Small(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void colorPopup_Amber(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void colorPopup_Green(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void colorPopup_Paperwhite(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void colorPopup_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void ResolutionPopup_24x40(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void ResolutionPopup_MaxWidth(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
#endif
        void Undo_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void Redo_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void SuspendTestButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void ResumeTestButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void SoftReset_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void BoldOn_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void BoldOff_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void RevMenuOn_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void RevMenuOff_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void onSettingsRequested(Windows::UI::ApplicationSettings::SettingsPane^ settingsPane, Windows::UI::ApplicationSettings::SettingsPaneCommandsRequestedEventArgs^ eventArgs);
        void NewSettingsFlyout(UserControl^ panel);
        void ShowDisplaySettingsFlyout(Windows::UI::Popups::IUICommand^ command);
        void ShowSettingsAboutFlyout(Windows::UI::Popups::IUICommand^ command);
    };
}
