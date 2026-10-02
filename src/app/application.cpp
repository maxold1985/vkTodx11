#include "application.h"

LRESULT CALLBACK Application::windowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_KEYDOWN && wParam == VK_ESCAPE)
    {
        DestroyWindow(hwnd);
        return 0;
    }

    if (message == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool Application::initialize(HINSTANCE instance, int showCommand)
{
    instance_ = instance;

    WNDCLASSW wc{};
    wc.lpfnWndProc = windowProcedure;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"VulkanD3D11Modular";

    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;

    hwnd_ = CreateWindowW(
        wc.lpszClassName,
        L"Vulkan 1.0 -> Direct3D 11 - Modular",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        960, 540,
        nullptr, nullptr, instance, nullptr);

    if (!hwnd_)
        return false;

    ShowWindow(hwnd_, showCommand);
    UpdateWindow(hwnd_);
    return renderer_.initialize(hwnd_, 960, 540);
}

int Application::run()
{
    MSG message{};
    bool running = true;

    while (running)
    {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                running = false;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        if (running)
            renderer_.render();
    }

    shutdown();
    return 0;
}

void Application::shutdown()
{
    renderer_.shutdown();
    hwnd_ = nullptr;
}
