#include "vk_d3d11.h"

#include <d3d11.h>
#include <spirv_hlsl.hpp>
#include <cstdio>
#include <dxgi.h>

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <string>
#include <new>
#include <type_traits>
#include <vector>


template <typename T>
static void releaseCom(T*& object)
{
    if (object)
    {
        object->Release();
        object = nullptr;
    }
}


template <typename VkHandle, typename Object>
static VkHandle toHandle(Object* object)
{
    if constexpr (std::is_pointer_v<VkHandle>)
    {
        return reinterpret_cast<VkHandle>(object);
    }
    else
    {
        return static_cast<VkHandle>(
            reinterpret_cast<uintptr_t>(object)
        );
    }
}


template <typename Object, typename VkHandle>
static Object* fromHandle(VkHandle handle)
{
    if constexpr (std::is_pointer_v<VkHandle>)
    {
        return reinterpret_cast<Object*>(handle);
    }
    else
    {
        return reinterpret_cast<Object*>(
            static_cast<uintptr_t>(handle)
        );
    }
}


struct DxInstance
{
};


struct DxPhysicalDevice
{
};


struct DxQueue;


struct DxDevice
{
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;

    IDXGISwapChain* swapChain = nullptr;
    ID3D11RenderTargetView* renderTarget = nullptr;

    DxQueue* queue = nullptr;

    uint32_t width = 1;
    uint32_t height = 1;
};


struct DxQueue
{
    DxDevice* device = nullptr;
};


struct DxMemory
{
    std::vector<uint8_t> data;
};


struct DxBuffer
{
    DxDevice* device = nullptr;

    ID3D11Buffer* buffer = nullptr;

    VkDeviceSize size = 0;
    VkBufferUsageFlags usage = 0;

    DxMemory* memory = nullptr;
};


struct DxShaderModule
{
    std::vector<uint32_t> code;
};


struct DxDescriptorSetLayoutBinding
{
    uint32_t binding = 0;
    VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    VkShaderStageFlags stageFlags = 0;
};

struct DxDescriptorSetLayout
{
    std::vector<DxDescriptorSetLayoutBinding> bindings;
};

struct DxDescriptorPool
{
    DxDevice* device = nullptr;
};

struct DxDescriptorSet
{
    DxDevice* device = nullptr;
    DxDescriptorSetLayout* layout = nullptr;
    DxBuffer* uniformBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]{};
    VkDeviceSize uniformOffsets[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]{};
    VkDeviceSize uniformRanges[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]{};
};

struct DxPipelineLayout
{
    std::vector<DxDescriptorSetLayout*> setLayouts;
};


struct DxPipeline
{
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11InputLayout* inputLayout = nullptr;

    UINT vertexStride =
        sizeof(float) * 6;
};


struct DxCommandPool
{
    DxDevice* device = nullptr;
};


struct DxCommandBuffer
{
    DxDevice* device = nullptr;

    DxPipeline* pipeline = nullptr;

    DxBuffer* vertexBuffer = nullptr;
    DxBuffer* indexBuffer = nullptr;

    DxPipelineLayout* pipelineLayout = nullptr;
    DxDescriptorSet* descriptorSets[4]{};

    VkDeviceSize vertexBufferOffset = 0;
    VkDeviceSize indexBufferOffset = 0;

    VkIndexType indexType =
        VK_INDEX_TYPE_UINT16;

    uint32_t indexCount = 0;
    uint32_t firstIndex = 0;

    int32_t vertexOffset = 0;
};


static DxPhysicalDevice gPhysicalDevice;


static bool readBinaryFile(
    const std::string& path,
    std::vector<uint8_t>& data)
{
    std::ifstream file(
        path,
        std::ios::binary |
        std::ios::ate
    );

    if (!file)
    {
        return false;
    }

    std::streamsize size =
        file.tellg();

    if (size <= 0)
    {
        return false;
    }

    file.seekg(
        0,
        std::ios::beg
    );

    data.resize(
        static_cast<size_t>(size)
    );

    return static_cast<bool>(
        file.read(
            reinterpret_cast<char*>(
                data.data()
            ),
            size
        )
    );
}


static std::string quoteArgument(
    const std::string& value)
{
    return std::string("\"") +
        value +
        "\"";
}


