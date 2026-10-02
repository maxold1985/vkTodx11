#pragma once

#include <vulkan/vulkan.h>
#include <windows.h>

#ifdef __cplusplus
extern "C"
{
#endif

VKAPI_ATTR VkResult VKAPI_CALL vkdx11CreateWindowSurface(
    VkDevice device,
    HWND hwnd,
    uint32_t width,
    uint32_t height
);

VKAPI_ATTR void VKAPI_CALL vkdx11BeginFrame(
    VkDevice device,
    float r,
    float g,
    float b,
    float a
);

VKAPI_ATTR void VKAPI_CALL vkdx11Present(
    VkDevice device
);

#ifdef __cplusplus
}
#endif
