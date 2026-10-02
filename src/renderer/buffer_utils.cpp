#include "buffer_utils.h"
#include <cstring>

bool createBufferWithMemory(
    VkDevice device,
    const void* source,
    size_t size,
    VkBufferUsageFlags usage,
    VkBuffer* buffer,
    VkDeviceMemory* memory)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, buffer) != VK_SUCCESS)
        return false;

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device, *buffer, &requirements);

    VkMemoryAllocateInfo memoryInfo{};
    memoryInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryInfo.allocationSize = requirements.size;
    memoryInfo.memoryTypeIndex = 0;

    if (vkAllocateMemory(device, &memoryInfo, nullptr, memory) != VK_SUCCESS)
        return false;

    if (vkBindBufferMemory(device, *buffer, *memory, 0) != VK_SUCCESS)
        return false;

    void* mapped = nullptr;
    if (vkMapMemory(device, *memory, 0, size, 0, &mapped) != VK_SUCCESS)
        return false;

    if (source)
        std::memcpy(mapped, source, size);

    vkUnmapMemory(device, *memory);
    return true;
}
