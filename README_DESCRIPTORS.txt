ETAPA: DESCRIPTOR SET + UNIFORM BUFFER

Implementado:
- vkCreateDescriptorSetLayout / vkDestroyDescriptorSetLayout
- vkCreateDescriptorPool / vkDestroyDescriptorPool
- vkAllocateDescriptorSets / vkFreeDescriptorSets
- vkUpdateDescriptorSets
- vkCmdBindDescriptorSets
- VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT -> D3D11_BIND_CONSTANT_BUFFER
- binding N -> D3D11 constant-buffer slot bN
- stageFlags VERTEX/FRAGMENT -> VSSetConstantBuffers / PSSetConstantBuffers

IMPORTANTE:
O shader SPIR-V de teste desta etapa ainda e o shader de triangulo anterior e nao declara o UBO.
O descriptor e o constant buffer ja percorrem toda a API/backend D3D11, mas a matriz ainda nao altera a imagem.
A proxima etapa e trocar o SPIR-V embutido por um vertex shader que declare layout(set=0,binding=0) uniform MVP.