static bool runFxc(
    const std::string& hlsl,
    const char* target,
    std::vector<uint8_t>& dxbc)
{
    char tempPath[MAX_PATH]{};

    if (!GetTempPathA(
        MAX_PATH,
        tempPath))
    {
        return false;
    }

    char hlslPath[MAX_PATH]{};

    if (!GetTempFileNameA(
        tempPath,
        "vkh",
        0,
        hlslPath))
    {
        return false;
    }

    std::string dxbcPath =
        std::string(hlslPath) +
        ".dxbc";

    {
        std::ofstream file(
            hlslPath,
            std::ios::binary
        );

        if (!file)
        {
            DeleteFileA(
                hlslPath
            );

            return false;
        }

        file.write(
            hlsl.data(),
            static_cast<std::streamsize>(
                hlsl.size()
            )
        );
    }

    char fxcPath[MAX_PATH]{};

    DWORD fxcLength =
        GetEnvironmentVariableA(
            "FXC",
            fxcPath,
            MAX_PATH
        );

    std::string fxc;

    if (fxcLength > 0 &&
        fxcLength < MAX_PATH)
    {
        fxc = fxcPath;
    }
    else
    {
        char exePath[MAX_PATH]{};
        DWORD exeLength = GetModuleFileNameA(nullptr, exePath, MAX_PATH);

        if (exeLength > 0 && exeLength < MAX_PATH)
        {
            std::string fullExePath(exePath);
            size_t slash = fullExePath.find_last_of("\\/");

            if (slash != std::string::npos)
            {
                std::string besideExe =
                    fullExePath.substr(0, slash + 1) + "fxc.exe";

                if (GetFileAttributesA(besideExe.c_str()) != INVALID_FILE_ATTRIBUTES)
                {
                    fxc = besideExe;
                }
            }
        }

        if (fxc.empty())
        {
            fxc = "fxc.exe";
        }
    }

    std::string command =
        quoteArgument(fxc) +
        " /nologo /O3 /E main /T " +
        target +
        " /Fo " +
        quoteArgument(dxbcPath) +
        " " +
        quoteArgument(hlslPath);

    STARTUPINFOA startupInfo{};
    startupInfo.cb =
        sizeof(startupInfo);

    PROCESS_INFORMATION processInfo{};

    std::vector<char> commandLine(
        command.begin(),
        command.end()
    );

    commandLine.push_back('\0');

    BOOL created =
        CreateProcessA(
            nullptr,
            commandLine.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &startupInfo,
            &processInfo
        );

    if (!created)
    {
        DWORD error = GetLastError();
        char message[1024]{};
        std::snprintf(
            message,
            sizeof(message),
            "Nao foi possivel iniciar FXC.\n\nFXC: %s\nErro Win32: %lu\n\nDefina FXC ou coloque fxc.exe ao lado do vkd3d11.exe.",
            fxc.c_str(),
            static_cast<unsigned long>(error)
        );
        MessageBoxA(nullptr, message, "Vulkan -> D3D11 / FXC", MB_OK | MB_ICONERROR);

        DeleteFileA(hlslPath);
        return false;
    }

    WaitForSingleObject(
        processInfo.hProcess,
        INFINITE
    );

    DWORD exitCode = 1;

    GetExitCodeProcess(
        processInfo.hProcess,
        &exitCode
    );

    CloseHandle(
        processInfo.hThread
    );

    CloseHandle(
        processInfo.hProcess
    );

    bool ok =
        exitCode == 0 &&
        readBinaryFile(
            dxbcPath,
            dxbc
        );

    if (!ok)
    {
        char message[1400]{};
        std::snprintf(
            message,
            sizeof(message),
            "FXC falhou.\n\nExit code: %lu\nFXC: %s\nHLSL temporario: %s\nTarget: %s\n\nAbra um CMD e execute o FXC manualmente nesse HLSL para ver o erro do shader.",
            static_cast<unsigned long>(exitCode),
            fxc.c_str(),
            hlslPath,
            target
        );
        MessageBoxA(nullptr, message, "Vulkan -> D3D11 / FXC", MB_OK | MB_ICONERROR);

        // Mantem o HLSL quando ha erro para diagnostico.
        DeleteFileA(dxbcPath.c_str());
        return false;
    }

    DeleteFileA(hlslPath);
    DeleteFileA(dxbcPath.c_str());
    return true;
}


static spv::ExecutionModel executionModelForStage(
    VkShaderStageFlagBits stage)
{
    switch (stage)
    {
        case VK_SHADER_STAGE_VERTEX_BIT:
            return spv::ExecutionModelVertex;

        case VK_SHADER_STAGE_FRAGMENT_BIT:
            return spv::ExecutionModelFragment;

        default:
            return spv::ExecutionModelMax;
    }
}


static bool spirvToDxbc(
    const DxShaderModule* module,
    VkShaderStageFlagBits stage,
    const char* entryPoint,
    std::vector<uint8_t>& dxbc)
{
    if (!module ||
        module->code.empty())
    {
        return false;
    }

    try
    {
        spirv_cross::CompilerHLSL compiler(
            module->code
        );

        spirv_cross::CompilerHLSL::Options options{};
        options.shader_model = 50;
        options.point_size_compat = false;
        options.point_coord_compat = false;
        options.support_nonzero_base_vertex_base_instance = false;

        compiler.set_hlsl_options(
            options
        );

        spv::ExecutionModel model =
            executionModelForStage(
                stage
            );

        if (model ==
            spv::ExecutionModelMax)
        {
            return false;
        }

        if (entryPoint &&
            std::strcmp(
                entryPoint,
                "main"
            ) != 0)
        {
            compiler.rename_entry_point(
                entryPoint,
                "main",
                model
            );
        }

        std::string hlsl =
            compiler.compile();

        const char* target =
            stage ==
                VK_SHADER_STAGE_VERTEX_BIT
                ? "vs_5_0"
                : "ps_5_0";

        return runFxc(
            hlsl,
            target,
            dxbc
        );
    }
    catch (const std::exception& error)
    {
        OutputDebugStringA(
            error.what()
        );

        OutputDebugStringA(
            "\\n"
        );

        return false;
    }
}


static DXGI_FORMAT vkFormatToDxgi(
    VkFormat format)
{
    switch (format)
    {
        case VK_FORMAT_R32_SFLOAT:
            return DXGI_FORMAT_R32_FLOAT;

        case VK_FORMAT_R32G32_SFLOAT:
            return DXGI_FORMAT_R32G32_FLOAT;

        case VK_FORMAT_R32G32B32_SFLOAT:
            return DXGI_FORMAT_R32G32B32_FLOAT;

        case VK_FORMAT_R32G32B32A32_SFLOAT:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;

        case VK_FORMAT_R8G8B8A8_UNORM:
            return DXGI_FORMAT_R8G8B8A8_UNORM;

        default:
            return DXGI_FORMAT_UNKNOWN;
    }
}


static void uploadBuffer(
    DxBuffer* buffer)
{
    if (!buffer ||
        !buffer->buffer ||
        !buffer->memory)
    {
        return;
    }

    buffer->device->context->UpdateSubresource(
        buffer->buffer,
        0,
        nullptr,
        buffer->memory->data.data(),
        0,
        0
    );
}


