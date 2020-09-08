// the turtlegraphnics device in the UCSD system
// (C) 2013 Frank Seide

// For Michael:
//  - no namespaces, missing headers, syntax error in sample on http://msdn.microsoft.com/library/windows/apps/hh825871.aspx
//  - http://msdn.microsoft.com/library/windows/apps/Hh706320 what is the lib to link? Needed to check project file of sample
//  - http://msdn.microsoft.com/en-us/library/windows/apps/xaml/jj150606 cannot be trivially translated to C++
//  - http://msdn.microsoft.com/en-us/library/windows/apps/xaml/Hh871366%28v=win.10%29.aspx: namespace of Clipboard?

#include <collection.h>
#include "pgraphics.h"
#include "pturtlegraphics.h"
#include "ptypes.h"     // for Pascal string and PTR<>
#include "pmachine.h"   // for loading SYSTEM.CHARSET

#include <wrl.h> 
#include <wrl\client.h> 
 
#include <dxgi.h> 
#include <dxgi1_2.h> 
#include <d2d1_1.h> 
#include <d3d11_1.h> 
#include <d2d1effects.h> 
#include "windows.ui.xaml.media.dxinterop.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d2d1.lib")

#include <memory>
#include <deque>
#include <stdexcept>

using namespace Platform; 
using namespace Microsoft::WRL;
using namespace Windows::Foundation;        // for Rect
using namespace Windows::UI;
using namespace Windows::UI::Xaml::Media::Imaging;
using namespace std;

// interface to term_winrt.cpp
// We are called regularly
void SetGraphicsTickCallback(SurfaceImageSource^ (*tickcallbackstatic)(void *, size_t w, size_t h), void * obj);
int term_PADDLE(size_t k);
bool term_BUTTON(size_t k);

extern const char * SYSTEM_CHARSET_DATA[2048];

namespace psystem
{

static std::exception failwithhr;   // singleton
static HRESULT operator||(HRESULT hr, const std::exception &)
{
    if (FAILED(hr))
        throw std::runtime_error("FAILED(hr)");
    return hr;
}

class graphicsperipheral : public pperipheral, CRITICAL_SECTION, public TURTLEGRAPHICS
{
    virtual ~graphicsperipheral() { DeleteCriticalSection(this); }

    // We communicate from our UNIT by writing the parameters out through UNITWRITE
    // with mode = 0x100 + proc number (same no as Apple), and size = #parameter bytes (for sanity checking).
    // Function return values are returned through read().
    INT lastTGResult;                       // result we communicate back
    INT lastTGProcNo;
    enum specialmodes
    {
        TGIMPLMODE = 2000,                  // actual codes are TGIMPLMODE + Apple proc no
        ASIMPLMODE = 2200,                  // actual codes are ASIMPLMODE + Apple proc no
        ASIMPLPADDLE = ASIMPLMODE + 2,
        ASIMPLBUTTON = ASIMPLMODE + 3,
    };
    virtual IORSLTWD write(const void * p, int offset, size_t size, int block, size_t mode)
    {
        if (mode == 0)
            return INOERROR;    // need to return IONOERROR because COMPILER writes to this device, seemingly for debugging
        lastTGResult = lastTGProcNo = -1;
#if 1   // TODO: compat with our own earlier attempts; remove
        if (mode == 256) // (procno was encoded in block)
            mode = TGIMPLMODE + block;
#endif
        const INT * args = (const INT *) (offset + (const char*)p);
        switch (mode)
        {
        case TGIMPLMODE + pnTURTLEGRAPHICSLOADED:    // note: we currently cannot generate this call
            if (size != 0)
                return IBADMODE;
            LOADED();
            return INOERROR;
        case TGIMPLMODE + pnINITTURTLE:
            if (size != 0)
                return IBADMODE;
            INITTURTLE();
            return INOERROR;
        case TGIMPLMODE + pnTURN:        // (RELANGLE: INTEGER);
            if (size != 2)
                return IBADMODE;
            TURN(args[0]);
            return INOERROR;
        case TGIMPLMODE + pnTURNTO:      // (ANGLE: INTEGER);
            if (size != 2)
                return IBADMODE;
            TURNTO(args[0]);
            return INOERROR;
        case TGIMPLMODE + pnMOVE:        // (RELDISTANCE: INTEGER);
            if (size != 2)
                return IBADMODE;
            MOVE(args[0]);
            return INOERROR;
        case TGIMPLMODE + pnMOVETO:      // (X, Y: INTEGER);
            if (size != 4)
                return IBADMODE;
            MOVETO(args[1],args[0]);
            return INOERROR;
        case TGIMPLMODE + pnPENCOLOR:    // (PCOLOR: TGCOLOR);
            if (size != 2)
                return IBADMODE;
            PENCOLOR((TGCOLOR)args[0]);
            return INOERROR;
        case TGIMPLMODE + pnTEXTMODE:
            if (size != 0)
                return IBADMODE;
            TEXTMODE();
            return INOERROR;
        case TGIMPLMODE + pnGRAFMODE:
            if (size != 0)
                return IBADMODE;
            GRAFMODE();
            return INOERROR;
        case TGIMPLMODE + pnFILLSCREEN:
            if (size != 2)
                return IBADMODE;
            FILLSCREEN((TGCOLOR)args[0]);
            return INOERROR;
        case TGIMPLMODE + pnVIEWPORT:
            if (size != 8)
                return IBADMODE;
            VIEWPORT(args[3], args[2], args[1], args[0]);
            return INOERROR;
        case TGIMPLMODE + pnTURTLEX:    // : INTEGER;
            if (size != 0)
                return IBADMODE;
            lastTGResult = TURTLEX();
            lastTGProcNo = mode;
            return INOERROR;
        case TGIMPLMODE + pnTURTLEY:    // : INTEGER;
            if (size != 0)
                return IBADMODE;
            lastTGResult = TURTLEY();
            lastTGProcNo = mode;
            return INOERROR;
        case TGIMPLMODE + pnTURTLEANG:  // : INTEGER;
            if (size != 0)
                return IBADMODE;
            lastTGResult = TURTLEANG();
            lastTGProcNo = mode;
            return INOERROR;
        case TGIMPLMODE + pnDRAWBLOCK:  // (VAR SOURCE:INTEGER{TODO: should be SOURCE:^INTEGER}; ROWSIZE, XSKIP, YSKIP, WIDTH, HEIGHT, XSCREEN, YSCREEN, MODE: INTEGER);
            if (size != 18)
                return IBADMODE;
            DRAWBLOCK(themachine.ptr(*(PTR<BYTE>*)&args[8]),
                args[7], args[6], args[5], args[4], args[3], args[2], args[1], args[0]);
            return INOERROR;
        case TGIMPLMODE + pnWCHAR:      // (CH: CHAR);
            if (size != 2)
                return IBADMODE;
            WCHAR(args[0]);
            return INOERROR;
        case TGIMPLMODE + pnWSTRING:    // (S: STRING);
            if (size != 2)
                return IBADMODE;
            WSTRING(*(const pstring*)args);
            return INOERROR;
        case TGIMPLMODE + pnCHARTYPE:   // (MODE: INTEGER);
            if (size != 2)
                return IBADMODE;
            CHARTYPE(args[0]);
            return INOERROR;
        case TGIMPLMODE + pnSYNCFRAME:  // :INTEGER
            if (size != 0)
                return IBADMODE;
            lastTGResult = SYNCFRAME();
            lastTGProcNo = mode;
            return INOERROR;
        }
        return IBADMODE;
    }
    virtual IORSLTWD stat(WORD & res) { return INOERROR; }
    virtual IORSLTWD read(void * p, int offset, size_t size, int block, size_t mode)
    {
        if (mode == 0)          // this device only supports special modes
            return IBADMODE;
        // TODO: something to get actual screen resolution
#if 1   // TODO: compat with our own earlier attempts; remove
        if (mode == 256) // (procno was encoded in block)
            mode = TGIMPLMODE + block;
#endif
        INT & result = *(INT *) (offset + (char*)p);
        // fetch return value of a previous UNITWRITE call
        if (size == 2 && mode == lastTGProcNo)
        {
            result = lastTGResult;
            return INOERROR;
        }
        switch (mode)
        {
        case ASIMPLPADDLE:  // pass arg as block
            if (size != 2)
                return IBADMODE;
            result = term_PADDLE(block % 4);
            themachine.beingpolled();
            return INOERROR;
        case ASIMPLBUTTON:  // pass arg as block
            if (size != 2)
                return IBADMODE;
            result = term_BUTTON(block % 4);
            themachine.beingpolled();
            return INOERROR;
        }
        return IBADMODE;
    }
    // reset all multi-media, i.e. TEXTMODE; also stop ongoing sound output
    // This is called every time we request a command character in the Pascal shell,
    // including the fail-retry loop; i.e. it is called a lot. Keep it cheap.
    virtual IORSLTWD clear()
    {
        lock_guard guard(this);
        graphicsVisible = false;        // enforce TEXTMODE
        opqueue.clear();                // and flush all pending drawing operations to avoid being switched back to GRAFMODE
        nativeResolution = false;       // gets set to TRUE if user calls VIEWPORT with something larger than Apple dimensions
        renderingOnHold = false;
        dirty = false;
        CHARSET.clear();
        lastTGResult = lastTGProcNo = -1;
        return INOERROR;
    }

