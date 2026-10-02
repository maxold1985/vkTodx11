#include "app/application.h"
#include <windows.h>

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int showCommand)
{
    Application app;

    if (!app.initialize(instance, showCommand))
    {
        MessageBoxW(
            nullptr,
            L"Falha ao inicializar a aplicacao. Veja vk_pipeline.log se o pipeline falhou.",
            L"Vulkan -> D3D11",
            MB_OK | MB_ICONERROR);
        return 1;
    }

    return app.run();
}
