// simple hack to turn WinRT async operations into synchronous ones
// Copyright (C) 2013 Frank Seide

#pragma once
#include <collection.h>

// synchronous execution of WinRT async operations
// auto opResult = await(SomeOpAsync(...));
// Note: This will NOT work from the UI thread, as it will deadlock.
template<typename T> inline static T^ await(Windows::Foundation::IAsyncOperation<T^>^ asyncOperation)
{
    Exception^ ex = nullptr;
    T^ result = nullptr;
    try
    {
        // create an event to wait for, to be flagged from the async callback
        auto h = ::CreateEventEx(nullptr, nullptr, 0, SYNCHRONIZE | EVENT_MODIFY_STATE);
        if (h == nullptr)
            throw ref new Platform::OutOfMemoryException();
        // install a completion handler to flag the event
        asyncOperation->Completed = ref new AsyncOperationCompletedHandler<T^>
            ([h,&ex,&result](IAsyncOperation<T^>^ operation, AsyncStatus status)
        {
            try
            {
                result = operation->GetResults();   // will throw in case of error
            }
            catch(Exception^ e)
            {
                ex = e; // pass exception across thread boundary
            }
            ::SetEvent(h);
        });
        // wait for the async op to complete
        auto r = ::WaitForSingleObjectEx(h, INFINITE, false);
        if (r == 0xffffffff)
            throw ref new Platform::InvalidArgumentException();
        ::CloseHandle(h);
    }
    catch(Exception^ e)
    {
        ex = e;
    }
    if (ex)
        throw ex;
    else
        return result;
}

// execute on UI thread
template<typename LAMBDA> inline static void dispatchproc(Windows::UI::Core::CoreDispatcher^ dispatcher, LAMBDA & lambda)
{
    Exception^ ex = nullptr;
    // create an event to wait for, to be flagged from the async callback
    auto h = ::CreateEventEx(nullptr, nullptr, 0, SYNCHRONIZE | EVENT_MODIFY_STATE);
    if (h == nullptr)
        throw ref new Platform::OutOfMemoryException();
    dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([&]() -> void
    {
        try
        {
            lambda();
        }
        catch(Exception^ e)
        {
            ex = e; // pass exception across thread boundary
        }
        ::SetEvent(h);
    }));
    auto r = ::WaitForSingleObjectEx(h, INFINITE, false);
    if (r == 0xffffffff)
        throw ref new Platform::InvalidArgumentException();
    ::CloseHandle(h);
}

#if 0       // not working due to template parameter stuff
// execute on UI thread
template<typename T, typename LAMBDA> inline static T dispatchfunc(Windows::UI::Core::CoreDispatcher^ dispatcher, LAMBDA & lambda)
{
    Exception^ ex = nullptr;
    T result;
    // create an event to wait for, to be flagged from the async callback
    auto h = ::CreateEventEx(nullptr, nullptr, 0, SYNCHRONIZE | EVENT_MODIFY_STATE);
    if (h == nullptr)
        throw ref new Platform::OutOfMemoryException();
    dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([&]() -> void
    {
        try
        {
            result = lambda();
        }
        catch(Exception^ e)
        {
            ex = e; // pass exception across thread boundary
        }
        ::SetEvent(h);
    }));
    auto r = ::WaitForSingleObjectEx(h, INFINITE, false);
    if (r == 0xffffffff)
        throw ref new Platform::InvalidArgumentException();
    ::CloseHandle(h);
    return result;
}

// execute on UI thread
template<typename T, typename LAMBDA> inline static T awaitdispatchfunc(Windows::UI::Core::CoreDispatcher^ dispatcher, LAMBDA & lambda)
{
    auto a = dispatchfunc<IAsyncOperation<T>,LAMBDA>(dispatcher, lambda);
    return await(a);
}
#endif
