/* Copyright 1996-2017 Arch D. Robison 

   Licensed under the Apache License, Version 2.0 (the "License"); 
   you may not use this file except in compliance with the License. 
   You may obtain a copy of the License at 
   
       http://www.apache.org/licenses/LICENSE-2.0 
       
   Unless required by applicable law or agreed to in writing, software 
   distributed under the License is distributed on an "AS IS" BASIS, 
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
   See the License for the specific language governing permissions and 
   limitations under the License.
 */

/******************************************************************************
 OS specific services built on DirectX
*******************************************************************************/

#define WIN32_LEAN_AND_MEAN

#undef UNICODE

#define _WIN32_WINNT 0x501 /* Necessary to get WM_MOUSELEAVE */

#define HAVE_SOUND 0

#include <d3d9.h>       // DirectX 9 includes
#include <CommDlg.h>    // Common dialogs
#include <Ole2.h>
#include <gdiplus.h>
#include <stdio.h>
#include "AssertLib.h"
#include "BuiltFromResource.h"
#include "Config.h"
#include "Game.h"
#include "Host.h"
#include "NimbleDraw.h"
#include "Parallel.h"
#include "TraceLib.h"
#if HAVE_SOUND
#include "SimpleSound.h"
#endif /* HAVE_SOUND */

#define WINDOW_CLASS_NAME "WINXCLASS"   // Class name

//! True iff running in exclusive screen mode
/** Use ExclusiveMode=false for interactive debugging. */

#ifdef EXCLUSIVE_MODE
const bool ExclusiveMode = EXCLUSIVE_MODE;
#elif ASSERTIONS
const bool ExclusiveMode = false;
#else
const bool ExclusiveMode = true;
#endif

//! Common display sizes for testing
enum CommonDisplaySize {
    XGA = 1024<<16 | 768,
    WXGA = 1366<<16 | 768,
    FHD = 1920<<16 | 1080,
    WUXGA = 1920<<16 | 1200,
    WQHD = 2560<<16 | 1440
};

//! Size to use in non-exclusive mode.
static CommonDisplaySize TestDisplaySize = XGA;

static int DisplayWidth;
static int DisplayHeight;
static short WindowWidth = ExclusiveMode ? 1024 : TestDisplaySize>>16;
static short WindowHeight = ExclusiveMode ? 768 : (TestDisplaySize&0xFFFF);

//! Set to true between time that a WM_SIZE message is received and GameResizeOrMove is called.
static bool ResizeOrMove = false;   
 
#define CREATE_LOG 0
#if CREATE_LOG
FILE* LogFile;
#endif /* CREATE_LOG */

//! Handle for main window
static HWND MainWindowHandle;   
static HINSTANCE MainHinstance;

// The DirectDraw object from which the device is created.
static IDirect3D9* d3d = NULL;

// Device upon which to draw
static IDirect3DDevice9* Device = NULL;

//! Return the greater of i rounded to a multiple of four or 128;
/** Rounds down in the case of a number that is of the form 4k+2 */
static int RoundAndClip( int i ) {
    int j = (i+1)>>2<<2;
    if( j<128 ) j=128;
    return j;
}

//! Decode error code from DirectX (only used during debugging)
static const char* Decode( HRESULT hr ) {
    switch( hr ) {
        case D3DERR_DEVICELOST:
            return "D3DERR_DEVICELOST";
        case D3DERR_INVALIDCALL:
            return "D3DERR_INVALIDCALL";
        case D3DERR_NOTAVAILABLE:
            return "D3DERR_NOTAVAILABLE";
        case D3DERR_OUTOFVIDEOMEMORY:
            return "D3DERR_OUTOFVIDEOMEMORY";
        default:
            return "<unknown>";
    }
}

void HostExit() {
    PostMessage(MainWindowHandle, WM_DESTROY,0,0);
}

static bool RequestedTrackMouseEvent = false;

