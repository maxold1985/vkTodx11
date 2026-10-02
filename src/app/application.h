#pragma once
#include "../renderer/renderer.h"
#include <windows.h>

class Application
{
public:
    bool initialize(HINSTANCE instance, int showCommand);
    int run();
    void shutdown();

private:
    static LRESULT CALLBACK windowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    Renderer renderer_;
};
