// audio device
// (C) 2013 Frank Seide

#pragma once

#include <collection.h>

#include "pperipherals.h"
#include "await.h"
#include <vector>
#include <mmdeviceapi.h>    // for ActivateAudioInterfaceAsync()
#include <AudioClient.h>

#pragma comment (lib, "Mmdevapi.lib")


using namespace std;
using namespace Platform;
using namespace Microsoft::WRL; // ComPtr
using namespace Windows::Foundation;
using namespace Windows::Media::Devices;

namespace psystem
{

// an efficient way to write COM code
// usage examples:
//  COM_function() || throw_hr ("message");
//  while ((s->Read (p, n, &m) || throw_hr ("Read failure")) == S_OK) { ... }
// is that cool or what?
struct bad_hr : public std::exception
{
    HRESULT hr;
    bad_hr (HRESULT p_hr, const char * msg) : hr (p_hr), std::exception (msg) { }
    // (only for use in || expression  --deprecated:)
    bad_hr() : std::exception (NULL) { }
    bad_hr (const char * msg) : std::exception (msg) { }
};
struct throw_hr
{
    const char * msg;
    inline throw_hr (const char * msg = NULL) : msg (msg) {}
};
inline static HRESULT operator|| (HRESULT hr, const throw_hr & e)
{
    if (SUCCEEDED (hr)) return hr;
    throw bad_hr (hr, e.msg);
}

class audioperipheral : public pperipheral
{
    virtual ~audioperipheral() { }

    Windows::UI::Core::CoreDispatcher^ uithread;
    Platform::String^ devicestring; // default audio device

    // block may be -1 (65535)
    virtual IORSLTWD read(void * vp, int boffset, size_t size, int block, size_t mode)
    {
        return IBADUNIT;    // audio input not implemented
    }

    // we only take single-shot audio of mono short INT buffers, which is sent asynchronously, no way to wait
    // Future: We could use 'block' to denote streams in which submissions are serialized.
#define ASIMPLNOTE 2208
    virtual IORSLTWD write(const void * vp, int boffset, size_t size, int block, size_t mode)
    {
        if (boffset != 0)
            return IBADMODE;
        if (mode == ASIMPLNOTE || size != 4)
        {
            const INT * ip = (const INT *) vp;
            size_t idur = abs (ip[0]);  // may be negative
            int pitch = ip[1];
            // convert values to float/hysical representation
            const float dur = idur / 1000.0f;   // duration is given in ms
            // Pitches are MIDI values; 69 is A4 = 440 Hz.
            const float freq = 440.0f * pow(2.0f, (pitch - 69) / 12.0f);
            // TODO: find real Apple values
            //dispatchproc(uithread, [&]        // seems not necessary, and is more sync'ed
            //{
                ComPtr<playonenote> onenote = new playonenote(freq, dur);
                ComPtr<IActivateAudioInterfaceAsyncOperation> asyncOp;
                ActivateAudioInterfaceAsync(devicestring->Data(), __uuidof(IAudioClient2), nullptr, onenote.Get(), &asyncOp) || throw_hr ("ActivateAudioInterfaceAsync");
            //});
        }
        // otherwise do nothing for now... :(
        //const short * p = (const short*) vp;
        //size_t nsamples = size / sizeof(*p);
        return IBADMODE;
    }

    virtual IORSLTWD stat(WORD & res)
    {
        return INOERROR;
    }

