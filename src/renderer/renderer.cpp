#include "renderer.h"
#include "buffer_utils.h"
#include "../shaders/embedded_spirv.h"
#include <cstdint>

namespace
{
struct Vertex
{
    float x, y, z;
    float r, g, b;
};

struct UniformData
{
    float mvp[16];
};
}

bool Renderer::fail(const wchar_t* message)
{
    MessageBoxW(nullptr, message, L"Vulkan -> D3D11", MB_OK | MB_ICONERROR);
    return false;
}

bool Renderer::initialize(HWND hwnd, uint32_t width, uint32_t height)
{
    width_ = width;
    height_ = height;

    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    if (vkCreateInstance(&instanceInfo, nullptr, &instance_) != VK_SUCCESS)
        return fail(L"vkCreateInstance falhou.");

    uint32_t physicalCount = 1;
    if (vkEnumeratePhysicalDevices(instance_, &physicalCount, &physicalDevice_) != VK_SUCCESS || physicalCount == 0)
        return fail(L"vkEnumeratePhysicalDevices falhou.");

    float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = 0;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;

    if (vkCreateDevice(physicalDevice_, &deviceInfo, nullptr, &device_) != VK_SUCCESS)
        return fail(L"vkCreateDevice falhou.");

    vkGetDeviceQueue(device_, 0, 0, &queue_);

    if (vkdx11CreateWindowSurface(device_, hwnd, width_, height_) != VK_SUCCESS)
        return fail(L"vkdx11CreateWindowSurface falhou.");

    return createGeometry() && createDescriptors() && createPipeline() && createCommands();
}

bool Renderer::createGeometry()
{
    const Vertex vertices[] =
    {
        {-0.75f, -0.65f, 0.0f, 1.0f, 0.0f, 0.0f},
        { 0.00f,  0.75f, 0.0f, 0.0f, 1.0f, 0.0f},
        { 0.75f, -0.65f, 0.0f, 0.0f, 0.0f, 1.0f}
    };
    const uint16_t indices[] = {0, 1, 2};

    if (!createBufferWithMemory(device_, vertices, sizeof(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &vertexBuffer_, &vertexMemory_))
        return fail(L"Falha ao criar vertex buffer.");

    if (!createBufferWithMemory(device_, indices, sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, &indexBuffer_, &indexMemory_))
        return fail(L"Falha ao criar index buffer.");

    return true;
}

bool Renderer::createDescriptors()
{
    UniformData uniform{};
    uniform.mvp[0] = 1.0f;
    uniform.mvp[5] = 1.0f;
    uniform.mvp[10] = 1.0f;
    uniform.mvp[15] = 1.0f;

    if (!createBufferWithMemory(device_, &uniform, sizeof(uniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &uniformBuffer_, &uniformMemory_))
        return fail(L"Falha ao criar uniform buffer.");

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &descriptorSetLayout_) != VK_SUCCESS)
        return fail(L"vkCreateDescriptorSetLayout falhou.");

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS)
        return fail(L"vkCreateDescriptorPool falhou.");

    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = descriptorPool_;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &descriptorSetLayout_;
    if (vkAllocateDescriptorSets(device_, &allocateInfo, &descriptorSet_) != VK_SUCCESS)
        return fail(L"vkAllocateDescriptorSets falhou.");

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = uniformBuffer_;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformData);

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptorSet_;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    return true;
}