extern "C"
{


VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(
    const VkInstanceCreateInfo* createInfo,
    const VkAllocationCallbacks* allocator,
    VkInstance* instance)
{
    (void)createInfo;
    (void)allocator;

    if (!instance)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    DxInstance* object =
        new (std::nothrow) DxInstance{};

    if (!object)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    *instance =
        toHandle<VkInstance>(
            object
        );

    return VK_SUCCESS;
}


VKAPI_ATTR void VKAPI_CALL vkDestroyInstance(
    VkInstance instance,
    const VkAllocationCallbacks* allocator)
{
    (void)allocator;

    DxInstance* object =
        fromHandle<DxInstance>(
            instance
        );

    delete object;
}


VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDevices(
    VkInstance instance,
    uint32_t* count,
    VkPhysicalDevice* devices)
{
    (void)instance;

    if (!count)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (!devices)
    {
        *count = 1;

        return VK_SUCCESS;
    }

    if (*count == 0)
    {
        return VK_INCOMPLETE;
    }

    devices[0] =
        toHandle<VkPhysicalDevice>(
            &gPhysicalDevice
        );

    *count = 1;

    return VK_SUCCESS;
}


VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties(
    VkPhysicalDevice physicalDevice,
    uint32_t* count,
    VkQueueFamilyProperties* properties)
{
    (void)physicalDevice;

    if (!count)
    {
        return;
    }

    if (!properties)
    {
        *count = 1;

        return;
    }

    if (*count == 0)
    {
        return;
    }

    properties[0] = {};

    properties[0].queueFlags =
        VK_QUEUE_GRAPHICS_BIT;

    properties[0].queueCount = 1;

    properties[0].minImageTransferGranularity =
    {
        1,
        1,
        1
    };

    *count = 1;
}


VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceMemoryProperties* properties)
{
    (void)physicalDevice;

    if (!properties)
    {
        return;
    }

    *properties = {};

    properties->memoryTypeCount = 1;

    properties->memoryTypes[0].propertyFlags =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    properties->memoryTypes[0].heapIndex = 0;

    properties->memoryHeapCount = 1;

    properties->memoryHeaps[0].size =
        1024ull *
        1024ull *
        1024ull;

    properties->memoryHeaps[0].flags =
        VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
}


VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(
    VkPhysicalDevice physicalDevice,
    const VkDeviceCreateInfo* createInfo,
    const VkAllocationCallbacks* allocator,
    VkDevice* device)
{
    (void)physicalDevice;
    (void)createInfo;
    (void)allocator;

    if (!device)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    DxDevice* dxDevice =
        new (std::nothrow) DxDevice{};

    if (!dxDevice)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    D3D_FEATURE_LEVEL requestedLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    D3D_FEATURE_LEVEL selectedLevel{};

    HRESULT result =
        D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            requestedLevels,
            ARRAYSIZE(requestedLevels),
            D3D11_SDK_VERSION,
            &dxDevice->device,
            &selectedLevel,
            &dxDevice->context
        );

    if (FAILED(result))
    {
        delete dxDevice;

        return VK_ERROR_INITIALIZATION_FAILED;
    }

    dxDevice->queue =
        new (std::nothrow) DxQueue{};

    if (!dxDevice->queue)
    {
        releaseCom(
            dxDevice->context
        );

        releaseCom(
            dxDevice->device
        );

        delete dxDevice;

        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    dxDevice->queue->device =
        dxDevice;

    *device =
        toHandle<VkDevice>(
            dxDevice
        );

    return VK_SUCCESS;
}


VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(
    VkDevice device,
    const VkAllocationCallbacks* allocator)
{
    (void)allocator;

    DxDevice* dxDevice =
        fromHandle<DxDevice>(
            device
        );

    if (!dxDevice)
    {
        return;
    }

    if (dxDevice->context)
    {
        dxDevice->context->ClearState();
    }

    releaseCom(
        dxDevice->renderTarget
    );

    releaseCom(
        dxDevice->swapChain
    );

    releaseCom(
        dxDevice->context
    );

    releaseCom(
        dxDevice->device
    );

    delete dxDevice->queue;
    delete dxDevice;
}


VKAPI_ATTR void VKAPI_CALL vkGetDeviceQueue(
    VkDevice device,
    uint32_t queueFamilyIndex,
    uint32_t queueIndex,
    VkQueue* queue)
{
    (void)queueFamilyIndex;
    (void)queueIndex;

    if (!queue)
    {
        return;
    }

    DxDevice* dxDevice =
        fromHandle<DxDevice>(
            device
        );

    if (!dxDevice)
    {
        *queue =
            VK_NULL_HANDLE;

        return;
    }

    *queue =
        toHandle<VkQueue>(
            dxDevice->queue
        );
}