    virtual IORSLTWD clear()
    {
        return INOERROR;
    }
public:
    audioperipheral()
    {
        // this constructor runs on the UI thread
        uithread = Windows::UI::Core::CoreWindow::GetForCurrentThread()->Dispatcher;
        clear();
        // create the audio device
        devicestring = MediaDevice::GetDefaultAudioRenderId(Windows::Media::Devices::AudioDeviceRole::Default);
    }
    class playonenote : public IActivateAudioInterfaceCompletionHandler 
    {
        float freq;
        float dur;
        HANDLE event;
    public:
        playonenote (float freq, float dur) : refCount(1), freq(freq), dur(dur)
        {
            event = ::CreateEventEx(nullptr, nullptr, 0, SYNCHRONIZE | EVENT_MODIFY_STATE);
            if (!event)
                throw ref new Platform::OutOfMemoryException();
        }
        virtual ~playonenote()
        {
            CloseHandle(event);
        }
        // COM garbage
        long refCount;
        ULONG STDMETHODCALLTYPE AddRef() { return InterlockedIncrement (&refCount); }
        ULONG STDMETHODCALLTYPE Release() { long c = InterlockedDecrement (&refCount); if (!c)
            delete this;
        return c; }
        HRESULT STDMETHODCALLTYPE QueryInterface (REFIID  riid, void ** ppvObject)
        {
            if (!ppvObject) return E_INVALIDARG;
            static const GUID IID_IActivateAudioInterfaceCompletionHandler =        // .lib missing--WTF
                //  72A22D78-   CDE4-   431D-     B8    CC-   84    3A    71    19    9B    6D
                //{ 0x72A22D78, 0xCDE4, 0x431D, { 0xb8, 0xcc, 0x84, 0x3a, 0x71, 0x19, 0x9b, 0x6d } };
                // {94EA2B94-   E9CC-   49E0-     C0    FF-   EE    64    CA    8F    5B    90}
            { 0x94ea2b94, 0xe9cc, 0x49e0, { 0xc0, 0xff, 0xee, 0x64, 0xca, 0x8f, 0x5b, 0x90 } };
            if (riid == IID_IActivateAudioInterfaceCompletionHandler) *ppvObject = (IUnknown *)(IActivateAudioInterfaceCompletionHandler *) this;
            else { *ppvObject = NULL; return E_NOINTERFACE; }
            AddRef();
            return S_OK;
        }
        ComPtr<IAudioClient2> audioClient;
        HRESULT STDMETHODCALLTYPE ActivateCompleted(IActivateAudioInterfaceAsyncOperation *activateOperation)
        {
            const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);
            try
            {
                HRESULT hrActivateResult;
                ComPtr<IUnknown> punkAudioInterface;
                activateOperation->GetActivateResult(&hrActivateResult, &punkAudioInterface) || throw_hr("GetActivateResult");
                hrActivateResult || throw_hr("ActivateAudioInterfaceAsync (async)");
                // Get the pointer for the Audio Client
                punkAudioInterface.Get()->QueryInterface(IID_PPV_ARGS(&audioClient));
                if (!audioClient)
                    E_FAIL || throw_hr("QueryInterface (audio client)");

#if 0       // need DeviceProps, but sample program creates these after the fact
                // Opt into HW Offloading.  If the endpoint does not support offload it will return AUDCLNT_E_ENDPOINT_OFFLOAD_NOT_CAPABLE
                AudioClientProperties audioProps = {0};
                audioProps.cbSize = sizeof(AudioClientProperties);
                audioProps.bIsOffload = m_DeviceProps.IsHWOffload;
                audioProps.eCategory = ( m_DeviceProps.IsBackground ? AudioCategory_BackgroundCapableMedia : AudioCategory_ForegroundOnlyMedia );

                if (m_DeviceProps.IsRawChosen && m_DeviceProps.IsRawSupported)
                {
                    audioProps.Options = AUDCLNT_STREAMOPTIONS_RAW;
                }

                hr = m_AudioClient->SetClientProperties( &audioProps );
#endif

                // This sample opens the device is shared mode so we need to find the supported WAVEFORMATEX mix format
                WAVEFORMATEX * wfmt;
                audioClient->GetMixFormat(&wfmt) || throw_hr("GetMixFormat");
                const float samplerate = (float) wfmt->nSamplesPerSec;
                const size_t bytespersample = wfmt->nBlockAlign;
                const size_t bytesperchannel = (wfmt->wBitsPerSample+7)/8;
                if (bytesperchannel > 4 || bytesperchannel == 3)  // not supported format
                    E_INVALIDARG || throw_hr("weird audio format");
                const size_t numchannels = wfmt->nChannels;
                if (numchannels * bytesperchannel != bytespersample)
                    E_INVALIDARG || throw_hr("weird audio format");

                // Initialize the AudioClient in Shared Mode with the user specified buffer
#define REFTIMES_PER_SEC  10000000
                REFERENCE_TIME requestedDuration = REFTIMES_PER_SEC/10;   // 1 second --expressed in 100-nanosecond units
                audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
                                        requestedDuration, 0/*periodicity*/, wfmt, nullptr) || throw_hr("Initialize");
                // Note: AUDCLNT_STREAMFLAGS_RATEADJUST can be used to adjust the sample rate