    // -----------------------------------------------------------------------
    // execution
    // -----------------------------------------------------------------------

    size_t screenWidth, screenHeight;       // last communicated physical screen size (note: read by INITTURTLE; so lock_guard this)

    // Direct2D objects 
    ComPtr<IDXGIDevice>                 dxgiDevice; 
    ComPtr<ID3D11Device>                m_d3dDevice; 
    ComPtr<ID2D1Device>                 m_d2dDevice; 
    ComPtr<ID2D1DeviceContext>          m_d2dContext; 
    size_t bitmapWidth, bitmapHeight;           // size of drawingBitmap as (to be) allocated (only used by rendering thread)
    ComPtr<ID2D1Bitmap1> drawingBitmap;         // we draw here

    typedef size_t BLITMODE;                    // MODE parameter for DRAWBLOCK and WCHAR/WSTRING (0..15)

    struct PENSTATE
    {
        char MODE;   // 1 draw, 0 don' draw, -1 draw reverse
        unsigned char R, G, B, A;
        // ^^ inefficient, we could encode A in MODE to make the struct 4 bytes
        // if we set A to non-1, we can indeed get transparency OVER our text display--that might allow some nice effects!
        D2D1_COLOR_F rgba() const { return D2D1::ColorF(R / 255.0f, G / 255.0f, B / 255.0f, A / 255.0f); }
        unsigned int rgba32() const { return (A<<24) + (R<<16) + (G<<8) + B; }
        bool operator==(const PENSTATE & other) const { return MODE == other.MODE && R == other.R && G == other.G && B == other.B && A == other.A; }
        static PENSTATE rgb(int r, int g, int b) { PENSTATE pen = { 1/*MODE*/, r, g, b, 255 }; return pen; }
    };

    bool dirty;                                 // something was drawn into the bitmap
    bool nativeResolution;                      // programmer told us to use native resolution rather than Apple's
    volatile bool renderingOnHold;              // if true then render function will not update
    RECT renderViewport;                        // current viewport, cached
    PENSTATE renderPen;                         // current pen
    ComPtr<ID2D1SolidColorBrush> renderBrush;   // current Pen's brush, cached
    bool graphicsVisible;                       // GRAFMODE was called (note: we clear this flag from interpreter thread in clear())

    // Initialize hardware-dependent resources.  --from a sample file
    bool LazyCreateDeviceResources(bool force)
    {
        if (m_d3dDevice && !force)                       // only the first time
            return false;

        // This flag adds support for surfaces with a different color channel ordering 
        // than the API default. It is required for compatibility with Direct2D. 
        UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;  

#if defined(_DEBUG)     
        // If the project is in a debug build, enable debugging via SDK Layers. 
        creationFlags |= D3D11_CREATE_DEVICE_DEBUG; 
#endif 

        // This array defines the set of DirectX hardware feature levels this app will support. 
        // Note the ordering should be preserved. 
        // Don't forget to declare your application's minimum required feature level in its 
        // description.  All applications are assumed to support 9.1 unless otherwise stated. 
        const D3D_FEATURE_LEVEL featureLevels[] = {  D3D_FEATURE_LEVEL_11_1,  D3D_FEATURE_LEVEL_11_0,  D3D_FEATURE_LEVEL_10_1,  D3D_FEATURE_LEVEL_10_0,  D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,  D3D_FEATURE_LEVEL_9_1 };

        // Create the Direct3D 11 API device object. 
        D3D11CreateDevice( 
            nullptr,                        // Specify nullptr to use the default adapter. 
            D3D_DRIVER_TYPE_HARDWARE, nullptr, 
            creationFlags,                  // Set debug and Direct2D compatibility flags. 
            featureLevels, ARRAYSIZE(featureLevels),    // List of feature levels this app can support. 
            D3D11_SDK_VERSION,              // Always set this to D3D11_SDK_VERSION for Metro style apps. 
            &m_d3dDevice,                   // Returns the Direct3D device created. 
            nullptr, nullptr) || failwithhr;

        // Get the Direct3D 11.1 API device. 
        m_d3dDevice.As(&dxgiDevice) || failwithhr;

        // Create the Direct2D device object and a corresponding context. 
        D2D1CreateDevice(dxgiDevice.Get(), nullptr, &m_d2dDevice) || failwithhr;

        m_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_d2dContext) || failwithhr;

        // Set DPI to the display's current DPI. 
        const float dpi = Windows::Graphics::Display::DisplayProperties::LogicalDpi;
        m_d2dContext->SetDpi(dpi, dpi);

        renderBrush = nullptr;  // flush cached brush (which is associated with the d2d context)

        return true;
    }

    void CreateDeviceResources() { LazyCreateDeviceResources(true); }

    // Begins drawing, allowing updates to content in the specified area. 
    // This bitmap is what we draw in, and is kept across screen resizes... or something. TODO: just draw into main bitmap instead! Or not, for double buffering?
    void BeginDraw()
    {     
        if (!m_d2dContext)
            throw std::runtime_error("we began to draw before the first screen tick");

#if 0   // keeping the bitmap
        // http://msdn.microsoft.com/en-us/library/windows/desktop/dd372260%28v=vs.85%29.aspx

        // Preserve the pre-existing target.
        ComPtr<ID2D1Image> oldTarget;
        m_d2dContext->GetTarget(&oldTarget);

        // Render static content to the sceneBitmap.
        m_d2dContext->SetTarget(sceneBitmap.Get());
        m_d2dContext->BeginDraw();
        // ...
        m_d2dContext->EndDraw();

        // Render sceneBitmap to oldTarget.
        m_d2dContext->SetTarget(oldTarget.Get());
        m_d2dContext->DrawBitmap(sceneBitmap.Get());
#endif

        // direct drawing into our own bitmap for now
        // If bitmap is non-NULL, this is being called from the update function.
        m_d2dContext->SetTarget(drawingBitmap.Get());
        // Begin drawing using D2D context. 
        m_d2dContext->BeginDraw();
    }

    // Ends drawing updates started by a previous BeginDraw call. 
    void EndDraw()
    { 
        // Remove the render target and end drawing. 
        m_d2dContext->EndDraw() || failwithhr;
        m_d2dContext->SetTarget(nullptr);
    }

    class lock_guard
    {
        graphicsperipheral * us;
    public:
        lock_guard(graphicsperipheral * ths) : us(ths) { EnterCriticalSection(us); }
        ~lock_guard() { LeaveCriticalSection(us); }
    };

