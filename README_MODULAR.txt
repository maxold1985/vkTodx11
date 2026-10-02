MODULARIZACAO
=============

src/main.cpp
  Somente ponto de entrada.

src/app/application.*
  Janela Win32, mensagens e loop principal.

src/renderer/renderer.*
  Inicializacao Vulkan-like, descriptors, pipeline, command buffer, render e cleanup.

src/renderer/buffer_utils.*
  Helper de VkBuffer + VkDeviceMemory + upload inicial.

src/shaders/embedded_spirv.*
  SPIR-V 1.0 embutido usado pelo VkShaderModule.

src/vk_d3d11.*
  Backend/traducao Vulkan -> Direct3D 11. Continua isolado da aplicacao.

O comportamento do teste continua sendo o triangulo conhecido. A modularizacao foi feita
antes de trocar o shader por MVP/cubo para reduzir regressao e facilitar a proxima etapa.
