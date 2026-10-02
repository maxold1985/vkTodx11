Vulkan 1.0 subset -> D3D11 / Windows 7

Esta versao NAO usa glslc e NAO carrega arquivos .spv externos.
O vertex shader e o fragment shader SPIR-V 1.0 estao embutidos em src/main.cpp
como arrays uint32_t.

Runtime de shader atual:
SPIR-V embutido -> SPIRV-Cross -> HLSL SM5 -> fxc.exe -> DXBC -> D3D11

IMPORTANTE:
- SPIRV-Cross deve existir em third_party/SPIRV-Cross (fetch_spirv_cross.bat baixa o fonte).
- FXC ainda e usado pelo backend em runtime nesta etapa. Defina FXC com o caminho do fxc.exe
  ou deixe fxc.exe no PATH.
- VULKAN_SDK so e usado para os headers Vulkan durante a compilacao do projeto.
- glslc.exe nao e necessario.
