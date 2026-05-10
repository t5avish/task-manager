#pragma once
#include <Windows.h>
#include <commctrl.h>

class ListView
{
public:
    HWND hwnd = nullptr;

    void create(HWND parent, HINSTANCE instance, const RECT &rc);
    void resize(const RECT &rc);
};
