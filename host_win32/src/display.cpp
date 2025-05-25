#include "display.h"
#pragma comment(lib, "d2d1.lib")
#include "d2d1.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <assert.h>
#include "task.h"
#include <gfx.hpp>
#include <uix.hpp>
#if LCD_BIT_DEPTH == 16
#define LCD_RGB565
#endif
using namespace gfx;
using namespace uix;
extern HINSTANCE hInst;  // current instance
extern HWND hWndMain;
static HWND hwnd_dx=NULL;

extern task_mutex_t app_mutex;
static ID2D1HwndRenderTarget* render_target = nullptr;
static ID2D1Factory* d2d_factory = nullptr;
static ID2D1Bitmap* render_bitmap = nullptr;
// mouse mess
static struct {
    int x;
    int y;
} mouse_loc;
static int mouse_state = 0;  // 0 = released, 1 = pressed
static int old_mouse_state = 0;
static int mouse_req = 0;


static uix::display lcd;

static void flush_bitmap(int x1, int y1, int w, int h, const void* bmp) {
    if (render_bitmap != NULL) {
        D2D1_RECT_U b;
        b.top = y1;
        b.left = x1;
        b.bottom = y1 + h ;
        b.right = x1 + w ;
#if !defined(LCD_RGB565) && !defined(LCD_RGB565_UNSWAPPED)
        render_bitmap->CopyFromMemory(&b, bmp, w * 4);
#else
        uint32_t* bmp32=(uint32_t*)malloc(w*h*4);
        if(bmp32==NULL) return;
        uint32_t* pd = bmp32;
        const uint16_t* ps = (uint16_t*)bmp;
        size_t pix_left = (size_t)(w*h);
        while(pix_left-->0) {
#ifndef LCD_RGB565_UNSWAPPED
            uint16_t val = (((*ps)>>8)&0xFF)|(((*ps++)&0xFF)<<8);
#else
            uint16_t val = *ps++;
#endif
            uint8_t r = (val>>0)&31;
            uint8_t g = (val>>5)&63;
            uint8_t b = (val>>11)&31;
            uint32_t px = ((r*255)/31)<<0;
            px |= (((g*255)/63)<<8);
            px |= (((b*255)/31)<<16);
            *(pd++)=px;
        }
        render_bitmap->CopyFromMemory(&b,bmp32 , w * 4);
        free(bmp32);
#endif   
    }
}
static bool read_mouse(int* out_x, int* out_y) {
    if (0 == task_mutex_lock(
                             app_mutex,    // handle to mutex
                             -1)) {  // no time-out interval

        if (mouse_state) {
            *out_x = mouse_loc.x;
            *out_y = mouse_loc.y;
        }
        mouse_req = 0;
        task_mutex_unlock(app_mutex);
        return mouse_state;
    }
    return false;
}