    SurfaceImageSource^ bitmapImageSource;
    ComPtr<ISurfaceImageSourceNative> bitmapImageSourceAsNative;

    // create the image brush
    // This is called from RenderInit(), which implements INITTURTLE.
    // TODO: we do not release graphics resources in clear() since that's the wrong thread
    void CreateImageSource(size_t w, size_t h)
    {
        bitmapImageSource = ref new SurfaceImageSource((int) w, (int) h, false);

        // full sample: http://code.msdn.microsoft.com/windowsapps/XAML-SurfaceImageSource-58f7e4d5/sourcecode?fileId=64193&pathId=1915311265

        // get the native interface
        IInspectable* sisInspectable = (IInspectable*) reinterpret_cast<IInspectable*>(bitmapImageSource);
        sisInspectable->QueryInterface(__uuidof(ISurfaceImageSourceNative), (void **)&bitmapImageSourceAsNative) || failwithhr;

        LazyCreateDeviceResources(false);

        AssociateImageSource();

        // create the drawing bit map
        CreateDrawingBitmap(w, h);
    }

    void CreateDrawingBitmap(size_t w, size_t h)
    {
        bitmapWidth = w;
        bitmapHeight = h;

        // Create a bitmap.
        // We will keep drawing into this; and bitblit it over as needed.
        // BUGBUG: if BeginDraw() fails, we will recreate this--and loose its content. Could copy it out?
        D2D1_SIZE_U size = { bitmapWidth, bitmapHeight };
        const float dpi = Windows::Graphics::Display::DisplayProperties::LogicalDpi;
        D2D1_BITMAP_PROPERTIES1 bitmapProperties = { D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi, dpi, D2D1_BITMAP_OPTIONS_TARGET, nullptr };
        //  | D2D1_BITMAP_OPTIONS_CPU_READ crashes the thing
        m_d2dContext->CreateBitmap(size, nullptr, 0, &bitmapProperties, &drawingBitmap) || failwithhr;
    }

    void AssociateImageSource()
    {
        // Associate the DXGI device with the SurfaceImageSource. 
        bitmapImageSourceAsNative->SetDevice(dxgiDevice.Get()) || failwithhr;
    }

    // actual drawing to the image source happens by copying the bitmap over
    // This is necessary because the image source bitmap is not retained across calls. WORKAROUND
    void FinalizeDrawing()
    {
        if (!drawingBitmap)
            return;
        D2D_RECT_F drawingRect = { 0, 0, (float) bitmapWidth, (float) bitmapHeight };
        D2D_RECT_F screenRect = drawingRect;
        D2D_RECT_F drawingRectOnScreen = screenRect;
#if 0
        D2D_RECT_F screenRect = { 0, 0, (float) screenWidth, (float) screenHeight };

        // we adjust screen area to show the drawingRect with correct aspect ratio
        //  - if we need to make it narrower, we will do so symmetrically
        //  - if we need to make it less tall, we will glue it to the top (thinking of the on-screen keyboard)
        if (bitmapHeight == 0 || screenHeight == 0)
           throw std::logic_error("FinalizeDrawing: zero height of drawingRect and/or screen");
        D2D_RECT_F drawingRectOnScreen = screenRect;
        float areaaspectratio = drawingRect.right / drawingRect.bottom;
        float screenaspectratio = drawingRectOnScreen.right / drawingRectOnScreen.bottom;
        D2D_RECT_F letterBoxRect1 = drawingRectOnScreen;
        D2D_RECT_F letterBoxRect2 = drawingRectOnScreen;
        if (areaaspectratio < screenaspectratio)    // need to make it more narrow
        {
            drawingRectOnScreen.right = screenHeight * areaaspectratio;
            float offset = (screenWidth - drawingRectOnScreen.right) / 2.0f;   // center horizontally
            drawingRectOnScreen.left += offset;
            drawingRectOnScreen.right += offset;
            letterBoxRect1.right = drawingRectOnScreen.left + 1;   // (add some margin since we may fall onto a fractional pixel)
            letterBoxRect2.left = drawingRectOnScreen.right - 1;
        }
        else                                        // need to make it less tall
        {
            drawingRectOnScreen.top = drawingRectOnScreen.bottom - screenWidth / areaaspectratio;
            letterBoxRect1.top = drawingRectOnScreen.bottom - 1;
            letterBoxRect2.bottom = 0;  // disable
        }
#endif

        // Begin drawing - returns a target surface and an offset to use as the top left origin when drawing. 
        ComPtr<IDXGISurface> surface;
        POINT offset;
        RECT updateRect = { 0, 0, (LONG) ceil(screenRect.right), (LONG) ceil(screenRect.bottom) };
        HRESULT beginDrawHR = bitmapImageSourceAsNative->BeginDraw(updateRect, &surface, &offset);
        while (beginDrawHR == DXGI_ERROR_DEVICE_REMOVED || beginDrawHR == DXGI_ERROR_DEVICE_RESET) 
        { 
            // If the device has been removed or reset, attempt to recreate it and continue drawing.
            // BUBGUG: This will loose its content. When does it happen??
            CreateDeviceResources();
            // re-associate the new DXGI device with the SurfaceImageSource. 
            AssociateImageSource();
            // and recreate the bitmap
            // BUGBUG: We will lose our content. Seems unavoidable. How often does this happen anyway
            CreateDrawingBitmap(bitmapWidth, bitmapHeight);
            beginDrawHR = bitmapImageSourceAsNative->BeginDraw(updateRect, &surface, &offset);
        } 
        // Notify the caller by throwing an exception if any other error was encountered. 
        beginDrawHR || failwithhr;

        // Create render target. 
        // Set context's render target. 
        ComPtr<ID2D1Bitmap1> bitmap; 
        m_d2dContext->CreateBitmapFromDxgiSurface(surface.Get(), nullptr, &bitmap) || failwithhr;

        m_d2dContext->BeginDraw();
        m_d2dContext->SetTarget(bitmap.Get());

        // Render updated drawingBitmap to the actual surface.

        // Apply a clip and transform to constrain updates to the target update area. 
        // This is required to ensure coordinates within the target surface remain 
        // consistent by taking into account the offset returned by BeginDraw, and 
        // can also improve performance by optimizing the area that is drawn by D2D. 
        // Apps should always account for the offset output parameter returned by  
        // BeginDraw, since it may not match the passed updateRect input parameter's location.
        // TODO: can we draw directly into the same bitmap?

        //m_d2dContext->PushAxisAlignedClip( 
        //    D2D1::RectF((float) offset.x, (float) offset.y,
        //    (float) offset.x + viewport.right - viewport.left,
        //    (float) offset.y + viewport.bottom - viewport.top),
        //    D2D1_ANTIALIAS_MODE_ALIASED);

        //m_d2dContext->SetTransform(D2D1::Matrix3x2F::Translation((float) offset.x, (float) offset.y));

        // copy the drawing bitmap
        D2D1::Matrix3x2F flipAndOffset;       // Apple Pascal has origin in lower left corner
        flipAndOffset._11 = 1.0; flipAndOffset._12 = 0.0;
        flipAndOffset._21 = 0.0; flipAndOffset._22 = -1.0;
        flipAndOffset._31 = (float) offset.x; flipAndOffset._32 = offset.y + screenRect.bottom;

        m_d2dContext->SetTransform(flipAndOffset);
#if 0
        // clear the area outside
        m_d2dContext->FillRectangle(letterBoxRect1, CachedBrush(PENSTATE::rgb(0,0,0)));
        if (letterBoxRect2.bottom != 0)
            m_d2dContext->FillRectangle(letterBoxRect2, CachedBrush(PENSTATE::rgb(0,0,0)));
#endif

        // copy bitmap itself
        m_d2dContext->DrawBitmap(drawingBitmap.Get(), &drawingRectOnScreen, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &drawingRect);

        // Remove the transform and clip applied in BeginDraw since 
        // the target area can change on every update. 
        m_d2dContext->SetTransform(D2D1::IdentityMatrix());
        //m_d2dContext->PopAxisAlignedClip();

        // Remove the render target and end drawing. 
        m_d2dContext->SetTarget(nullptr);
        m_d2dContext->EndDraw() || failwithhr;

        bitmapImageSourceAsNative->EndDraw() || failwithhr;
    } 

