#pragma once
#include <Windows.h>
#include <string>

class Button
{
public:
    HWND hwnd = nullptr;
    int width = 100;
    int height = 30;
    int margin_from_corner = 10;

    void create(HWND parent, HINSTANCE instance, const std::wstring &label, int x, int y, int id);
    void position_bottom_right(int window_width, int window_height);
};
