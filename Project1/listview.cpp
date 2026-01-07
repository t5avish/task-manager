#include "listview.hpp"

void ListView::create(HWND parent, HINSTANCE instance, const RECT& rc)
{
    hwnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        WC_LISTVIEW,
        L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
        rc.left,
        rc.top,
        rc.right - rc.left,
        rc.bottom - rc.top,
        parent,
        nullptr,
        instance,
        nullptr
    );
}

void ListView::resize(const RECT& rc)
{
    MoveWindow(
        hwnd,
        rc.left,
        rc.top,
        rc.right - rc.left,
        rc.bottom - rc.top,
        TRUE
    );
}