    D2D1_COLOR_F rgb(int R, int G, int B) { return D2D1::ColorF(R / 255.0f, G / 255.0f, B / 255.0f, 1.0f); }

    // Clears the background with the given color. 
    //void Clear(D2D1_COLOR_F color) { lock_guard guard(this); m_d2dContext->Clear(color); dirty = true; }

    // how to read a pixel:
#if 0
    // CPU Access buffer
D3D11_TEXTURE2D_DESC StagedDesc = {
	1,//UINT Width;
	1,//UINT Height;
	1,//UINT MipLevels;
	1,//UINT ArraySize;
	DXGI_FORMAT_R32G32B32A32_FLOAT,//DXGI_FORMAT Format;
	1, 0,//DXGI_SAMPLE_DESC SampleDesc;
	D3D11_USAGE_STAGING,//D3D11_USAGE Usage;
	0,//UINT BindFlags;
	D3D11_CPU_ACCESS_READ,//UINT CPUAccessFlags;
	0//UINT MiscFlags;
};
m_pd3dDevice->CreateTexture2D( &StagedDesc, NULL, &Surface->CPUAccessBuffer );
		// Select the pixel
		D3D11_BOX SrcBox;
		SrcBox.left = X; // Minimum X
		SrcBox.right = X+1; // Maximum X
		SrcBox.top = Y; // Minimum Y
		SrcBox.bottom = Y+1; // Maximum Y
		SrcBox.front = 0; // Always 0 for 2D textures
		SrcBox.back = 1; // Always 1 for 2D textures
		
		// Get the texture pointer
		if (Surface->HaveExtraColorBuffer == false || Surface->SwapState == true) {
			InputResource = Surface->OriginalColorBuffer;
		} else {
			InputResource = Surface->ExtraColorBuffer;
		}
		
		// Copy the pixel to the staging buffer
		m_pImmediateContext->CopySubresourceRegion(Surface->CPUAccessBuffer,0,0,0,0,InputResource,0,&SrcBox);
		
		// Lock the memory
		D3D11_MAPPED_SUBRESOURCE MappingDesc;
		m_pImmediateContext->Map(Surface->CPUAccessBuffer,0,D3D11_MAP_READ,0,&MappingDesc);
			
			// Get the data
			if (MappingDesc.pData == NULL) {
				::MessageBox(NULL, L"DrawSurface_GetPixelColor: Could not read the pixel color because the mapped subresource returned NULL.", L"Error!", NULL);
			} else {
				Result = *((D3DXVECTOR4*)MappingDesc.pData);
			}
			
		// Unlock the memory
		m_pImmediateContext->Unmap(Surface->CPUAccessBuffer,0);
#endif

    // call this before any drawing; corresponds to INITTURTLE
    void RenderInit(size_t w, size_t h)
    {
        if (drawingBitmap && w == bitmapWidth && h == bitmapHeight)
            return;

        // first time or bitmap size has changed
        CreateImageSource(w, h);
    }

    // drawing state
    // create a brush from a PENSTATE, cached
    ID2D1SolidColorBrush * CachedBrush(PENSTATE pen)
    {
        if (renderBrush || !(pen == renderPen))
        {
            m_d2dContext->CreateSolidColorBrush(pen.rgba(), &renderBrush) || failwithhr;
            renderPen = pen;
        }
        return renderBrush.Get();
    }

    // get the rect for the ViewPort
    D2D_RECT_F RenderViewportRect(const RECT & viewport) const
    {
        // we flip top and bottom since origin is at the bottom
        D2D_RECT_F r = { (float) viewport.left, (float) viewport.bottom, (float) viewport.right, (float) viewport.top };
        return r;
    }

    void RenderFillScreen(PENSTATE fillcolor, size_t x, size_t y, size_t w, size_t h, const RECT & viewport)
    {
        // for REVERSE, we use DRAWBLOCK --this is VERY inefficient, since we create a dummy bitmap for this (because Windows 8 does not have the proper API for this)
        if (fillcolor.MODE < 0)
        {
            // to invert the screen, we use DRAWBLOCK
            size_t evenbytesperrow = (w + 15) / 16 * 2;
            vector<char> bits(evenbytesperrow * h);
            fillcolor.MODE = 1; fillcolor.R = fillcolor.G = fillcolor.B = 255;
            RenderBlitBits(bits.data(), evenbytesperrow, 0, 0, w, h, x, x, 3, fillcolor, viewport);
            return;
        }

        // draw filled rectangle in the viewport
        m_d2dContext->PushAxisAlignedClip(RenderViewportRect(viewport), D2D1_ANTIALIAS_MODE_ALIASED);
        D2D_RECT_F r = { (float) x, (float) y, (float) x+w, (float) y+h };
        m_d2dContext->FillRectangle(r, CachedBrush(fillcolor));
        m_d2dContext->PopAxisAlignedClip();
        dirty = true;
    }

    // draw a line
    void RenderDrawLine(D2D1_POINT_2F from, D2D1_POINT_2F to, const PENSTATE & pen, const RECT & viewport)
    {
        m_d2dContext->PushAxisAlignedClip(RenderViewportRect(viewport), D2D1_ANTIALIAS_MODE_ALIASED);
        // TODO: in REVERSE mode, we could draw it in white into a temp rectangle and XOR it on like DRAWBLOCK
        m_d2dContext->DrawLine(from, to, CachedBrush(pen), 1.0f);
        m_d2dContext->PopAxisAlignedClip();
        dirty = true;
    }

