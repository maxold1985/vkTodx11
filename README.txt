Vulkan -> Direct3D 11
Pipeline corrigido
==================

Esta versao corrige a tela preta da versao anterior.

Implementado:

    vkCreateShaderModule
    vkDestroyShaderModule

    vkCreatePipelineLayout
    vkDestroyPipelineLayout

    vkCreateGraphicsPipelines
    vkDestroyPipeline

    vkCmdBindPipeline

Backend:

    VkPipeline
        |
        +-- ID3D11VertexShader
        +-- ID3D11PixelShader
        +-- ID3D11InputLayout

No submit:

    vkQueueSubmit
        |
        +-- IASetInputLayout
        +-- VSSetShader
        +-- PSSetShader
        +-- IASetVertexBuffers
        +-- IASetIndexBuffer
        +-- DrawIndexed

Resultado esperado:

    triangulo RGB em fundo escuro.

IMPORTANTE

O VkShaderModule ainda NAO traduz SPIR-V real.

Nesta etapa, o backend compila HLSL interno usando D3DCompile.
Isso valida o pipeline Vulkan -> D3D11.

Para SPIR-V real, o proximo modulo sera:

    SPIR-V
       |
       v
    SPIR-V -> HLSL
       |
       v
    D3DCompile
       |
       v
    DXBC
       |
       v
    Direct3D 11

BUILD

    set VULKAN_SDK=C:\VulkanSDK\1.4.321.1
    build.bat

Bibliotecas:

    d3d11
    dxgi
    d3dcompiler
    user32
    gdi32

Nao linkar vulkan-1.

As funcoes Vulkan deste experimento sao implementadas diretamente
em src/vk_d3d11.cpp.
