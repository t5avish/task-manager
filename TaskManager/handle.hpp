#pragma once

#include <windows.h>

struct handle
{
    HANDLE value;

    explicit handle(HANDLE h) : value(h) {}

    ~handle() {
        if (value && value != INVALID_HANDLE_VALUE) {
            CloseHandle(value);
        }
    }

    handle(const handle&) = delete;
    handle& operator=(const handle&) = delete;

    operator HANDLE() const {
        return value;
    }

    bool operator==(HANDLE h) const {
        return value == h;
    }
};