    // TODO: test the extended modes fully
    void RenderBlitBits(const void * bitsource, size_t rowsize, size_t srcx, size_t srcy, size_t w, size_t h, int dstx, int dsty, BLITMODE xmode, const PENSTATE & pen, const RECT & viewport)
    {
        /*
        (*  MODE RANGES 0..15 TO FILL IN THE FOLLOWING TRUTH TABLE:             *)
        (*                                                                      *)
        (*               CURRENT SCREEN      SOURCE      RESULTANT SCREEN       *)
        (*              I---------------I---------------I---------------I       *)
        (*              I    FALSE      I    FALSE      I   LSB MODE    I       *)
        (*              I---------------I---------------I---------------I       *)
        (*              I    FALSE      I    TRUE       I               I       *)
        (*              I---------------I---------------I---------------I       *)
        (*              I    TRUE       I    FALSE      I               I       *)
        (*              I---------------I---------------I---------------I       *)
        (*              I    TRUE       I    TRUE       I   MSB MODE    I       *)
        (*              I---------------I---------------I---------------I       *)
         */
        // we support these:
        //  - 0: set to black   == copy (black, black)
        //  - 3: invert screen  == XOR with (white, white)
        //  - 5: inverted copy  == copy (white, black)
        //  - 6: XOR            == XOR with (black, white)
        //  - 10: copy          == copy (black, white)
        //  - 12: NOP
        //  - 13: OR with compl.== plus (white, black)
        //  - 14: OR            == plus (black, white)
        //  - 15: set to white  == copy (white, white)
        // TODO: we could also do AND with those blend modes
        // and special modes:
        //  - 0x1x: 1-bit means above drawmodes but using PENCOLOR, not WHITE (XOR not supported)
        //  - 0x20: data is 16-bit xcolor format, not bits; with transparent color support; modes 0..15 are ignored
        //  - 0xXxx: X=1sss scale by (sss+2), i.e. 2..9 times
        size_t scale = 1;
        BLITMODE mode = xmode & 0xf;
        if (xmode & 0x800)
            scale = 2 + ((xmode >> 8) & 7);     // scale bit
        if (xmode & 0x10 && pen.MODE == 0)      // NONE
            mode = 12;                          // BUGBUG: if transp. image brush works, this should stencil through (draw in transparent color)
        else if (xmode & 0x10 && pen.MODE < 0)  // use PENCOLOR but PENCOLOR is REVERSE
            xmode = 0;                          // render as XOR mode (ignore the pencolor)
        if (mode == 12)
            return;

        D2D_RECT_F srcRect = { (float) 0, (float) 0, (float) w, (float) h};
        D2D_RECT_F dstRect = { (float) dstx, (float) dsty, (float) dstx + w * scale, (float) dsty + h * scale};

        ComPtr<ID2D1Bitmap1> sourceAsBitmap;    // we draw this
        const float dpi = Windows::Graphics::Display::DisplayProperties::LogicalDpi;

        if (!(xmode & 0x20))                    // regular binary bitmap (Apple style)
        {
            unsigned int zero, one;
            unsigned int white = 0xffffffff;
            unsigned int black = 0xff000000;
            unsigned int transparent = 0;
            unsigned int pencolor = (xmode & 0x10) ? pen.rgba32() : 0xffffffff; // 0x10 overrides 'white' with PENCOLOR
            bool xormode = false;   // XOR or copy (possibly with background=transparent to implement OR)
            switch (mode)
            {
            // XOR modes
            case 3: zero = white; one = white; xormode = true; break;            // invert screen
            case 6: zero = black; one = white; xormode = true; break;            // XOR
            // OR modes--draw with transparency
            case 13: zero = pencolor; one = transparent; xormode = false; break;   // OR with complement of bitmap
            case 14: zero = transparent; one = pencolor; xormode = false; break;   // OR the bitmap
            // clear modes
            case 0: zero = black; one = black; xormode = false; break;             // set to black
            case 15: zero = pencolor; one = pencolor; xormode = false; break;      // set to white
            // copy modes
            // 5 with PENCOLOR(transparent) will paint the background as see-through to the text window!
            case 5: zero = pencolor; one = black; xormode = false; break;          // inverted copy
            default:
            case 10: zero = black; one = white; xormode = false; break;            // straight copy
            }

            // convert into a 32-bit pixel map, since Direct2D does NOT seem to support true good ole bit maps!!
            const char * bits = (const char *) bitsource;
            vector<unsigned int> pixels(w * h);
            // once we find out how to access the screen directly, we can do this manually here
            for (size_t y = 0; y < h; y++) for (size_t x = 0; x < w; x++)
            {
                size_t rowoffset = (y + srcy) * rowsize;
                size_t bitoffset = x + srcx;
                bool bit = (bits[rowoffset + bitoffset / 8] & (1 << (bitoffset % 8))) != 0;
                pixels[x + y * w] = bit ? one : zero;
            }

            // XOR mode requires pre-scaled bitmap
            size_t ws = w;
            size_t hs = h;
            if (xormode && scale > 1)
            {
                ws *= scale;
                hs *= scale;
                vector<unsigned int> scaledpixels(ws * hs);
                for (size_t ys = 0; ys < hs; ys++) for (size_t xs = 0; xs < ws; xs++)
                    scaledpixels[xs + ys * ws] = pixels[xs/scale + ys/scale * w];
                pixels.swap(scaledpixels);
            }

            // convert it into a bitmap
            D2D_SIZE_U size = { ws, hs };
            D2D1_BITMAP_PROPERTIES1 bitmapProperties = { D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi, dpi, D2D1_BITMAP_OPTIONS_TARGET, nullptr };
            m_d2dContext->CreateBitmap(size, pixels.data(), ws * sizeof(pixels[0]), &bitmapProperties, &sourceAsBitmap) || failwithhr;

            if (!xormode)      // copy mode
            {
                m_d2dContext->PushAxisAlignedClip(RenderViewportRect(viewport), D2D1_ANTIALIAS_MODE_ALIASED);
                m_d2dContext->DrawBitmap(sourceAsBitmap.Get(), &dstRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, &srcRect);
                m_d2dContext->PopAxisAlignedClip();
                dirty = true;
            }
            else                        // XOR mode --'pixels' is already pre-scaled since blend effects cannot scale
            {
                EndDraw();  // temporarily escape from current drawing; flushes everything

                // draw 
                m_d2dContext->BeginDraw();
                // pull the affected region into a temp bitmap
                ComPtr<ID2D1Bitmap1> tempBitmap;         // we draw here
                m_d2dContext->CreateBitmap(size/*note: scaled*/, nullptr, 0, &bitmapProperties, &tempBitmap) || failwithhr;

                m_d2dContext->SetTarget(tempBitmap.Get());
                D2D_RECT_F tmpRect = { (float) 0, (float) 0, (float) ws, (float) hs };
                m_d2dContext->DrawBitmap(drawingBitmap.Get(), &tmpRect/*dst*/, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, &dstRect/*src*/);
                // it is confirmed that this indeed gets us the bitmap out

                // Remove the render target and end drawing. 
                m_d2dContext->EndDraw() || failwithhr;

                BeginDraw();        // get back in
                m_d2dContext->SetTarget(drawingBitmap.Get());

                ComPtr<ID2D1Effect> blendEffect;
                m_d2dContext->CreateEffect(CLSID_D2D1Blend, &blendEffect);
                blendEffect->SetInput(0, tempBitmap.Get());         // destination
                blendEffect->SetInput(1, sourceAsBitmap.Get());     // source
                blendEffect->SetValue(D2D1_BLEND_PROP_MODE, D2D1_BLEND_MODE_EXCLUSION); // this is an XOR if mask data is binary
                // (FRGB + BRGB ?2 * FRGB * BRGB)
                // we want (bit - x)
                // (1 + BRGB ?2 * 1 * BRGB) = 1 ?BRGB
                // (0 + BRGB ?2 * 0 * BRGB) = BRGB
                // yes
#if 0       // for future reference: how to add
                m_d2dContext->CreateEffect(CLSID_D2D1Composite, &blendEffect);
                blendEffect->SetInput(0, tempBitmap.Get());         // destination
                blendEffect->SetInput(1, sourceAsBitmap.Get());     // source
                blendEffect->SetValue(D2D1_COMPOSITE_PROP_MODE, D2D1_COMPOSITE_MODE_PLUS/* | D2D1_COMPOSITE_MODE_MASK_INVERT*/);
#endif

                m_d2dContext->PushAxisAlignedClip(RenderViewportRect(viewport), D2D1_ANTIALIAS_MODE_ALIASED);
                m_d2dContext->DrawImage(blendEffect.Get(), D2D1::Point2F((float) dstx,(float) dsty));
                m_d2dContext->PopAxisAlignedClip();
            }
        }
        else            // xmode & 0x20: bitmap is a 16-bit RGB map
        {
            const unsigned short * pixels16 = (const unsigned short *) bitsource;
#if 0
            // convert it into a bitmap --it is already in the correct source format
            D2D_SIZE_U size = { w, h };
            D2D1_BITMAP_PROPERTIES1 bitmapProperties = { D2D1::PixelFormat(DXGI_FORMAT_B5G5R5A1_UNORM, D2D1_ALPHA_MODE_IGNORE), dpi, dpi, D2D1_BITMAP_OPTIONS_TARGET, nullptr };
            m_d2dContext->CreateBitmap(size, pixels, w * sizeof(pixels[0]), &bitmapProperties, &sourceAsBitmap) || failwithhr;
#else       // meh--supported in DirectX 11.1; the MacBook Air at least does not support it, so we need to copy...
            vector<unsigned int> pixels(w * h);
            // manually convert it --expensive!
            for (size_t y = 0; y < h; y++) for (size_t x = 0; x < w; x++)
            {
                size_t rowoffset = (y + srcy) * rowsize;
                size_t coloffset = x + srcx;
                unsigned short pixel16 = pixels16[rowoffset + coloffset];
                // convert
                unsigned int B = (pixel16 % 32) * 33 / 4;
                pixel16 >>= 5;
                unsigned int G = (pixel16 % 32) * 33 / 4;
                pixel16 >>= 5;
                unsigned int R = (pixel16 % 32) * 33 / 4;
                pixel16 >>= 5;      // alpha bit
                unsigned int A = pixel16 * 256 - pixel16;   // = * 255
                unsigned int pixel32 = (A<<24) + (R<<16) + (G<<8) + B;
                pixels[x + y * w] = pixel32;
            }
            // convert it into a bitmap
            D2D_SIZE_U size = { w, h };
            D2D1_BITMAP_PROPERTIES1 bitmapProperties = { D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi, dpi, D2D1_BITMAP_OPTIONS_TARGET, nullptr };
            m_d2dContext->CreateBitmap(size, pixels.data(), w * sizeof(pixels[0]), &bitmapProperties, &sourceAsBitmap) || failwithhr;
#endif
            // render it
            m_d2dContext->PushAxisAlignedClip(RenderViewportRect(viewport), D2D1_ANTIALIAS_MODE_ALIASED);
            m_d2dContext->DrawBitmap(sourceAsBitmap.Get(), &dstRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, &srcRect);
            m_d2dContext->PopAxisAlignedClip();
        }
        dirty = true;
    }
#if 0
        m_pRenderTarget->DrawText(
            sc_helloWorld,
            ARRAYSIZE(sc_helloWorld) - 1,
            m_pTextFormat,
            D2D1::RectF(0, 0, renderTargetSize.width, renderTargetSize.height),
            m_pBlackBrush
            );
#endif
    void RenderTextMode()
    {
        graphicsVisible = false;
    }

