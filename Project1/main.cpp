#include <iostream>
#include <windows.h>
#include <tlhelp32.h>
#include <commctrl.h>
#include <algorithm>
#include <string>
#include "common.hpp"
#include "button.hpp"
#pragma comment(lib, "Comctl32.lib")

using namespace common;
static HWND listView_hwnd;
static Button end_task_button;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

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
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

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

    RECT rc;
    GetClientRect(hwnd, &rc);

    listView_hwnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        WC_LISTVIEW,
        L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT,
        10, 10, rc.right - 20, rc.bottom - end_task_button.height - end_task_button.margin_from_corner * 2 - 10,
        hwnd,
        NULL,
        hInstance,
        NULL
    );

    LVCOLUMN list_view_cols = {};
    list_view_cols.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    std::wstring col1 = L"Name";
    std::wstring col2 = L"PID";
    std::wstring col3 = L"Status";

    LVCOLUMN col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    col.pszText = col1.data();
    col.cx = 200;
    ListView_InsertColumn(listView_hwnd, 0, &col);

    col.pszText = col2.data();
    col.cx = 100;
    ListView_InsertColumn(listView_hwnd, 1, &col);

    col.pszText = col3.data();
    col.cx = 100;
    ListView_InsertColumn(listView_hwnd, 2, &col);

    insert_processes_into_grid(listView_hwnd);

    MoveWindow(end_task_button.hwnd,
        rc.right - end_task_button.width - end_task_button.margin_from_corner,
        rc.bottom - end_task_button.height - end_task_button.margin_from_corner,
        end_task_button.width, end_task_button.height,
        TRUE);

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
        int bottom_right_x = rc.right - end_task_button.width - end_task_button.margin_from_corner;
        int bottom_right_y = rc.bottom - end_task_button.height - end_task_button.margin_from_corner;
        end_task_button.create(hwnd, instance, L"End task", bottom_right_x, bottom_right_y);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        SetBkMode(hdc, TRANSPARENT);
        return 0;
    }

    case WM_SIZE:
    {
        int window_width = LOWORD(lParam);
        int window_height = HIWORD(lParam);

        end_task_button.position_bottom_right(window_width, window_height);

        MoveWindow(
            listView_hwnd,
            10, 10,
            window_width - 20,
            window_height - end_task_button.height - end_task_button.margin_from_corner * 2 - 10,
            TRUE
        );
        return 0;
    }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}