                // register event
                audioClient->SetEventHandle(event);

                // get actual buffer size
                UINT32 bufferFrameCount;
                audioClient->GetBufferSize(&bufferFrameCount) || throw_hr("GetBufferSize");
                vector<float> data(bufferFrameCount);

                // get render client
                ComPtr<IAudioRenderClient> renderClient;
                audioClient->GetService(IID_IAudioRenderClient, (void**)&renderClient) || throw_hr("GetService");

                // send the data
                const size_t nsamples = (size_t) (samplerate * dur);
                const size_t nullsamples = 2 * bufferFrameCount;    // extra silence since we dont' know when to call Stop()
                for (size_t k = 0; k < nsamples + nullsamples; )
                {
                    // wait for event (except it's the first time)
                    if (k > 0)
                    {
                        auto r = ::WaitForSingleObjectEx(event, INFINITE, false);
                        if (r == 0xffffffff)
                            throw ref new Platform::InvalidArgumentException();
                    }

                    // determine how many samples are needed
                    size_t sendsamples = bufferFrameCount;
                    if (k > 0)  // maybe the system does not have that many samples buffer space
                    {
                        UINT32 numFramesPadding;;
                        audioClient->GetCurrentPadding(&numFramesPadding) || throw_hr("GetCurrentPadding");
                        sendsamples = bufferFrameCount - numFramesPadding;
                    }
                    data.resize(sendsamples);

                    // prepare buffer
                    const float twopi = 2.0f *  3.1415926535897f;
                    for (size_t i = 0; i < data.size(); i++)
                    {
                        if (i + k >= nsamples)
                        {
                            data[i] = 0;    // beyond end: pad with zeroes
                            continue;
                        }
                        const float t = (k + i) / samplerate;   // physical time
                        float val = sin(twopi * freq * t);      // sine wave
                        val *= 1.2f;                            // clip it a little
                        if (val > 1.0f) val = 1.0f;
                        else if (val < -1.0f) val = -1.0f;
                        float ampl = 1.0f;
                        const float bdec = 0.15f;               // first decay duration
                        const float bdrop = 0.5;                // drop to this after first decay
                        const float edec = min (0.15f, dur);    // end decay
                        ampl *= bdrop + (1-bdrop) * max (1.0f - t / bdec, 0.0f);
                        ampl *= min ((dur - t) / edec, 1.0f);
                        ampl *= 0.3f;                           // full volume is too loud!!
                        data[i] = val * ampl;
                    }

                    // send buffer
                    BYTE *pData;
                    renderClient->GetBuffer(sendsamples, &pData) || throw_hr("GetBuffer");

                    unsigned char * bdata = (unsigned char*) pData;
                    short * sdata = (short*) pData;
                    float * rdata = (float*) pData;
                    for (size_t i = 0; i < sendsamples; i++)
                    {
                        float val = data[i];    // -1 .. +1
                        // copy the same value to all channels
                        for (size_t c = 0; c < numchannels; c++)
                        {
                            switch (bytesperchannel)
                            {
                            case 1:
                                *bdata++ = (unsigned char) ((val + 1.0f) * 127.99);
                                break;
                            case 2:
                                *sdata++ = (short) (-32768 + (int) ((val + 1.0f) * 32767.99));
                                break;
                            case 4:
                                *rdata++ = val * 0.5f;
                                break;
                            }
                        }
                    }

                    renderClient->ReleaseBuffer(sendsamples, 0) || throw_hr("ReleaseBuffer");

                    // if first time then we still need to start
                    if (k == 0)
                        audioClient->Start() || throw_hr("Start");

                    // done
                    k += sendsamples;
                }
                if (nsamples > 0)
                {
                    ::WaitForSingleObjectEx(event, INFINITE, false);    // this does not seem to work
                    audioClient->Stop() || throw_hr("Stop");
                }

                // I do not know why I need this Release(), but with out it, this object will never call its destructor.
                // Maybe ComPtr takes an additional reference count upon assigning?
                Release();
                return S_OK;
            }
            catch (const bad_hr & e)
            {
                Release();
                return e.hr;
            }
        }
    };

};

pperipheral * createaudioperipheral() { return new audioperipheral(); }

};