    void RenderGrafMode()
    {
        graphicsVisible = true;
    }

    // call back from timer tick
    // Reports the actual window size, so INITTURTLE can adapt as needed.
    // This first call to this must happen before the interpreter has started, so we can pick up the screen size.
    SurfaceImageSource^ tickcallback(size_t w, size_t h)
    {
        {
            lock_guard guard(this);
            screenWidth = w;            // remember last communicated values, used by INITTURTLE in interpreter thread
            screenHeight = h;
        }
        // pick up initialization and/or changes
        //CreateImageSource();
        // perform all queued operations
        if (!renderingOnHold)
        {
            bool doneanything = executedeferredops();
            // blit the drawing bitmap over
            if (doneanything)
                FinalizeDrawing();
        }
        return graphicsVisible ? bitmapImageSource : nullptr;
    }

    // returns whether we should be in graphics (true) or text mode (false)
    static SurfaceImageSource^ tickcallbackstatic(void * p, size_t w, size_t h)
    {
        return ((graphicsperipheral*)p)->tickcallback(w, h);
    }

#if 0
    //ComPtr<SurfaceImageSource> bitmapImageSource;
    void setsize(size_t w, size_t h)
    {
        bitmapWidth = w;
        bitmapHeight = h;
        RECT vp = { 0, 0, w, h };
        viewport = vp;
        nativeResolution = false;
        dirty = false;

        penbrush = nullptr;

        // full sample: http://code.msdn.microsoft.com/windowsapps/XAML-SurfaceImageSource-58f7e4d5/sourcecode?fileId=64193&pathId=1915311265

        // Use Scenario2Drawing as a source for the Image control 
        //    Image2.Source = Scenario2Drawing; 

        // note: TermCanvas may hold a reference to bitmapImageSource; the following creates a new one
        // TODO: how to deal with growing the area (window moved)?
        bitmapImageSource = ref new SurfaceImageSource(bitmapWidth, bitmapHeight, true);

        // get the native interface
        IInspectable* sisInspectable = (IInspectable*) reinterpret_cast<IInspectable*>(bitmapImageSource);
        sisInspectable->QueryInterface(__uuidof(ISurfaceImageSourceNative), (void **)&bitmapImageSourceAsNative) || failwithhr;

        CreateDeviceResources();
#endif
        // tell the terminal to draw with this
        //SetImageSource(bitmapImageSource, w, h);
#if 0
        Clear(rgb(0, 0, 0));
        Pen(rgb(255, 128, 0));
        FillSolidRect(D2D1::RectF(30,30,40,100));
        Pen(rgb(255, 255, 255));
        DrawLine(D2D1::Point2F(30,30), D2D1::Point2F(200,100));

        // and another
        // Pen carries over, but image doesn't... :( Not what we need.
        DrawLine(D2D1::Point2F(0,(float)h), D2D1::Point2F((float)w,0), 1);

        // now transfer it to the image source
        // We will do that lazily.
        CreateImageSource();
    }
#endif

public:

    // note: this is run before any first attempt of calling callbacks in the terminal
    graphicsperipheral(pmachine & themachine) :
        themachine(themachine),
        screenWidth(0), screenHeight(0), bitmapWidth(0), bitmapHeight(0), graphicsVisible(false),
        nativeResolution(false), renderingOnHold(false), dirty(false)
    {
        SYNCFRAMEfreq.QuadPart = 0;
        InitializeCriticalSectionEx(this, 0, 0);
        SetGraphicsTickCallback(tickcallbackstatic, this);
    }

private:

    // -----------------------------------------------------------------------
    // TURTLEGRAPHICS
    //  - routines that modify the screen are routed through a FIFO
    // -----------------------------------------------------------------------

    struct deferredop
    {
        TURTLEGRAPHICSprocs op;     // pnINITTURTLE, pnMOVETO, pnTEXTMODE, pnGRAFMODE, pnFILLSCREEN, pnDRAWBLOCK
        RECT viewport;              // current viewport
        int x1, y1, x2, y2;         // draw line, drawblock
        PENSTATE pen;
        vector<char> source;        // created by caller, released by render thread; Note: inefficient; we make 3 copies of this... don't care for now
        size_t rowsize;
        size_t xskip;                  // left-most pixel has this offset; yskip == 0
        BLITMODE mode;
        deferredop(){}
        deferredop(TURTLEGRAPHICSprocs pop) : op(pop) {}
    };
    vector<deferredop> opqueue;     // deferred ops are submitted here; access FIFO only under lock_guard
    void submitdeferredop(deferredop & op)
    {
        if (CURRENTVP.right < CURRENTVP.left || CURRENTVP.top < CURRENTVP.bottom)   // zero viewport
            return;
        op.viewport = CURRENTVP;    // current state
        lock_guard guard(this);     // FIFO is under lock
        opqueue.push_back(op);
        // we must avoid pumping the queue faster than the renderer on the UI can follow
        // If we fall behind by a reasonable amount of ops, we tell the interpreter to yield.
        // This is a last resort against a out-of-control drawing loop such as GRAFDEMO.CODE (which should add more WAIT loops)
        // We also yield in case of a full-screen FILLSCREEN(REVERSE) since that has the meaning of flashing the screen for the user, so better make it visible.
        // A better way would be to flash for a specific number of frames.
        // TODO: while HOLDON we should not do this, or at least increase the limit
        const size_t queueoverrunlimit = 100000;
        bool interrupt = opqueue.size() > queueoverrunlimit;
        if (op.op == pnFILLSCREEN && op.pen.MODE == -1/*REVERSE*/ && CURRENTVP.left == 0 && CURRENTVP.bottom == 0 && CURRENTVP.right == WIDTH && CURRENTVP.top == HEIGHT)
            interrupt = true;
        if (interrupt)
            themachine.yield(16);   // tell machine to yield a little time; we will keep doing this until the queue is flushed
    }
    // on render thread:
    bool executedeferredops()       // execute all currently queued ops
    {
        // fetch the deferred queue: we get the whole one and replace it with an empty one to which the interpreter thread keeps adding
        vector<deferredop> opq;
        {
            lock_guard guard(this); // FIFO is under lock
            opq.reserve(opqueue.capacity());  // keep the same amount of memory, as an indicator of how much drawing is being done
            opq.swap(opqueue);      // get current FIFO, replace FIFO with empty one
            // now we own it (it is only local), so we can now safely operate on our copy, without lock
        }
        bool doneanything = false;
        bool drawing = false;       // call BeginDraw()/EndDraw() lazily
        for (size_t k = 0; k < opq.size(); k++)
        {
            const deferredop & op = opq[k];
            switch (op.op)
            {
            case pnINITTURTLE:
                if (drawing) { EndDraw(); drawing = false; }    // init happens outside BeginDraw()/EndDraw()
                RenderInit(op.viewport.right, op.viewport.top);
                doneanything = true;
                break;
            case pnTEXTMODE:
                RenderTextMode();
                break;
            case pnGRAFMODE:
                RenderGrafMode();
                break;
            case pnFILLSCREEN:
                if (!drawing) { BeginDraw(); drawing = true; }
                RenderFillScreen(op.pen, op.x1, op.y1, op.x2-op.x1, op.y2-op.y1, op.viewport);
                doneanything = true;
                break;
            case pnMOVETO:
                if (!drawing) { BeginDraw(); drawing = true; }
                RenderDrawLine(D2D1::Point2F((float)op.x1,(float)op.y1), D2D1::Point2F((float)op.x2,(float)op.y2), op.pen, op.viewport);
                doneanything = true;
                break;
            case pnDRAWBLOCK:
                if (!drawing) { BeginDraw(); drawing = true; }
                RenderBlitBits(op.source.data(), op.rowsize, op.xskip, 0/*yskip*/, op.x2-op.x1/*width*/, op.y2-op.y1/*height*/, op.x1, op.y1, op.mode, op.pen, op.viewport);
                doneanything = true;
                break;
            default: throw std::logic_error("executedeferredop(): unexpected op code");
            }
        }
        if (drawing) { EndDraw(); drawing = false; }
        return doneanything;
    }

