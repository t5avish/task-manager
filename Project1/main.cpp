#include <windows.h>
#include <tlhelp32.h>
#include <commctrl.h>
#include <algorithm>
#include <string>
#include "common.hpp"
#include "button.hpp"
#include "listview.hpp"
#pragma comment(lib, "Comctl32.lib")

#define ID_END_TASK 1001
#define ID_REFRESH_TIMER 2001
#define ID_MENU_REFRESH_AUTO_2   3001
#define ID_MENU_REFRESH_AUTO_5   3002
#define ID_MENU_REFRESH_AUTO_10  3003
#define ID_MENU_REFRESH_MANUAL   3004
#define ID_REFRESH_NOW 3005

using namespace common;
static ListView process_list;
static Button end_task_button;
static DWORD selected_pid = 0;
static UINT refresh_rate = 2000;
static bool is_refreshing = false;
static HACCEL haccel = nullptr;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static RECT calculate_listview_rect(int width, int height)
{
    constexpr int MARGIN = 10;
    return RECT {
        MARGIN,
        MARGIN,
        width - MARGIN,
        height - end_task_button.height - end_task_button.margin_from_corner * 2 - MARGIN
    };
}

static void update_refresh_checkmarks(HWND hwnd, UINT active_id)
{
    HMENU menu_bar = GetMenu(hwnd);
    HMENU options = GetSubMenu(menu_bar, 0);
    HMENU refresh = GetSubMenu(options, 0);
    HMENU auto_menu = GetSubMenu(refresh, 0);

    if (active_id == 0) {
        CheckMenuItem(auto_menu, ID_MENU_REFRESH_AUTO_2, MF_BYCOMMAND | MF_UNCHECKED);
        CheckMenuItem(auto_menu, ID_MENU_REFRESH_AUTO_5, MF_BYCOMMAND | MF_UNCHECKED);
        CheckMenuItem(auto_menu, ID_MENU_REFRESH_AUTO_10, MF_BYCOMMAND | MF_UNCHECKED);
        return;
    }

    CheckMenuRadioItem(auto_menu, ID_MENU_REFRESH_AUTO_2, ID_MENU_REFRESH_AUTO_10, active_id, MF_BYCOMMAND);
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

    if (hwnd == NULL) {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    ACCEL accels[] = {
    { FVIRTKEY, VK_F5, ID_REFRESH_NOW }
    };
    haccel = CreateAcceleratorTable(accels, 1);

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        if (!TranslateAccelerator(hwnd, haccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    DestroyAcceleratorTable(haccel);

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
    {
        KillTimer(hwnd, ID_REFRESH_TIMER);
        PostQuitMessage(0);
        return 0;
    }

    case WM_CREATE:
    {
        HINSTANCE instance = ((LPCREATESTRUCT)lParam)->hInstance;

        RECT rc;
        GetClientRect(hwnd, &rc);

        HMENU menu_bar = CreateMenu();
        HMENU options_menu = CreateMenu();
        HMENU refresh_menu = CreateMenu();
        HMENU auto_menu = CreateMenu();

        AppendMenu(auto_menu, MF_STRING, ID_MENU_REFRESH_AUTO_2, L"2 seconds");
        AppendMenu(auto_menu, MF_STRING, ID_MENU_REFRESH_AUTO_5, L"5 seconds");
        AppendMenu(auto_menu, MF_STRING, ID_MENU_REFRESH_AUTO_10, L"10 seconds");

        AppendMenu(refresh_menu, MF_POPUP, (UINT_PTR)auto_menu, L"Auto");
        AppendMenu(refresh_menu, MF_STRING, ID_MENU_REFRESH_MANUAL, L"Manual (F5)");
        AppendMenu(options_menu, MF_POPUP, (UINT_PTR)refresh_menu, L"Refresh");

        AppendMenu(menu_bar, MF_POPUP, (UINT_PTR)options_menu, L"Options");

        SetMenu(hwnd, menu_bar);

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

        is_refreshing = true;
        insert_processes_into_grid(process_list.hwnd);
        is_refreshing = false;

        end_task_button.create(hwnd, instance, L"End task", 0, 0, ID_END_TASK);
        end_task_button.position_bottom_right(rc.right, rc.bottom);

        EnableWindow(end_task_button.hwnd, FALSE);

        SetTimer(hwnd, ID_REFRESH_TIMER, 2000, NULL);
        update_refresh_checkmarks(hwnd, ID_MENU_REFRESH_AUTO_2);
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

    case WM_NOTIFY:
    {
        if (is_refreshing) break;

        auto* hdr = reinterpret_cast<LPNMHDR>(lParam);

        if (hdr->hwndFrom != process_list.hwnd || hdr->code != LVN_ITEMCHANGED) {
            break;
        }

        auto* lv = reinterpret_cast<LPNMLISTVIEW>(lParam);

        if (!(lv->uChanged & LVIF_STATE)) {
            break;
        }

        const bool is_selected = (lv->uNewState & LVIS_SELECTED) && !(lv->uOldState & LVIS_SELECTED);

        const bool is_deselected = !(lv->uNewState & LVIS_SELECTED) && (lv->uOldState & LVIS_SELECTED);

        if (is_selected) {
            LVITEM item{};
            item.mask = LVIF_PARAM;
            item.iItem = lv->iItem;

            ListView_GetItem(process_list.hwnd, &item);

            selected_pid = static_cast<DWORD>(item.lParam);
            EnableWindow(end_task_button.hwnd, TRUE);

        } else if (is_deselected) {
            selected_pid = 0;
            EnableWindow(end_task_button.hwnd, FALSE);
        }

        return 0;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {

        // END TASK BUTTON
        case ID_END_TASK:
        {
            if (HIWORD(wParam) == BN_CLICKED && selected_pid)
            {
                if (!soft_kill_process_by_pid(selected_pid))
                {
                    hard_kill_process_by_pid(selected_pid);
                }

                WaitForSingleObject(OpenProcess(SYNCHRONIZE, FALSE, selected_pid), 2000);

                is_refreshing = true;
                refresh_processes_in_grid(process_list.hwnd);
                is_refreshing = false;

                EnableWindow(end_task_button.hwnd, FALSE);
                selected_pid = 0;
            }
            return 0;
        }

        // MENU
        case ID_MENU_REFRESH_AUTO_2:
        {
            refresh_rate = 2000;
            SetTimer(hwnd, ID_REFRESH_TIMER, refresh_rate, NULL);
            update_refresh_checkmarks(hwnd, ID_MENU_REFRESH_AUTO_2);
            return 0;
        }

        case ID_MENU_REFRESH_AUTO_5:
        {
            refresh_rate = 5000;
            SetTimer(hwnd, ID_REFRESH_TIMER, refresh_rate, NULL);
            update_refresh_checkmarks(hwnd, ID_MENU_REFRESH_AUTO_5);
            return 0;
        }

        case ID_MENU_REFRESH_AUTO_10:
        {
            refresh_rate = 10000;
            SetTimer(hwnd, ID_REFRESH_TIMER, refresh_rate, NULL);
            update_refresh_checkmarks(hwnd, ID_MENU_REFRESH_AUTO_10);
            return 0;
        }

        // MANUAL REFRESH
        case ID_MENU_REFRESH_MANUAL:
        {
            KillTimer(hwnd, ID_REFRESH_TIMER);
            update_refresh_checkmarks(hwnd, 0);
            return 0;
        }

        case ID_REFRESH_NOW:
        {
            is_refreshing = true;
            refresh_processes_in_grid(process_list.hwnd);
            is_refreshing = false;
            return 0;
        }
        }

        break;
    }

    case WM_TIMER:
    {
        if (wParam == ID_REFRESH_TIMER)
        {
            is_refreshing = true;
            refresh_processes_in_grid(process_list.hwnd);
            is_refreshing = false;
        }
        return 0;
    }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}