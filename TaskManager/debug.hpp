#pragma once

#include <format>

#define dbg(...) OutputDebugStringW(std::format(L"[TaskManager]: " __VA_ARGS__).c_str())
