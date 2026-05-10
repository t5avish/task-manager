#pragma once
#include "scope.hpp"
#include <format>
#define dbg(...) OutputDebugStringW(std::format(L"[TaskManager]: " __VA_ARGS__).c_str())

namespace utility {

    using scope::scope_exit;

    inline void exception_guard(auto f)
    {
        try {
            f();
        }
        catch (const std::exception& e) {
            std::wstring msg(e.what(), e.what() + std::strlen(e.what()));
            dbg(L"exception: {}", msg);
        }
    }
}