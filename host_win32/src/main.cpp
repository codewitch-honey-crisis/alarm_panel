#include <stdint.h>
#include <stddef.h>
#include "display.h"
#include "task.h"
// uix_test.cpp : Defines the entry point for the application.
//
#pragma comment(lib, "d2d1.lib")
#include "d2d1.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <assert.h>
extern void run();
extern void loop();
extern "C" int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                                 _In_opt_ HINSTANCE hPrevInstance,
                                 _In_ LPWSTR lpCmdLine, _In_ int nCmdShow);

// Global Variables:
HINSTANCE hInst;  // current instance
HANDLE quit_event = NULL;
extern HWND hWndMain; 
HWND hWndMain;
extern task_mutex_t app_mutex;
task_mutex_t app_mutex = NULL;
static HANDLE app_thread = NULL;
static bool should_quit = false;

// this handles our main application loop
// plus rendering
static DWORD CALLBACK render_thread_proc(void* state) {
    
    bool quit = false;
    while (!quit) {
        
        if (WAIT_OBJECT_0 == WaitForSingleObject(quit_event, 0)) {
            quit = true;
        }
    }
    return 0;
}

// Forward declarations of functions included in this code module:

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
    hInst = hInstance;  // Store instance handle in our global variable
    // for signalling when to exit
    quit_event = CreateEvent(NULL,              // default security attributes
                             TRUE,              // manual-reset event
                             FALSE,             // initial state is nonsignaled
                             TEXT("QuitEvent")  // object name
    );
    // for handling our render
    app_mutex = task_mutex_init();
    assert(app_mutex != NULL);

    RECT r = {0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1};
    // adjust the size of the window so
    // the above is our client rect
    AdjustWindowRectEx(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE,
                       WS_EX_APPWINDOW);
    hWndMain = CreateWindowW(
        L"WIN32_HOST", L"Win32 Host", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left ,
        r.bottom - r.top , nullptr, nullptr, hInstance, nullptr);

    
    ShowWindow(hWndMain, nCmdShow);
    UpdateWindow(hWndMain);
    return TRUE;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine,
                      _In_ int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    CoInitialize(NULL);
    // Initialize global strings
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = NULL;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = L"WIN32_HOST";
    wcex.hIconSm = NULL;

    if(0==RegisterClassExW(&wcex)) {
        DWORD err = GetLastError();
        assert(false);
    }


    // Perform application initialization:
    if (!InitInstance(hInstance, nCmdShow)) {
        return FALSE;
    }
    MSG msg = {0};
    run();
    while (!should_quit) {
        DWORD result = 0;
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                should_quit = true;
                break;
            }
           
            
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        loop();
        if (WAIT_OBJECT_0 == WaitForSingleObject(quit_event, 0)) {
            should_quit = true;
        }
    }
    CoUninitialize();

    return (int)msg.wParam;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam,
                         LPARAM lParam) {
    switch (message) {
        case WM_DESTROY:
            SetEvent(quit_event);
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