// This is the main message handler of the system
static LRESULT CALLBACK WindowProc(HWND hwnd,
                            UINT msg, 
                            WPARAM wparam, 
                            LPARAM lparam)
{                       
    switch(msg) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP: {
            NimblePoint p( (int)LOWORD(lparam), (int)HIWORD(lparam) );
            if( msg==WM_LBUTTONDOWN )
                GameMouseButtonDown( p, 0 );
            else
                GameMouseButtonUp( p, 0 );
            break;
        }
        case WM_MOUSELEAVE:
            RequestedTrackMouseEvent = false;
            HostShowCursor(true);
            break;

        case WM_MOUSEMOVE: {

            if (!RequestedTrackMouseEvent) {
                // First time mouse entered my window:
                // Request leave notification
                TRACKMOUSEEVENT tme;
                tme.cbSize = sizeof(tme);
                tme.hwndTrack = hwnd;
                tme.dwFlags = TME_LEAVE;
                TrackMouseEvent(&tme);
                RequestedTrackMouseEvent = true;
            }

            NimblePoint p( (int)LOWORD(lparam), (int)HIWORD(lparam) );
            GameMouseMove( p );
            break;
        }
        case WM_CREATE:
            // Do initialization stuff here
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;        // used in WM_PAINT
            HDC         hdc;       // handle to a device context

            // Start painting
            hdc = BeginPaint(hwnd,&ps);
#if CREATE_LOG
            if(hdc==NULL) {
                fprintf(LogFile, "BeginPaint returned NULL\n");
                fflush(LogFile);
            } else {
                fprintf(LogFile, "BeginPaint returned okay\n");
            }
#endif

            // End painting
            EndPaint(hwnd,&ps);
#if CREATE_LOG
            fprintf(LogFile,"EndPaint returned\n");
#endif
            return 0;
        }
        case WM_MOVE:
            return 0;
        
        case WM_SIZE:
            // Extract new window size
            // Round to multiple of four, because some of our loop unrollings depend upon such.
            WindowWidth = RoundAndClip(LOWORD(lparam));
            WindowHeight = HIWORD(lparam);
            ResizeOrMove = true;
            return 0;
        
        case WM_COMMAND:
            break;

        case WM_DESTROY:
            // Kill the application
            PostQuitMessage(0);
            return 0;

        case WM_CHAR: {
            if( 'A'<=wparam && wparam<='Z' )
                wparam = wparam-'A'+'a';
            GameKeyDown(wparam);
            return 0;
        }
        default:
            break;

    } // end switch

    // Process any messages that we didn't take care of
    return (DefWindowProc(hwnd, msg, wparam, lparam));

} // end WinProc

static void ProductionAssert( bool predicate, const char * msg ) {
    if( !predicate ) {
        char* lpMsgBuf;
        FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            GetLastError(),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
            (char*)&lpMsgBuf,
            0,
            NULL
            );
        // Display the string.
        MessageBoxA( NULL, msg, lpMsgBuf, MB_OK | MB_ICONINFORMATION );
        // Free the buffer.
        LocalFree( lpMsgBuf );
        exit(1);
    }
}

void HostLoadResource( BuiltFromResourcePixMap& item ) {
    HRSRC x = ::FindResourceA(  NULL, item.resourceName(), "PNG" );
    ProductionAssert(x,item.resourceName());
    DWORD n = SizeofResource( NULL, x); 
    Assert(n);
    HGLOBAL g = ::LoadResource( NULL, x );
    Assert(g);
    const void* r=LockResource(g);
    Assert(r!=NULL);
    HGLOBAL buf = ::GlobalAlloc(GMEM_MOVEABLE, n);
    Assert(buf);
    char* png = (char*)::GlobalLock(buf);
    Assert(png);
    memcpy(png,r,n);
    // Following assertion check that it is a PNG resource.
    Assert(memcmp(png+1,"PNG",3)==0 );
    IStream* s = NULL;
    HRESULT streamCreationStatus = CreateStreamOnHGlobal(buf,FALSE,&s);
    Assert( streamCreationStatus==S_OK );
    Assert(s);
    Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromStream(s,FALSE);
    ProductionAssert(bitmap,"Gdiplus::Bitmap::FromStream returned false");
    Gdiplus::Status fromStreamStatus = bitmap->GetLastStatus();
    ProductionAssert(fromStreamStatus==Gdiplus::Ok,"Gdiplus::Bitmap::FromStream failed");
    s->Release();
    ::GlobalUnlock(buf);
    ::GlobalFree(buf);

    int w=bitmap->GetWidth();
    int h=bitmap->GetHeight();
    const Gdiplus::Rect rect(0,0,w,h);
    Gdiplus::BitmapData lockedBits;
    Gdiplus::Status lockStatus = bitmap->LockBits(&rect,0,PixelFormat32bppARGB,&lockedBits);
    Assert( lockStatus==Gdiplus::Ok);
    NimblePixMap map(w,h,8*sizeof(NimblePixel),lockedBits.Scan0,lockedBits.Stride);
    item.buildFrom(map);
    Gdiplus::Status unlockStatus = bitmap->UnlockBits(&lockedBits);
    delete bitmap;
    Assert(unlockStatus==Gdiplus::Ok);
    return;
}