static LRESULT CALLBACK WindowProcDX(HWND hWnd, UINT uMsg, WPARAM wParam,
                              LPARAM lParam) {
    // shouldn't get this, but handle anyway
    if (uMsg == WM_SIZE) {
        if (render_target) {
            D2D1_SIZE_U size = D2D1::SizeU(LOWORD(lParam), HIWORD(lParam));
            render_target->Resize(size);
        }
    }
    if (uMsg == WM_LBUTTONDOWN && hWnd == hwnd_dx) {
        if (LOWORD(lParam) < LCD_WIDTH &&
            HIWORD(lParam) < LCD_HEIGHT) {
            SetCapture(hwnd_dx);
            
            if (0==task_mutex_lock(app_mutex,-1)) {  // no time-out interval
                old_mouse_state = mouse_state;
                mouse_state = 1;
                mouse_loc.x = LOWORD(lParam);
                mouse_loc.y = HIWORD(lParam);
                mouse_req = 1;
                task_mutex_unlock(app_mutex);
            }
        }
    }
    if (uMsg == WM_MOUSEMOVE &&
        hWnd == hwnd_dx) {
        if (0==task_mutex_lock(app_mutex,-1)) {
            if (mouse_state == 1 && MK_LBUTTON == wParam) {
                mouse_req = 1;
                mouse_loc.x = (int16_t)LOWORD(lParam);
                mouse_loc.y = (int16_t)HIWORD(lParam);
            }
            task_mutex_unlock(app_mutex);
        }
    }
    if (uMsg == WM_LBUTTONUP &&
        hWnd == hwnd_dx) {
        ReleaseCapture();
        if (0==task_mutex_lock(app_mutex,-1)) {  
            old_mouse_state = mouse_state;
            mouse_req = 1;
            mouse_state = 0;
            mouse_loc.x = (int16_t)LOWORD(lParam);
            mouse_loc.y = (int16_t)HIWORD(lParam);
            task_mutex_unlock(app_mutex);
        }
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void display_init() {
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInst;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpfnWndProc = WindowProcDX;
    wcex.lpszClassName = L"WIN32_HOST_DX";
    wcex.hIcon = NULL;
    wcex.hIconSm = NULL;
    if(0==RegisterClassExW(&wcex)) {
        DWORD err = GetLastError();
        assert(false);
    }
    hwnd_dx = CreateWindowW(wcex.lpszClassName, L"", WS_CHILDWINDOW | WS_VISIBLE, 0,
                                 0, LCD_WIDTH, LCD_HEIGHT, hWndMain, NULL, hInst, NULL);
    if(hwnd_dx==NULL) {
        DWORD err = GetLastError();
        assert(false);
    }
    // start DirectX
    HRESULT hr =
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_factory);
    assert(hr == S_OK);
    // set up our direct2d surface
    {
        RECT rc;
        GetClientRect(hwnd_dx, &rc);
        D2D1_SIZE_U size =
            D2D1::SizeU((rc.right - rc.left + 1), rc.bottom - rc.top + 1);

        hr = d2d_factory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(hwnd_dx, size), &render_target);
        assert(hr == S_OK);
    }
    // initialize the render bitmap
    {
        D2D1_SIZE_U size = {0};
        D2D1_BITMAP_PROPERTIES props;
        render_target->GetDpi(&props.dpiX, &props.dpiY);
        D2D1_PIXEL_FORMAT pixelFormat = D2D1::PixelFormat(
#ifdef USE_RGB
            DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
#else
            DXGI_FORMAT_B8G8R8A8_UNORM,
#endif
            D2D1_ALPHA_MODE_IGNORE);
        props.pixelFormat = pixelFormat;
        size.width = LCD_WIDTH;
        size.height = LCD_HEIGHT;

        hr = render_target->CreateBitmap(size, props, &render_bitmap);
        assert(hr == S_OK);
    }
    #ifdef LCD_DIVISOR
    static constexpr const size_t lcd_divisor = LCD_DIVISOR;
#else
    static constexpr const size_t lcd_divisor = 10;
#endif
#ifdef LCD_BIT_DEPTH
    static constexpr const size_t lcd_pixel_size = (LCD_BIT_DEPTH + 7) / 8;
#else
    static constexpr const size_t lcd_pixel_size = 2;
#endif
    // the size of our transfer buffer
    static const constexpr size_t lcd_transfer_buffer_size = 2*
        LCD_WIDTH * LCD_HEIGHT * lcd_pixel_size / lcd_divisor;

    uint8_t* lcd_transfer_buffer1 =
        (uint8_t*)malloc(lcd_transfer_buffer_size);
    assert(lcd_transfer_buffer1 != nullptr) ;
    lcd.buffer_size(lcd_transfer_buffer_size);
    lcd.buffer1(lcd_transfer_buffer1);
    lcd.on_flush_callback([](const rect16& bounds, const void* bmp, void* state){
        flush_bitmap(bounds.x1,bounds.y1,bounds.width(),bounds.height(),bmp);
        lcd.flush_complete();
    });
    lcd.on_touch_callback([](point16* out_locations, size_t* in_out_locations_size, void* state){
        int x,y;
        if(!*in_out_locations_size) return;
        *in_out_locations_size=0;
        if(read_mouse(&x,&y)) {
            *in_out_locations_size=1;
            *out_locations = point16(x,y);
        }
        
    });
}

void display_update() {
    lcd.update();
    if (render_target && render_bitmap) {
        if (0==task_mutex_lock(app_mutex,-1)) {   // no time-out interval
            render_target->BeginDraw();
            D2D1_RECT_F rect_dest = {0, 0, (float)LCD_WIDTH, (float)LCD_HEIGHT};
            render_target->DrawBitmap(
                render_bitmap, rect_dest, 1.0f,
                D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, NULL);
            render_target->EndDraw();
            task_mutex_unlock(app_mutex);
        }
    }
    
}
void display_screen(void* screen) {
    lcd.active_screen(*(screen_base*)screen);
}