bool Renderer::createPipeline()
{
    VkShaderModuleCreateInfo shaderInfo{};
    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = kVertexSpirvSize;
    shaderInfo.pCode = kVertexSpirv;
    if (vkCreateShaderModule(device_, &shaderInfo, nullptr, &vertexShader_) != VK_SUCCESS)
        return fail(L"vkCreateShaderModule(vertex) falhou.");

    shaderInfo.codeSize = kFragmentSpirvSize;
    shaderInfo.pCode = kFragmentSpirv;
    if (vkCreateShaderModule(device_, &shaderInfo, nullptr, &fragmentShader_) != VK_SUCCESS)
        return fail(L"vkCreateShaderModule(fragment) falhou.");

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout_;
    if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS)
        return fail(L"vkCreatePipelineLayout falhou.");

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertexShader_;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragmentShader_;
    stages[1].pName = "main";

    VkVertexInputBindingDescription vertexBinding{};
    vertexBinding.binding = 0;
    vertexBinding.stride = sizeof(Vertex);
    vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributes[2]{};
    attributes[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};
    attributes[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3};

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &vertexBinding;
    vertexInput.vertexAttributeDescriptionCount = 2;
    vertexInput.pVertexAttributeDescriptions = attributes;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{};
    viewport.width = static_cast<float>(width_);
    viewport.height = static_cast<float>(height_);
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{};
    scissor.extent = {width_, height_};

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState attachment{};
    attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &attachment;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &raster;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pColorBlendState = &blend;
    pipelineInfo.layout = pipelineLayout_;

    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline_) != VK_SUCCESS)
        return fail(L"vkCreateGraphicsPipelines falhou. Verifique FXC, SPIR-V e vk_pipeline.log.");
    return true;
}

bool Renderer::createCommands()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = 0;
    if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS)
        return fail(L"vkCreateCommandPool falhou.");

    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = commandPool_;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(device_, &allocateInfo, &commandBuffer_) != VK_SUCCESS)
        return fail(L"vkAllocateCommandBuffers falhou.");

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(commandBuffer_, &beginInfo) != VK_SUCCESS)
        return fail(L"vkBeginCommandBuffer falhou.");

    vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &descriptorSet_, 0, nullptr);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffer_, 0, 1, &vertexBuffer_, &offset);
    vkCmdBindIndexBuffer(commandBuffer_, indexBuffer_, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(commandBuffer_, 3, 1, 0, 0, 0);
    return vkEndCommandBuffer(commandBuffer_) == VK_SUCCESS;
}

void Renderer::render()
{
    vkdx11BeginFrame(device_, 0.03f, 0.04f, 0.07f, 1.0f);
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commandBuffer_;
    vkQueueSubmit(queue_, 1, &submit, VK_NULL_HANDLE);
    vkdx11Present(device_);
}

void Renderer::shutdown()
{
    if (!device_)
        return;
    vkDeviceWaitIdle(device_);
    if (commandBuffer_) vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer_);
    if (commandPool_) vkDestroyCommandPool(device_, commandPool_, nullptr);
    if (pipeline_) vkDestroyPipeline(device_, pipeline_, nullptr);
    if (pipelineLayout_) vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
    if (fragmentShader_) vkDestroyShaderModule(device_, fragmentShader_, nullptr);
    if (vertexShader_) vkDestroyShaderModule(device_, vertexShader_, nullptr);
    if (descriptorSet_ && descriptorPool_) vkFreeDescriptorSets(device_, descriptorPool_, 1, &descriptorSet_);
    if (descriptorPool_) vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
    if (descriptorSetLayout_) vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
    if (uniformBuffer_) vkDestroyBuffer(device_, uniformBuffer_, nullptr);
    if (uniformMemory_) vkFreeMemory(device_, uniformMemory_, nullptr);
    if (indexBuffer_) vkDestroyBuffer(device_, indexBuffer_, nullptr);
    if (indexMemory_) vkFreeMemory(device_, indexMemory_, nullptr);
    if (vertexBuffer_) vkDestroyBuffer(device_, vertexBuffer_, nullptr);
    if (vertexMemory_) vkFreeMemory(device_, vertexMemory_, nullptr);
    vkDestroyDevice(device_, nullptr);
    device_ = VK_NULL_HANDLE;
    if (instance_) vkDestroyInstance(instance_, nullptr);
    instance_ = VK_NULL_HANDLE;
}