    // TURTLEGRAPHICS implementation, partially based on sample code in manual
    size_t WIDTH, HEIGHT;      // screen dimensions as used by this system set by INITTURTLE; either Apple or native, at time of INITTURTLE
    int TGXPOS, TGYPOS;
    int TGHEADING;
    RECT CURRENTVP;
    PENSTATE PEN;
    int CHARMODE;   // for WCHAR, WSTRING
    int ROUND(double f) { if (f > 0) return (int) (f + 0.5); else return -(int) (-f + 0.5); }
    vector<char> CHARSET;   // content of SYSTEM.CHARSET file, loaded when the UNIT is loaded
    pmachine & themachine;  // we call interrupt() on this if we have too many queue entries, to slow it down

    void LOADED()
    {
#if 0   // this does not work with II.0 since it does not have initialization code for UNITs
        clear();
        // load SYSTEM.CHARSET here
        themachine.loadsystemfile("SYSTEM.CHARSET", CHARSET);
#endif
    }

    // At time of INITTURTLE, the screen size is frozen based on the last actual size reported by the render thread.
    void INITTURTLE()
    {
        // lazy load the CHARSET file
        // TODO: may fail; then avoid trying again each time; needs a flag (fails: use Consolas font)
        if (CHARSET.empty())
        {
            auto rslt = themachine.loadsystemfile("SYSTEM.CHARSET", CHARSET);
            // no SYSTEM.CHARSET supplied by user: default to our font
            if (rslt != INOERROR)
            {
                CHARSET.reserve(2048);  // we create the full Latin-1 char set
                for (size_t ch1 = 0; ch1 < 256; ch1++)
                {
                    size_t ch = (ch1 >= 32) ? ch1 : ch1 + 256 - 32; // map last 32 chars here (greek symbols)
                    // TODO: this is a dup--we really need to create those 32 chars as something else, some symbols
                    size_t row = ch / 16;   // row of chars (they are in a 16 x 16 grid)
                    size_t col = ch % 16;   // and column 
                    for (int r = 7; r >= 0; r--)    // Apple CHARSET is from bottom to top
                    {
                        const char * s = SYSTEM_CHARSET_DATA[(row * 8 + r) * 16 + col];
                        char mask = 0;
                        for (size_t c = 0; c < 7; c++)
                        {
                            char bit = (s[c] == '#') ? 1 : 0;
                            mask |= (bit << c);
                        }
                        CHARSET.push_back(mask);
                    }
                }
            }
        }

        lock_guard guard(this);             // under lock since we access screenWidth/screenHeight
        if (screenWidth == 0 || screenHeight == 0)
            throw std::logic_error("INITTURTLE: interpreter was started before screen layout known and communicated through callback");
        WIDTH = nativeResolution ? screenWidth : 280;    // Apple dimensions unless user enlarged it before
        HEIGHT = nativeResolution ? screenHeight : 192;
        VIEWPORT(0, WIDTH-1, 0, HEIGHT-1);
        deferredop op(pnINITTURTLE);        // create bitmaps
        submitdeferredop(op);               // desired bitmap size is carried over as viewport argument
        TGHEADING = 0;                      // to the right (according to Apple Pascal manual)
        TGXPOS = (CURRENTVP.left + CURRENTVP.right) / 2;  // turtle into the center
        TGYPOS = (CURRENTVP.top + CURRENTVP.bottom) / 2;
        PENCOLOR(TGCOLOR_NONE);
        CHARMODE = 10;                      // default according to Apple manual
        FILLSCREEN(TGCOLOR_BLACK);
        GRAFMODE();
    }
    void TURN (int RELANGLE)
    {
        TGHEADING = (TGHEADING + RELANGLE) % 360;
    }
    void MOVE(int RELDISTANCE)
    {
        const double RADCONST = 57.29578;
        MOVETO(ROUND(TGXPOS + RELDISTANCE * cos(TGHEADING / RADCONST)),
               ROUND(TGYPOS + RELDISTANCE * sin(TGHEADING / RADCONST)));
    }
    void MOVETO(int X, int Y)
    {
        if (PEN.MODE != 0)
        {
            if (TGXPOS != X && TGYPOS != Y) // diagonal line
            {
                // TODO: we do not submit zero-pixel lines, but is that correct?
                deferredop op(pnMOVETO);    // line drawing is deferred
                op.pen = PEN;
                op.x1 = TGXPOS;
                op.y1 = TGYPOS;
                op.x2 = X;
                op.y2 = Y;
                submitdeferredop(op);
            }
            else if (TGXPOS != X)           // vertical
            {
                deferredop op(pnFILLSCREEN);
                op.pen = PEN;
                op.x1 = min(TGXPOS, X);
                op.y1 = TGYPOS;
                op.x2 = max(TGXPOS, X)+1;
                op.y2 = TGYPOS+1;
                submitdeferredop(op);
            }
            else if (TGYPOS != Y)
            {
                deferredop op(pnFILLSCREEN);
                op.pen = PEN;
                op.x1 = TGXPOS;
                op.y1 = min(TGYPOS, Y);
                op.x2 = TGXPOS+1;
                op.y2 = max(TGYPOS, Y)+1;
                submitdeferredop(op);
            }
            // else start == end: ignore
        }
        TGXPOS = X;
        TGYPOS = Y;
    }
    void TURNTO(int ANGLE)
    {
        TGHEADING = ANGLE;
    }
    void setpen(PENSTATE & pen, TGCOLOR PCOLOR)
    {
        pen.MODE = 1;
        pen.A = 255;
        if (PCOLOR & 0x8000)        // xcolor 5-bit RGB
        {
            int xc = PCOLOR;
            pen.B = ((xc & 0x1f) * 0x21) / 4; xc >>= 5;
            pen.G = ((xc & 0x1f) * 0x21) / 4; xc >>= 5;
            pen.R = ((xc & 0x1f) * 0x21) / 4;
            return;
        }
        else if (PCOLOR & 0x4000)   // xcolor transparent = see-through to text mode
        {
            pen.A = pen.R = pen.G = pen.B = 0;
            return;
        }
        else switch (PCOLOR)
        {
        case TGCOLOR_NONE: pen.MODE = 0; break;
        case TGCOLOR_WHITE: case TGCOLOR_WHITE1: case TGCOLOR_WHITE2:
            pen.R = 255; pen.G = 255; pen.B = 255; break;
        case TGCOLOR_BLACK: case TGCOLOR_BLACK1: case TGCOLOR_BLACK2:
            pen.R = 0; pen.G = 0; pen.B = 0; break;
        case TGCOLOR_REVERSE:
            pen.R = 255; pen.G = 255; pen.B = 255; pen.MODE = -1; break;
        // Apple:
        case TGCOLOR_RADAR: pen.R = 255; pen.G = 255; pen.B = 255; pen.MODE = -1; break; // "reserved for future use"
        case TGCOLOR_GREEN: pen.R = 0; pen.G = 255; pen.B = 0; break;
        case TGCOLOR_VIOLET: pen.R = 255; pen.G = 0; pen.B = 255; break;
        case TGCOLOR_ORANGE: pen.R = 255; pen.G = 128; pen.B = 0; break;
        case TGCOLOR_BLUE: pen.R = 0; pen.G = 0; pen.B = 255; break;
        }
    }
    void PENCOLOR(TGCOLOR PCOLOR) { setpen(PEN, PCOLOR); }
    void TEXTMODE() { submitdeferredop(deferredop(pnTEXTMODE)); }
    void GRAFMODE() { submitdeferredop(deferredop(pnGRAFMODE)); }
    void FILLSCREEN(TGCOLOR FILLCOLOR)
    {
        // special mode: when in native resolution, call FILLSCREEN with these to toggle hold mode
        if (FILLCOLOR == TGCOLOR_FILLSCREEN_HOLDON || FILLCOLOR == TGCOLOR_FILLSCREEN_HOLDOFF)
        {
            renderingOnHold = (FILLCOLOR == TGCOLOR_FILLSCREEN_HOLDON);
            return;
        }
        deferredop op(pnFILLSCREEN);    // drawing is deferred
        setpen(op.pen, FILLCOLOR);
        if (op.pen.MODE == 0)
            return;
        op.x1 = 0;
        op.y1 = 0;
        op.x2 = WIDTH;
        op.y2 = HEIGHT;
        submitdeferredop(op);           // (note: this will be a NOP if the viewport area is zero)
    }
    void VIEWPORT(int LEFT, int RIGHT, int BOTTOM, int TOP)
    {
        // if user calls VIEWPORT before INITTURTLE, then the screen gets enlarged --TODO: think this through; e.g. user could read out TURTLEX/Y and double it to get dimensions
        // e.g. call it with VIEWPORT(0,INTMAX,0,INTMAX), then read out TURTLEX/Y to know actual size
        nativeResolution |= RIGHT >= 280 || TOP >= 192;    // we use this as a trigger; really, we will have a new function called GETSCRNVP
        if (RIGHT > (int)WIDTH-1) RIGHT = WIDTH-1;
        if (TOP > (int)HEIGHT-1) TOP = HEIGHT-1;
        CURRENTVP.left = LEFT;
        CURRENTVP.top = TOP +1;     // remember Apple Pascal has origin in lower left corner
        CURRENTVP.right = RIGHT +1; // RIGHT and TOP are inclusive, per some Apple sample code
        CURRENTVP.bottom = BOTTOM;
    }

