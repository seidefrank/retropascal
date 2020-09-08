//
// SettingsAbout.xaml.h
// Declaration of the SettingsAbout class
//

#pragma once

#include "SettingsAbout.g.h"

namespace ReTro_Pascal
{
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class SettingsAbout sealed
	{
	public:
		SettingsAbout();
        private:
            void SettingsBackClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        };
}