static void LoadResourceBitmaps(HINSTANCE hInstance) {
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    int w, h;
    if( ExclusiveMode ) {
        w = DisplayWidth;
        h = DisplayHeight;
    } else {
        w = WindowWidth;
        h = WindowHeight;
    }
    GameInitialize(w,h);
    Gdiplus::GdiplusShutdown(gdiplusToken);
}

#include <float.h>

//! Initialize floating-point control state.
/** Disables floating point exceptions */
static void InitializeFloatingPoint(void)
{
    _control87(MCW_EM,MCW_EM);
}

static double TotalClockTime;
static LARGE_INTEGER OldClockFreq;
static LARGE_INTEGER OldClockCount;

static void InitializeClock() {
    BOOL countStatus = QueryPerformanceCounter(&OldClockCount); 
    Assert(countStatus);
    BOOL freqStatus = QueryPerformanceFrequency(&OldClockFreq);  
    Assert(freqStatus);
};

double HostClockTime() {
    // The following algorithms attempts to compensate for dynamic frequency scaling
    // by using the average frequency between now and the previous clock reading.
    LARGE_INTEGER count, freq;
    BOOL counterStatus = QueryPerformanceCounter(&count); 
    Assert(counterStatus);
    BOOL freqStatus = QueryPerformanceFrequency(&freq);  
    Assert(freqStatus);
    TotalClockTime += 2*double(count.QuadPart-OldClockCount.QuadPart)/(freq.QuadPart+OldClockFreq.QuadPart);
    OldClockCount = count;
    OldClockFreq = freq;
    return TotalClockTime; 
}

bool HostIsKeyDown( int key ) {
    switch( key ) {
        case HOST_KEY_LEFT:  key=VK_LEFT;  break;
        case HOST_KEY_RIGHT: key=VK_RIGHT; break;
        case HOST_KEY_UP:    key=VK_UP;    break;
        case HOST_KEY_DOWN:  key=VK_DOWN;  break;
        default:
            if( 'a'<=key && key<='z' ) 
                key=toupper(key);
            break;
    }
    return GetKeyState(key)<0;
}

static void SetPresentParameters( D3DPRESENT_PARAMETERS& present ) {
    ZeroMemory( &present, sizeof(D3DPRESENT_PARAMETERS) );
    present.BackBufferCount           = 2;
    present.SwapEffect                = D3DSWAPEFFECT_DISCARD;
    present.BackBufferFormat          = D3DFMT_X8R8G8B8;
    present.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    present.Windowed                  = !ExclusiveMode;

    if( ExclusiveMode ) {
        present.BackBufferHeight = DisplayHeight;
        present.BackBufferWidth = DisplayWidth;
    }
}

void HostSetFrameIntervalRate( int limit ) {
    D3DPRESENT_PARAMETERS present;
    SetPresentParameters(present);
#if CREATE_LOG
	fprintf(LogFile, "BackBufferCount=%u\n", present.BackBufferCount);
	fprintf(LogFile, "SwapEffect=%u\n", present.SwapEffect);
	fprintf(LogFile, "PresentationInterval=%u\n", present.PresentationInterval);
    fflush(LogFile);
#endif
    present.PresentationInterval = limit==0 ? D3DPRESENT_INTERVAL_IMMEDIATE : 
                                   limit==1 ? D3DPRESENT_INTERVAL_DEFAULT : 
                                              D3DPRESENT_INTERVAL_TWO;
    Device->Reset(&present);
}