    int TURTLEX() { return TGXPOS; }
    int TURTLEY() { return TGYPOS; }
    int TURTLEANG() { return TGHEADING; }
    bool SCREENBIT(int X, int Y) { return false; }  // cannot support this with current FIFO architecture; maybe in the future
    void DRAWBLOCK(const void * SOURCE, int ROWSIZE/*bytes per row; even in sample*/, int XSKIP, int YSKIP, int WIDTH, int HEIGHT, int XSCREEN, int YSCREEN, int MODE)
    {
        if (MODE == 12) // do nothing
            return;
        deferredop op(pnDRAWBLOCK);
        op.x1 = XSCREEN;
        op.y1 = YSCREEN;
        op.x2 = XSCREEN + WIDTH;
        op.y2 = YSCREEN + HEIGHT;
        op.xskip = XSKIP;
        op.mode = MODE;
        op.rowsize = ROWSIZE;
        op.pen = PEN;           // used when use-PENCOLOR flag is set
        // due to pipeline nature, we must copy the bitmap to send it over
        const char * p = (const char *) SOURCE;
        p += ROWSIZE * YSKIP;
        op.source.assign(p, p + HEIGHT * ROWSIZE);
        submitdeferredop(op);           // (note: this will be a NOP if the viewport area is zero)
    }
    void WCHAR(int ch)
    {
        if (ch < 0 || ch > 255)
            throwoutofbounds();
        if (ch * 8 + 7 < (int) CHARSET.size())    // we ignore out-of-bounds characters completely (Apple Pascal supports up to code 127, ours has up to 255)
            DRAWBLOCK(&CHARSET[ch * 8], 1, 0, 0, 7, 8, TGXPOS, TGYPOS, CHARMODE);
        size_t scale = 1;       // WCHAR also supports our new scaling-mode bit
        if (CHARMODE & 0x800) scale = 2 + ((CHARMODE >> 8) & 7);    // scale bits
        TGXPOS += 7 * scale;
    }
    void WSTRING(const pstring & S)
    {
        for (size_t k = 1; k <= S.size(); k++)
            WCHAR(S.at(k));
    }
    void CHARTYPE(int MODE) { CHARMODE = MODE; }

    // wait until the VBL interval
    // and return ms since this was last called
    // Intended for game loops.
    LARGE_INTEGER SYNCFRAMEfreq, SYNCFRAMElast;
    size_t lastms;
    int SYNCFRAME()
    {
        if (SYNCFRAMEfreq.QuadPart == 0)    // first time
        {
            if (!QueryPerformanceFrequency (&SYNCFRAMEfreq)) // count ticks per second
                throw std::runtime_error ("auto_timer: QueryPerformanceFrequency failure");
            QueryPerformanceCounter (&SYNCFRAMElast);
        }
        LARGE_INTEGER now;
        QueryPerformanceCounter (&now);
        LARGE_INTEGER elapsed, reported, missing;
        elapsed.QuadPart = now.QuadPart - SYNCFRAMElast.QuadPart;   // number of ticks since last time
        // round down to ms
        const int waitms = 1000/60;
        size_t reportedms = waitms + (size_t) (elapsed.QuadPart * 1000 / SYNCFRAMEfreq.QuadPart);    // rounded down --this is what we report
        if (reportedms < 0)          // could be due to the trickery we are doing here
            reportedms = 0;
        reported.QuadPart = reportedms * SYNCFRAMEfreq.QuadPart / 1000;
        missing.QuadPart = elapsed.QuadPart - reported.QuadPart;                // reporting this dropped this many ticks
        SYNCFRAMElast.QuadPart = now.QuadPart - missing.QuadPart;               // so we keep them for next call
        themachine.yield(waitms);   // TODO: implement this properly, using the actual VBL info (render-thread tick)
        return reportedms;          // report 'waitms' more than actually have passed until the yield() call
        // is this really correct??
    }
};

pperipheral * creategraphicsperipheral(pmachine & themachine) { return new graphicsperipheral(themachine); }
TURTLEGRAPHICS * toTURTLEGRAPHICS(pperipheral* p)
{
    auto gp = dynamic_cast<graphicsperipheral*>(p);
    return gp;
}

};
