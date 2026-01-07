#include <windows.h>
#include <tlhelp32.h>
#include <commctrl.h>
#include <algorithm>
#include <string>
#include "common.hpp"
#include "button.hpp"
#include "listview.hpp"
#pragma comment(lib, "Comctl32.lib")

using namespace common;
static ListView process_list;
static Button end_task_button;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static RECT calculate_listview_rect(int width, int height)
{
    constexpr int MARGIN = 10;
    return RECT{
        MARGIN,
        MARGIN,
        width - MARGIN,
        height - end_task_button.height - end_task_button.margin_from_corner * 2 - MARGIN
    };
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"Task Manager";

    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = CreateSolidBrush(RGB(240, 240, 240));

    RegisterClass(&wc);

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_LISTVIEW_CLASSES; // Initialize ListView control
    InitCommonControlsEx(&icex);

    // Create the window.
    HWND hwnd = CreateWindowEx(
        0,                              // Optional window styles.
        CLASS_NAME,                     // Window class
        L"Task Manager",    // Window text
        WS_OVERLAPPEDWINDOW,            // Window style

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 400,

        NULL,       // Parent window    
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
    );

    if (hwnd == NULL)
    {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_CREATE:
    {
        HINSTANCE instance = ((LPCREATESTRUCT)lParam)->hInstance;

        RECT rc;
        GetClientRect(hwnd, &rc);

        process_list.create(hwnd, instance, calculate_listview_rect(rc.right, rc.bottom));

        const std::wstring columns[] = { L"Name", L"PID", L"Status" };
        const int widths[] = { 200, 100, 100 };

        LVCOLUMN col = {};
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

        for (int i = 0; i < 3; i++) {
            col.pszText = const_cast<LPWSTR>(columns[i].c_str());
            col.cx = widths[i];
            ListView_InsertColumn(process_list.hwnd, i, &col);
        }

        insert_processes_into_grid(process_list.hwnd);

        end_task_button.create(hwnd, instance, L"End task", 0, 0);
        end_task_button.position_bottom_right(rc.right, rc.bottom);
        return 0;
    }

    case WM_SIZE:
    {
        int window_width = LOWORD(lParam);
        int window_height = HIWORD(lParam);

        end_task_button.position_bottom_right(window_width, window_height);
        process_list.resize(calculate_listview_rect(window_width, window_height));
        return 0;
    }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}