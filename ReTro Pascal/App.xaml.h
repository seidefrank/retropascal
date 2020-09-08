//
// App.xaml.h
// Declaration of the App class.
//

#pragma once

#include "App.g.h"

namespace ReTro_Pascal
{
    /// <summary>
    /// Provides application-specific behavior to supplement the default Application class.
    /// </summary>
    ref class App sealed
    {
        Windows::ApplicationModel::Activation::ApplicationExecutionState OnLaunched_PreviousExecutionState;
    public:
        App();
        virtual void OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ args) override;
        Windows::ApplicationModel::Activation::ApplicationExecutionState FetchPreviousExecutionState()
        {
            auto prevState = OnLaunched_PreviousExecutionState;
            OnLaunched_PreviousExecutionState = Windows::ApplicationModel::Activation::ApplicationExecutionState::Running;
            return prevState;
        }

    private:
        void OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ e);
    };
}