static bool InitializeDirectX( HWND hwnd ) {
    HRESULT hr;

    // Create the Direct3D interface object.
    d3d = Direct3DCreate9( D3D_SDK_VERSION );
    Assert(d3d);

    // Get device capabilities
    D3DCAPS9 displayCaps;
    hr = d3d->GetDeviceCaps(0,D3DDEVTYPE_HAL,&displayCaps);
    Assert(hr==D3D_OK);

    // Get resolution of current display mode
    D3DDISPLAYMODE displayMode;
    hr = d3d->GetAdapterDisplayMode(0,&displayMode);
    Assert(hr==D3D_OK);
    DisplayHeight = displayMode.Height;
    DisplayWidth = displayMode.Width;
    if( DisplayHeight>DISPLAY_HEIGHT_MAX || DisplayWidth>DISPLAY_WIDTH_MAX ) {
        char buffer[200];
        sprintf_s(buffer,sizeof(buffer),
                  "Display is too big (%dx%d).\n"
                  "Please set your display to a 32-bit color mode no bigger than %dx%d.\n"
                  "Then restart Seismic Duck.\n",
                  DisplayWidth,DisplayHeight,DISPLAY_WIDTH_MAX,DISPLAY_HEIGHT_MAX);
        MessageBoxA(hwnd,buffer,NULL,MB_OK|MB_ICONWARNING);
        return false;
    }

    if( DisplayHeight<DISPLAY_HEIGHT_MIN || DisplayWidth<DISPLAY_WIDTH_MIN ) {
        char buffer[200];
        sprintf_s(buffer,sizeof(buffer),
                  "Display is too small (%dx%d).\n"
                  "Please set your display to a 32-bit color mode no smaller than %dx%d.\n"
                  "Then restart Seismic Duck.\n",
                  DisplayWidth,DisplayHeight,DISPLAY_WIDTH_MIN,DISPLAY_HEIGHT_MIN);
        MessageBoxA(hwnd,buffer,NULL,MB_OK|MB_ICONWARNING);
        return false;
    }

    // Create the device
    D3DPRESENT_PARAMETERS present;
    SetPresentParameters(present);
    hr = d3d->CreateDevice( 
        D3DADAPTER_DEFAULT, 
        D3DDEVTYPE_HAL, 
        hwnd, 
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &present,
        &Device
    );
    if ( hr!=D3D_OK  ) {
        char buffer[200];
        sprintf_s(buffer,200,"Internal error: d3d->CreateDevice returned %s\n",Decode(hr));
        MessageBoxA(hwnd,buffer,NULL,MB_OK|MB_ICONWARNING);
        return false;
    }

    return true;
}

//! Release all resources allocated to DirectX
static void ShutdownDirectX() {
    if( Device ) 
        Device->Release();
    if( d3d )
        d3d->Release();
} 

#if CREATE_LOG
static void CheckResult(HRESULT hr, const char* expr)
#else
static inline void CheckResult(HRESULT hr)
#endif
{
#if CREATE_LOG
    if(hr!=D3D_OK) {
        fprintf(LogFile, "%s returned error %s\n", expr, Decode(hr));
        fflush(LogFile);
    } else {
        fprintf(LogFile, "%s returned D3D_OK\n", expr);
    }
#endif
    Assert(hr==D3D_OK);
}

#if CREATE_LOG
#define CHECK_RESULT(e) CheckResult(e,#e)
#else
#define CHECK_RESULT(e) CheckResult(e)
#endif

static void UpdateAndDraw( HWND hwnd ) {
    CHECK_RESULT(Device->BeginScene());
    IDirect3DSurface9* backBuffer=0;
    CHECK_RESULT(Device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&backBuffer));
    D3DSURFACE_DESC desc;
    CHECK_RESULT(backBuffer->GetDesc(&desc));
    D3DLOCKED_RECT lockedRect;
    CHECK_RESULT(backBuffer->LockRect(&lockedRect,NULL,D3DLOCK_NOSYSLOCK|D3DLOCK_DISCARD));
    double t0 = HostClockTime();
    NimblePixMap pixmap(desc.Width,desc.Height,32,lockedRect.pBits,lockedRect.Pitch);
    if( ResizeOrMove ) {
        GameResizeOrMove( pixmap );
        ResizeOrMove = false;
    }
#if CREATE_LOG
	static double previousTime;
	double currentTime = HostClockTime();
	fprintf(LogFile, "drawing into %p at time %.3f, delta %.5f\n", pixmap.at(0, 0), HostClockTime(), currentTime - previousTime);
    fflush(LogFile);
	previousTime = currentTime;
#endif
	GameUpdateDraw(pixmap, NimbleUpdate | NimbleDraw);

    extern void ThrottleWorkers(double,double);         // Defined in Parallel.cpp
#if CREATE_LOG
    fprintf(LogFile, "calling ThrottleWorkers\n");
    fflush(stderr);
#endif
    ThrottleWorkers(t0,HostClockTime());
#if CREATE_LOG
    fprintf(LogFile, "returned from ThrottleWorkers\n");
    fflush(stderr);
#endif
    CHECK_RESULT(backBuffer->UnlockRect());
    backBuffer->Release();
    CHECK_RESULT(Device->EndScene());
    CHECK_RESULT(Device->Present( 0, 0, 0, 0 ));
}

