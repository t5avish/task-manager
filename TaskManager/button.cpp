#include "button.hpp"

void Button::create(HWND parent, HINSTANCE instance, const std::wstring &label, int x, int y, int id) {
    hwnd = CreateWindow(
        L"BUTTON",
        label.c_str(),
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        x, y, width, height,
        parent,
        (HMENU)id,
        instance,
        NULL
    );
}

void Button::position_bottom_right(int window_width, int window_height) {
    int x = window_width - width - margin_from_corner;
    int y = window_height - height - margin_from_corner;

    MoveWindow(hwnd, x, y, width, height, TRUE);
}
