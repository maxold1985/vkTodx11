Vulkan 1.0 subset -> SPIR-V -> SPIRV-Cross -> HLSL -> FXC -> DXBC -> D3D11

1. Rode fetch_spirv_cross.bat uma vez.
2. Defina VULKAN_SDK.
3. Defina FXC com o caminho completo do fxc.exe, por exemplo:
   set FXC=C:\Program Files (x86)\Windows Kits\8.1\bin\x86\fxc.exe
   Se fxc.exe estiver no PATH, FXC pode ficar indefinido.
4. Rode build.bat.

Mudanca principal:
- vkCreateShaderModule agora guarda SPIR-V real.
- vkCreateGraphicsPipelines pega os VkShaderModule dos stages.
- SPIRV-Cross converte cada stage para HLSL Shader Model 5.0.
- FXC gera DXBC temporario.
- D3D11 cria ID3D11VertexShader/ID3D11PixelShader a partir desse DXBC.
- Nao ha link com d3dcompiler e o EXE nao chama D3DCompile.

Limitacoes atuais:
- vertex + fragment apenas.
- formatos de vertex implementados: R32, RG32, RGB32, RGBA32 float e RGBA8 UNORM.
- descriptors, images, samplers, UBOs, render pass completo e sincronizacao Vulkan ainda nao estao implementados.
- FXC precisa estar disponivel quando um pipeline novo precisar ser compilado. O proximo passo e adicionar cache de DXBC para eliminar essa necessidade em execucoes seguintes.
