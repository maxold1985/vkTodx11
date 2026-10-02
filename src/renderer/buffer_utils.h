#pragma once
#include "../vk_d3d11.h"
#include <cstddef>

bool createBufferWithMemory(
    VkDevice device,
    const void* source,
    size_t size,
    VkBufferUsageFlags usage,
    VkBuffer* buffer,
    VkDeviceMemory* memory);