void HostShowCursor( bool show ) {
    static bool CursorIsVisible = true;
    if( show!=CursorIsVisible ) {
        CursorIsVisible = show;
        ShowCursor(show);
    }
}

int WINAPI WinMain( HINSTANCE hinstance,
                    HINSTANCE hprevinstance,
                    LPSTR lpcmdline,
                    int ncmdshow)
{
#if CREATE_LOG
    LogFile = fopen("C:/tmp/log.txt","w");
    fprintf(LogFile,"enter WinMain\n");
    fflush(LogFile);
#endif /* CREATE_LOG */
    MainHinstance = hinstance;

    InitializeFloatingPoint();
    InitializeClock();
    WNDCLASS winclass;  // this will hold the class we create
    HWND     hwnd;      // generic window handle
    MSG      msg;       // generic message

    // first fill in the window class stucture
    winclass.style          = CS_DBLCLKS | CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    winclass.lpfnWndProc    = WindowProc;
    winclass.cbClsExtra     = 0;
    winclass.cbWndExtra     = 0;
    winclass.hInstance      = hinstance;
    winclass.hIcon          = LoadIcon(NULL, IDI_APPLICATION);
    winclass.hCursor        = LoadCursor(NULL, IDC_ARROW);
    winclass.hbrBackground  = (HBRUSH)GetStockObject(BLACK_BRUSH);
    winclass.lpszMenuName   = NULL;
    winclass.lpszClassName  = WINDOW_CLASS_NAME;


    // Register the window class
    if (!RegisterClass(&winclass)) {
#if CREATE_LOG
        fprintf(LogFile,"cannot register class\n");
        fclose(LogFile);
#endif /* CREATE_LOG */
        return 0;
    }

    // Create the window, note the use of WS_POPUP
    hwnd = CreateWindow(WINDOW_CLASS_NAME,                  // class
                              GameTitle(),      // title
                              WS_POPUP|(ExclusiveMode?0:WS_VISIBLE)|(ExclusiveMode?0:WS_SIZEBOX|WS_BORDER|WS_CAPTION)/*WS_OVERLAPPEDWINDOW | WS_VISIBLE*/,
                              0,                        // x
                              0,                        // y
                              WindowWidth,              // width
                              WindowHeight,             // height
                              NULL,                     // handle to parent
                              NULL,                     // handle to menu
                              hinstance,                // instance
                              NULL);                    // creation parms
    if( !hwnd ) {
#if CREATE_LOG
        fprintf(LogFile,"cannot create main window\n");
        fclose(LogFile);
#endif /* CREATE_LOG */
        return 0;
    }
#if CREATE_LOG
    fprintf(LogFile,"WinMain registered main window\n");
    fflush(LogFile);
#endif /* CREATE_LOG */
    srand(2);
    MainWindowHandle = hwnd;
#if USE_HOST_MENU_SYSTEM
    HostSetMenuBar(0);
    UpdateMenu();
#endif
#if USE_CILK
    // If using Cilk, start run-time now, because if started later, it causes a D3DERR_DEVICELOST issue.
    cilk_for( int i=0; i<1; ++i ) {};
#endif

    // Perform DirectX intialization
    if( !InitializeDirectX(hwnd) )
        return 0;

#if HAVE_SOUND
    if( !InitializeDirectSound(hwnd) ) 
        return 0;
#endif /* HAVE_SOUND */

    LoadResourceBitmaps(hinstance);
#if CREATE_LOG
    fprintf(LogFile,"WinMain loaded resource bitmap\n");
    fflush(LogFile);
#endif /* CREATE_LOG */

    // Enter main event loop
    for(;;) {
        if (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) { 
#if CREATE_LOG
            fprintf(LogFile, "PeekMessage returns with msg.message=%x\n", msg.message);
            fflush(LogFile);
#endif /* CREATE_LOG */
            // Test if this is a quit
            if (msg.message == WM_QUIT)
               break;
        
            // Translate any accelerator keys
            TranslateMessage(&msg);
    
            // Send the message to the window proc
            DispatchMessage(&msg);
        }
        
        // Main game processing goes here
        UpdateAndDraw(hwnd);
    }
    
    // Shutdown game and release all resources
#if HAVE_SOUND
    ShutdownDirectSound();
#endif /* HAVE_SOUND */
    ShutdownDirectX();

    // Return to Windows 
    return msg.wParam;

} // end WinMain