VKAPI_ATTR VkResult VKAPI_CALL vkCreateBuffer(
    VkDevice device,
    const VkBufferCreateInfo* createInfo,
    const VkAllocationCallbacks* allocator,
    VkBuffer* buffer)
{
    (void)allocator;

    if (!device ||
        !createInfo ||
        !buffer ||
        createInfo->size == 0)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    DxDevice* dxDevice =
        fromHandle<DxDevice>(
            device
        );

    DxBuffer* dxBuffer =
        new (std::nothrow) DxBuffer{};

    if (!dxBuffer)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    dxBuffer->device =
        dxDevice;

    dxBuffer->size =
        createInfo->size;

    dxBuffer->usage =
        createInfo->usage;

    D3D11_BUFFER_DESC description{};

    description.ByteWidth =
        static_cast<UINT>(
            (createInfo->size + 15) &
            ~VkDeviceSize(15)
        );

    description.Usage =
        D3D11_USAGE_DEFAULT;

    if (createInfo->usage &
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
    {
        description.BindFlags |=
            D3D11_BIND_VERTEX_BUFFER;
    }

    if (createInfo->usage &
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
    {
        description.BindFlags |=
            D3D11_BIND_INDEX_BUFFER;
    }

    if (createInfo->usage &
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
    {
        description.BindFlags |=
            D3D11_BIND_CONSTANT_BUFFER;
    }

    HRESULT result =
        dxDevice->device->CreateBuffer(
            &description,
            nullptr,
            &dxBuffer->buffer
        );

    if (FAILED(result))
    {
        delete dxBuffer;

        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    *buffer =
        toHandle<VkBuffer>(
            dxBuffer
        );

    return VK_SUCCESS;
}


VKAPI_ATTR void VKAPI_CALL vkDestroyBuffer(
    VkDevice device,
    VkBuffer buffer,
    const VkAllocationCallbacks* allocator)
{
    (void)device;
    (void)allocator;

    DxBuffer* dxBuffer =
        fromHandle<DxBuffer>(
            buffer
        );

    if (!dxBuffer)
    {
        return;
    }

    releaseCom(
        dxBuffer->buffer
    );

    delete dxBuffer;
}


VKAPI_ATTR void VKAPI_CALL vkGetBufferMemoryRequirements(
    VkDevice device,
    VkBuffer buffer,
    VkMemoryRequirements* requirements)
{
    (void)device;

    if (!requirements)
    {
        return;
    }

    DxBuffer* dxBuffer =
        fromHandle<DxBuffer>(
            buffer
        );

    if (!dxBuffer)
    {
        *requirements = {};

        return;
    }

    requirements->size =
        (dxBuffer->size + 15) &
        ~VkDeviceSize(15);

    requirements->alignment = 16;
    requirements->memoryTypeBits = 1;
}


VKAPI_ATTR VkResult VKAPI_CALL vkAllocateMemory(
    VkDevice device,
    const VkMemoryAllocateInfo* allocateInfo,
    const VkAllocationCallbacks* allocator,
    VkDeviceMemory* memory)
{
    (void)device;
    (void)allocator;

    if (!allocateInfo ||
        !memory)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    DxMemory* dxMemory =
        new (std::nothrow) DxMemory{};

    if (!dxMemory)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    try
    {
        dxMemory->data.resize(
            static_cast<size_t>(
                allocateInfo->allocationSize
            )
        );
    }
    catch (...)
    {
        delete dxMemory;

        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    *memory =
        toHandle<VkDeviceMemory>(
            dxMemory
        );

    return VK_SUCCESS;
}


VKAPI_ATTR void VKAPI_CALL vkFreeMemory(
    VkDevice device,
    VkDeviceMemory memory,
    const VkAllocationCallbacks* allocator)
{
    (void)device;
    (void)allocator;

    DxMemory* dxMemory =
        fromHandle<DxMemory>(
            memory
        );

    delete dxMemory;
}


VKAPI_ATTR VkResult VKAPI_CALL vkBindBufferMemory(
    VkDevice device,
    VkBuffer buffer,
    VkDeviceMemory memory,
    VkDeviceSize memoryOffset)
{
    (void)device;

    DxBuffer* dxBuffer =
        fromHandle<DxBuffer>(
            buffer
        );

    DxMemory* dxMemory =
        fromHandle<DxMemory>(
            memory
        );

    if (!dxBuffer ||
        !dxMemory ||
        memoryOffset != 0)
    {
        return VK_ERROR_MEMORY_MAP_FAILED;
    }

    if (dxBuffer->size >
        dxMemory->data.size())
    {
        return VK_ERROR_MEMORY_MAP_FAILED;
    }

    dxBuffer->memory =
        dxMemory;

    return VK_SUCCESS;
}


VKAPI_ATTR VkResult VKAPI_CALL vkMapMemory(
    VkDevice device,
    VkDeviceMemory memory,
    VkDeviceSize offset,
    VkDeviceSize size,
    VkMemoryMapFlags flags,
    void** data)
{
    (void)device;
    (void)flags;

    DxMemory* dxMemory =
        fromHandle<DxMemory>(
            memory
        );

    if (!dxMemory ||
        !data)
    {
        return VK_ERROR_MEMORY_MAP_FAILED;
    }

    if (offset >
        dxMemory->data.size())
    {
        return VK_ERROR_MEMORY_MAP_FAILED;
    }

    VkDeviceSize mappedSize =
        size;

    if (size ==
        VK_WHOLE_SIZE)
    {
        mappedSize =
            dxMemory->data.size() -
            offset;
    }

    if (offset + mappedSize >
        dxMemory->data.size())
    {
        return VK_ERROR_MEMORY_MAP_FAILED;
    }

    *data =
        dxMemory->data.data() +
        static_cast<size_t>(
            offset
        );

    return VK_SUCCESS;
}


VKAPI_ATTR void VKAPI_CALL vkUnmapMemory(
    VkDevice device,
    VkDeviceMemory memory)
{
    (void)device;
    (void)memory;
}


VKAPI_ATTR VkResult VKAPI_CALL vkCreateShaderModule(
    VkDevice device,
    const VkShaderModuleCreateInfo* createInfo,
    const VkAllocationCallbacks* allocator,
    VkShaderModule* shaderModule)
{
    (void)device;
    (void)allocator;

    if (!createInfo ||
        !shaderModule ||
        !createInfo->pCode ||
        createInfo->codeSize == 0)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    DxShaderModule* module =
        new (std::nothrow)
            DxShaderModule{};

    if (!module)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    size_t wordCount =
        createInfo->codeSize /
        sizeof(uint32_t);

    module->code.assign(
        createInfo->pCode,
        createInfo->pCode +
        wordCount
    );

    *shaderModule =
        toHandle<VkShaderModule>(
            module
        );

    return VK_SUCCESS;
}


VKAPI_ATTR void VKAPI_CALL vkDestroyShaderModule(
    VkDevice device,
    VkShaderModule shaderModule,
    const VkAllocationCallbacks* allocator)
{
    (void)device;
    (void)allocator;

    DxShaderModule* module =
        fromHandle<DxShaderModule>(
            shaderModule
        );

    delete module;
}


VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorSetLayout(
    VkDevice device,
    const VkDescriptorSetLayoutCreateInfo* createInfo,
    const VkAllocationCallbacks* allocator,
    VkDescriptorSetLayout* setLayout)
{
    (void)device;
    (void)allocator;
    if (!createInfo || !setLayout) return VK_ERROR_INITIALIZATION_FAILED;
    DxDescriptorSetLayout* layout = new (std::nothrow) DxDescriptorSetLayout{};
    if (!layout) return VK_ERROR_OUT_OF_HOST_MEMORY;
    for (uint32_t i = 0; i < createInfo->bindingCount; ++i)
    {
        const VkDescriptorSetLayoutBinding& b = createInfo->pBindings[i];
        if (b.descriptorCount != 1 || b.binding >= D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT)
        {
            delete layout;
            return VK_ERROR_FEATURE_NOT_PRESENT;
        }
        layout->bindings.push_back({b.binding, b.descriptorType, b.stageFlags});
    }
    *setLayout = toHandle<VkDescriptorSetLayout>(layout);
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorSetLayout(
    VkDevice device, VkDescriptorSetLayout setLayout, const VkAllocationCallbacks* allocator)
{
    (void)device; (void)allocator;
    delete fromHandle<DxDescriptorSetLayout>(setLayout);
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorPool(
    VkDevice device, const VkDescriptorPoolCreateInfo* createInfo,
    const VkAllocationCallbacks* allocator, VkDescriptorPool* descriptorPool)
{
    (void)createInfo; (void)allocator;
    if (!device || !descriptorPool) return VK_ERROR_INITIALIZATION_FAILED;
    DxDescriptorPool* pool = new (std::nothrow) DxDescriptorPool{};
    if (!pool) return VK_ERROR_OUT_OF_HOST_MEMORY;
    pool->device = fromHandle<DxDevice>(device);
    *descriptorPool = toHandle<VkDescriptorPool>(pool);
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorPool(
    VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks* allocator)
{
    (void)device; (void)allocator;
    delete fromHandle<DxDescriptorPool>(descriptorPool);
}

VKAPI_ATTR VkResult VKAPI_CALL vkAllocateDescriptorSets(
    VkDevice device, const VkDescriptorSetAllocateInfo* allocateInfo, VkDescriptorSet* descriptorSets)
{
    if (!device || !allocateInfo || !descriptorSets) return VK_ERROR_INITIALIZATION_FAILED;
    for (uint32_t i = 0; i < allocateInfo->descriptorSetCount; ++i)
    {
        DxDescriptorSet* set = new (std::nothrow) DxDescriptorSet{};
        if (!set) return VK_ERROR_OUT_OF_HOST_MEMORY;
        set->device = fromHandle<DxDevice>(device);
        set->layout = fromHandle<DxDescriptorSetLayout>(allocateInfo->pSetLayouts[i]);
        descriptorSets[i] = toHandle<VkDescriptorSet>(set);
    }
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkFreeDescriptorSets(
    VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount,
    const VkDescriptorSet* descriptorSets)
{
    (void)device; (void)descriptorPool;
    for (uint32_t i = 0; i < descriptorSetCount; ++i)
        delete fromHandle<DxDescriptorSet>(descriptorSets[i]);
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkUpdateDescriptorSets(
    VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* descriptorWrites,
    uint32_t descriptorCopyCount, const VkCopyDescriptorSet* descriptorCopies)
{
    (void)device; (void)descriptorCopyCount; (void)descriptorCopies;
    for (uint32_t i = 0; i < descriptorWriteCount; ++i)
    {
        const VkWriteDescriptorSet& w = descriptorWrites[i];
        if (w.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || !w.pBufferInfo || w.descriptorCount != 1)
            continue;
        if (w.dstBinding >= D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT) continue;
        DxDescriptorSet* set = fromHandle<DxDescriptorSet>(w.dstSet);
        set->uniformBuffers[w.dstBinding] = fromHandle<DxBuffer>(w.pBufferInfo[0].buffer);
        set->uniformOffsets[w.dstBinding] = w.pBufferInfo[0].offset;
        set->uniformRanges[w.dstBinding] = w.pBufferInfo[0].range;
    }
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreatePipelineLayout(
    VkDevice device,
    const VkPipelineLayoutCreateInfo* createInfo,
    const VkAllocationCallbacks* allocator,
    VkPipelineLayout* pipelineLayout)
{
    (void)device;
    (void)createInfo;
    (void)allocator;

    if (!pipelineLayout)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    DxPipelineLayout* layout =
        new (std::nothrow)
            DxPipelineLayout{};

    if (!layout)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if (createInfo && createInfo->setLayoutCount)
    {
        layout->setLayouts.reserve(createInfo->setLayoutCount);
        for (uint32_t i = 0; i < createInfo->setLayoutCount; ++i)
        {
            layout->setLayouts.push_back(
                fromHandle<DxDescriptorSetLayout>(createInfo->pSetLayouts[i]));
        }
    }

    *pipelineLayout =
        toHandle<VkPipelineLayout>(
            layout
        );

    return VK_SUCCESS;
}


VKAPI_ATTR void VKAPI_CALL vkDestroyPipelineLayout(
    VkDevice device,
    VkPipelineLayout pipelineLayout,
    const VkAllocationCallbacks* allocator)
{
    (void)device;
    (void)allocator;

    DxPipelineLayout* layout =
        fromHandle<DxPipelineLayout>(
            pipelineLayout
        );

    delete layout;
}


VKAPI_ATTR VkResult VKAPI_CALL vkCreateGraphicsPipelines(
    VkDevice device,
    VkPipelineCache pipelineCache,
    uint32_t createInfoCount,
    const VkGraphicsPipelineCreateInfo* createInfos,
    const VkAllocationCallbacks* allocator,
    VkPipeline* pipelines)
{
    (void)pipelineCache;
    (void)allocator;

    if (!device ||
        !createInfos ||
        !pipelines)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    DxDevice* dxDevice =
        fromHandle<DxDevice>(
            device
        );

    for (uint32_t i = 0;
         i < createInfoCount;
         ++i)
    {
        const VkGraphicsPipelineCreateInfo& createInfo =
            createInfos[i];

        const VkPipelineShaderStageCreateInfo* vertexStage =
            nullptr;

        const VkPipelineShaderStageCreateInfo* fragmentStage =
            nullptr;

        for (uint32_t stageIndex = 0;
             stageIndex < createInfo.stageCount;
             ++stageIndex)
        {
            const VkPipelineShaderStageCreateInfo& stage =
                createInfo.pStages[stageIndex];

            if (stage.stage ==
                VK_SHADER_STAGE_VERTEX_BIT)
            {
                vertexStage =
                    &stage;
            }
            else if (stage.stage ==
                VK_SHADER_STAGE_FRAGMENT_BIT)
            {
                fragmentStage =
                    &stage;
            }
        }

        if (!vertexStage ||
            !fragmentStage)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        DxShaderModule* vertexModule =
            fromHandle<DxShaderModule>(
                vertexStage->module
            );

        DxShaderModule* fragmentModule =
            fromHandle<DxShaderModule>(
                fragmentStage->module
            );

        std::vector<uint8_t> vertexBytecode;
        std::vector<uint8_t> pixelBytecode;

        if (!spirvToDxbc(
                vertexModule,
                VK_SHADER_STAGE_VERTEX_BIT,
                vertexStage->pName,
                vertexBytecode))
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        if (!spirvToDxbc(
                fragmentModule,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                fragmentStage->pName,
                pixelBytecode))
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        DxPipeline* pipeline =
            new (std::nothrow)
                DxPipeline{};

        if (!pipeline)
        {
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }

        HRESULT result =
            dxDevice->device->CreateVertexShader(
                vertexBytecode.data(),
                vertexBytecode.size(),
                nullptr,
                &pipeline->vertexShader
            );

        if (FAILED(result))
        {
            delete pipeline;
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        result =
            dxDevice->device->CreatePixelShader(
                pixelBytecode.data(),
                pixelBytecode.size(),
                nullptr,
                &pipeline->pixelShader
            );

        if (FAILED(result))
        {
            releaseCom(
                pipeline->vertexShader
            );

            delete pipeline;
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        std::vector<D3D11_INPUT_ELEMENT_DESC> elements;
        std::vector<std::string> semanticNames;

        if (createInfo.pVertexInputState)
        {
            const VkPipelineVertexInputStateCreateInfo& inputState =
                *createInfo.pVertexInputState;

            semanticNames.reserve(
                inputState.vertexAttributeDescriptionCount
            );

            elements.reserve(
                inputState.vertexAttributeDescriptionCount
            );

            for (uint32_t attributeIndex = 0;
                 attributeIndex < inputState.vertexAttributeDescriptionCount;
                 ++attributeIndex)
            {
                const VkVertexInputAttributeDescription& attribute =
                    inputState.pVertexAttributeDescriptions[attributeIndex];

                DXGI_FORMAT format =
                    vkFormatToDxgi(
                        attribute.format
                    );

                if (format ==
                    DXGI_FORMAT_UNKNOWN)
                {
                    releaseCom(
                        pipeline->pixelShader
                    );

                    releaseCom(
                        pipeline->vertexShader
                    );

                    delete pipeline;
                    return VK_ERROR_FORMAT_NOT_SUPPORTED;
                }

                semanticNames.push_back(
                    "TEXCOORD"
                );

                D3D11_INPUT_ELEMENT_DESC element{};
                element.SemanticName =
                    semanticNames.back().c_str();
                element.SemanticIndex =
                    attribute.location;
                element.Format =
                    format;
                element.InputSlot =
                    attribute.binding;
                element.AlignedByteOffset =
                    attribute.offset;
                element.InputSlotClass =
                    D3D11_INPUT_PER_VERTEX_DATA;
                element.InstanceDataStepRate =
                    0;

                elements.push_back(
                    element
                );
            }

            if (inputState.vertexBindingDescriptionCount > 0)
            {
                pipeline->vertexStride =
                    inputState.pVertexBindingDescriptions[0].stride;
            }
        }

        if (!elements.empty())
        {
            result =
                dxDevice->device->CreateInputLayout(
                    elements.data(),
                    static_cast<UINT>(
                        elements.size()
                    ),
                    vertexBytecode.data(),
                    vertexBytecode.size(),
                    &pipeline->inputLayout
                );

            if (FAILED(result))
            {
                releaseCom(
                    pipeline->pixelShader
                );

                releaseCom(
                    pipeline->vertexShader
                );

                delete pipeline;
                return VK_ERROR_INITIALIZATION_FAILED;
            }
        }

        pipelines[i] =
            toHandle<VkPipeline>(
                pipeline
            );
    }

    return VK_SUCCESS;
}


VKAPI_ATTR void VKAPI_CALL vkDestroyPipeline(
    VkDevice device,
    VkPipeline pipeline,
    const VkAllocationCallbacks* allocator)
{
    (void)device;
    (void)allocator;

    DxPipeline* dxPipeline =
        fromHandle<DxPipeline>(
            pipeline
        );

    if (!dxPipeline)
    {
        return;
    }

    releaseCom(
        dxPipeline->inputLayout
    );

    releaseCom(
        dxPipeline->pixelShader
    );

    releaseCom(
        dxPipeline->vertexShader
    );

    delete dxPipeline;
}


VKAPI_ATTR VkResult VKAPI_CALL vkCreateCommandPool(
    VkDevice device,
    const VkCommandPoolCreateInfo* createInfo,
    const VkAllocationCallbacks* allocator,
    VkCommandPool* commandPool)
{
    (void)createInfo;
    (void)allocator;

    if (!commandPool)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    DxCommandPool* pool =
        new (std::nothrow)
            DxCommandPool{};

    if (!pool)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    pool->device =
        fromHandle<DxDevice>(
            device
        );

    *commandPool =
        toHandle<VkCommandPool>(
            pool
        );

    return VK_SUCCESS;
}


VKAPI_ATTR void VKAPI_CALL vkDestroyCommandPool(
    VkDevice device,
    VkCommandPool commandPool,
    const VkAllocationCallbacks* allocator)
{
    (void)device;
    (void)allocator;

    DxCommandPool* pool =
        fromHandle<DxCommandPool>(
            commandPool
        );

    delete pool;
}


VKAPI_ATTR VkResult VKAPI_CALL vkAllocateCommandBuffers(
    VkDevice device,
    const VkCommandBufferAllocateInfo* allocateInfo,
    VkCommandBuffer* commandBuffers)
{
    if (!allocateInfo ||
        !commandBuffers)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    DxDevice* dxDevice =
        fromHandle<DxDevice>(
            device
        );

    for (uint32_t i = 0;
         i < allocateInfo->commandBufferCount;
         ++i)
    {
        DxCommandBuffer* command =
            new (std::nothrow)
                DxCommandBuffer{};

        if (!command)
        {
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }

        command->device =
            dxDevice;

        commandBuffers[i] =
            toHandle<VkCommandBuffer>(
                command
            );
    }

    return VK_SUCCESS;
}


VKAPI_ATTR void VKAPI_CALL vkFreeCommandBuffers(
    VkDevice device,
    VkCommandPool commandPool,
    uint32_t commandBufferCount,
    const VkCommandBuffer* commandBuffers)
{
    (void)device;
    (void)commandPool;

    if (!commandBuffers)
    {
        return;
    }

    for (uint32_t i = 0;
         i < commandBufferCount;
         ++i)
    {
        DxCommandBuffer* command =
            fromHandle<DxCommandBuffer>(
                commandBuffers[i]
            );

        delete command;
    }
}


VKAPI_ATTR VkResult VKAPI_CALL vkBeginCommandBuffer(
    VkCommandBuffer commandBuffer,
    const VkCommandBufferBeginInfo* beginInfo)
{
    (void)beginInfo;

    DxCommandBuffer* command =
        fromHandle<DxCommandBuffer>(
            commandBuffer
        );

    if (!command)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    command->pipeline = nullptr;

    command->vertexBuffer = nullptr;
    command->indexBuffer = nullptr;

    command->vertexBufferOffset = 0;
    command->indexBufferOffset = 0;

    command->indexType =
        VK_INDEX_TYPE_UINT16;

    command->indexCount = 0;
    command->firstIndex = 0;
    command->vertexOffset = 0;

    return VK_SUCCESS;
}


VKAPI_ATTR VkResult VKAPI_CALL vkEndCommandBuffer(
    VkCommandBuffer commandBuffer)
{
    if (!commandBuffer)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return VK_SUCCESS;
}


VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(
    VkCommandBuffer commandBuffer,
    VkPipelineBindPoint pipelineBindPoint,
    VkPipeline pipeline)
{
    if (pipelineBindPoint !=
        VK_PIPELINE_BIND_POINT_GRAPHICS)
    {
        return;
    }

    DxCommandBuffer* command =
        fromHandle<DxCommandBuffer>(
            commandBuffer
        );

    if (!command)
    {
        return;
    }

    command->pipeline =
        fromHandle<DxPipeline>(
            pipeline
        );
}


VKAPI_ATTR void VKAPI_CALL vkCmdBindVertexBuffers(
    VkCommandBuffer commandBuffer,
    uint32_t firstBinding,
    uint32_t bindingCount,
    const VkBuffer* buffers,
    const VkDeviceSize* offsets)
{
    (void)firstBinding;

    DxCommandBuffer* command =
        fromHandle<DxCommandBuffer>(
            commandBuffer
        );

    if (!command ||
        !buffers ||
        bindingCount == 0)
    {
        return;
    }

    command->vertexBuffer =
        fromHandle<DxBuffer>(
            buffers[0]
        );

    if (offsets)
    {
        command->vertexBufferOffset =
            offsets[0];
    }
}


VKAPI_ATTR void VKAPI_CALL vkCmdBindIndexBuffer(
    VkCommandBuffer commandBuffer,
    VkBuffer buffer,
    VkDeviceSize offset,
    VkIndexType indexType)
{
    DxCommandBuffer* command =
        fromHandle<DxCommandBuffer>(
            commandBuffer
        );

    if (!command)
    {
        return;
    }

    command->indexBuffer =
        fromHandle<DxBuffer>(
            buffer
        );

    command->indexBufferOffset =
        offset;

    command->indexType =
        indexType;
}


VKAPI_ATTR void VKAPI_CALL vkCmdBindDescriptorSets(
    VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
    VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount,
    const VkDescriptorSet* descriptorSets, uint32_t dynamicOffsetCount,
    const uint32_t* dynamicOffsets)
{
    (void)pipelineBindPoint; (void)dynamicOffsetCount; (void)dynamicOffsets;
    DxCommandBuffer* command = fromHandle<DxCommandBuffer>(commandBuffer);
    if (!command) return;
    command->pipelineLayout = fromHandle<DxPipelineLayout>(layout);
    for (uint32_t i = 0; i < descriptorSetCount && firstSet + i < 4; ++i)
        command->descriptorSets[firstSet + i] = fromHandle<DxDescriptorSet>(descriptorSets[i]);
}

VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexed(
    VkCommandBuffer commandBuffer,
    uint32_t indexCount,
    uint32_t instanceCount,
    uint32_t firstIndex,
    int32_t vertexOffset,
    uint32_t firstInstance)
{
    (void)instanceCount;
    (void)firstInstance;

    DxCommandBuffer* command =
        fromHandle<DxCommandBuffer>(
            commandBuffer
        );

    if (!command)
    {
        return;
    }

    command->indexCount =
        indexCount;

    command->firstIndex =
        firstIndex;

    command->vertexOffset =
        vertexOffset;
}


VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(
    VkQueue queue,
    uint32_t submitCount,
    const VkSubmitInfo* submits,
    VkFence fence)
{
    (void)fence;

    DxQueue* dxQueue =
        fromHandle<DxQueue>(
            queue
        );

    if (!dxQueue ||
        !submits)
    {
        return VK_ERROR_DEVICE_LOST;
    }

    DxDevice* device =
        dxQueue->device;

    for (uint32_t submitIndex = 0;
         submitIndex < submitCount;
         ++submitIndex)
    {
        const VkSubmitInfo& submit =
            submits[submitIndex];

        for (uint32_t commandIndex = 0;
             commandIndex < submit.commandBufferCount;
             ++commandIndex)
        {
            DxCommandBuffer* command =
                fromHandle<DxCommandBuffer>(
                    submit.pCommandBuffers[
                        commandIndex
                    ]
                );

            if (!command ||
                !command->pipeline ||
                !command->vertexBuffer ||
                !command->indexBuffer)
            {
                continue;
            }

            uploadBuffer(
                command->vertexBuffer
            );

            uploadBuffer(
                command->indexBuffer
            );


            /*
             * VkPipeline -> D3D11 pipeline.
             */

            device->context->IASetInputLayout(
                command->pipeline->inputLayout
            );

            device->context->VSSetShader(
                command->pipeline->vertexShader,
                nullptr,
                0
            );

            device->context->PSSetShader(
                command->pipeline->pixelShader,
                nullptr,
                0
            );

            device->context->IASetPrimitiveTopology(
                D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
            );


            /*
             * Vertex buffer.
             */

            UINT stride =
                command->pipeline->vertexStride;

            UINT vertexOffset =
                static_cast<UINT>(
                    command->vertexBufferOffset
                );

            ID3D11Buffer* vertexBuffer =
                command->vertexBuffer->buffer;

            device->context->IASetVertexBuffers(
                0,
                1,
                &vertexBuffer,
                &stride,
                &vertexOffset
            );


            /*
             * Index buffer.
             */

            DXGI_FORMAT indexFormat =
                DXGI_FORMAT_R16_UINT;

            if (command->indexType ==
                VK_INDEX_TYPE_UINT32)
            {
                indexFormat =
                    DXGI_FORMAT_R32_UINT;
            }

            device->context->IASetIndexBuffer(
                command->indexBuffer->buffer,
                indexFormat,
                static_cast<UINT>(
                    command->indexBufferOffset
                )
            );


            /* Vulkan descriptor set 0 -> D3D11 constant-buffer slots bN. */
            DxDescriptorSet* set0 = command->descriptorSets[0];
            if (set0 && set0->layout)
            {
                for (const DxDescriptorSetLayoutBinding& binding : set0->layout->bindings)
                {
                    if (binding.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) continue;
                    DxBuffer* uniform = set0->uniformBuffers[binding.binding];
                    if (!uniform) continue;
                    uploadBuffer(uniform);
                    ID3D11Buffer* cb = uniform->buffer;
                    if (binding.stageFlags & VK_SHADER_STAGE_VERTEX_BIT)
                        device->context->VSSetConstantBuffers(binding.binding, 1, &cb);
                    if (binding.stageFlags & VK_SHADER_STAGE_FRAGMENT_BIT)
                        device->context->PSSetConstantBuffers(binding.binding, 1, &cb);
                }
            }

            /*
             * Draw.
             */

            device->context->DrawIndexed(
                command->indexCount,
                command->firstIndex,
                command->vertexOffset
            );
        }
    }

    return VK_SUCCESS;
}


VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(
    VkDevice device)
{
    (void)device;

    return VK_SUCCESS;
}


VKAPI_ATTR VkResult VKAPI_CALL vkQueueWaitIdle(
    VkQueue queue)
{
    (void)queue;

    return VK_SUCCESS;
}


VKAPI_ATTR VkResult VKAPI_CALL vkdx11CreateWindowSurface(
    VkDevice device,
    HWND hwnd,
    uint32_t width,
    uint32_t height)
{
    DxDevice* dxDevice =
        fromHandle<DxDevice>(
            device
        );

    if (!dxDevice ||
        !hwnd ||
        width == 0 ||
        height == 0)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    dxDevice->width =
        width;

    dxDevice->height =
        height;

    IDXGIDevice* dxgiDevice =
        nullptr;

    IDXGIAdapter* adapter =
        nullptr;

    IDXGIFactory* factory =
        nullptr;

    HRESULT result =
        dxDevice->device->QueryInterface(
            __uuidof(IDXGIDevice),
            reinterpret_cast<void**>(
                &dxgiDevice
            )
        );

    if (SUCCEEDED(result))
    {
        result =
            dxgiDevice->GetAdapter(
                &adapter
            );
    }

    if (SUCCEEDED(result))
    {
        result =
            adapter->GetParent(
                __uuidof(IDXGIFactory),
                reinterpret_cast<void**>(
                    &factory
                )
            );
    }

    DXGI_SWAP_CHAIN_DESC swapChainDescription{};

    swapChainDescription.BufferDesc.Width =
        width;

    swapChainDescription.BufferDesc.Height =
        height;

    swapChainDescription.BufferDesc.Format =
        DXGI_FORMAT_R8G8B8A8_UNORM;

    swapChainDescription.SampleDesc.Count =
        1;

    swapChainDescription.BufferUsage =
        DXGI_USAGE_RENDER_TARGET_OUTPUT;

    swapChainDescription.BufferCount =
        2;

    swapChainDescription.OutputWindow =
        hwnd;

    swapChainDescription.Windowed =
        TRUE;

    swapChainDescription.SwapEffect =
        DXGI_SWAP_EFFECT_DISCARD;

    if (SUCCEEDED(result))
    {
        result =
            factory->CreateSwapChain(
                dxDevice->device,
                &swapChainDescription,
                &dxDevice->swapChain
            );
    }

    releaseCom(
        factory
    );

    releaseCom(
        adapter
    );

    releaseCom(
        dxgiDevice
    );

    if (FAILED(result))
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    ID3D11Texture2D* backBuffer =
        nullptr;

    result =
        dxDevice->swapChain->GetBuffer(
            0,
            __uuidof(ID3D11Texture2D),
            reinterpret_cast<void**>(
                &backBuffer
            )
        );

    if (SUCCEEDED(result))
    {
        result =
            dxDevice->device
                ->CreateRenderTargetView(
                    backBuffer,
                    nullptr,
                    &dxDevice->renderTarget
                );
    }

    releaseCom(
        backBuffer
    );

    if (FAILED(result))
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return VK_SUCCESS;
}


VKAPI_ATTR void VKAPI_CALL vkdx11BeginFrame(
    VkDevice device,
    float r,
    float g,
    float b,
    float a)
{
    DxDevice* dxDevice =
        fromHandle<DxDevice>(
            device
        );

    if (!dxDevice ||
        !dxDevice->renderTarget)
    {
        return;
    }

    float clearColor[4] =
    {
        r,
        g,
        b,
        a
    };

    dxDevice->context->ClearRenderTargetView(
        dxDevice->renderTarget,
        clearColor
    );

    dxDevice->context->OMSetRenderTargets(
        1,
        &dxDevice->renderTarget,
        nullptr
    );

    D3D11_VIEWPORT viewport{};

    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;

    viewport.Width =
        static_cast<float>(
            dxDevice->width
        );

    viewport.Height =
        static_cast<float>(
            dxDevice->height
        );

    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    dxDevice->context->RSSetViewports(
        1,
        &viewport
    );
}


VKAPI_ATTR void VKAPI_CALL vkdx11Present(
    VkDevice device)
{
    DxDevice* dxDevice =
        fromHandle<DxDevice>(
            device
        );

    if (!dxDevice ||
        !dxDevice->swapChain)
    {
        return;
    }

    dxDevice->swapChain->Present(
        1,
        0
    );
}


}
