#include <vulkan/vulkan.h>           // A API principal do Vulkan
#include <vulkan/vulkan_android.h>
#include <game-activity/native_app_glue/android_native_app_glue.h> // A ponte entre o ciclo de vida do Android e o C++
#include <android/log.h>             //  Logcat
#include <cstring>
#include <jni.h>
#include <vector>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "VulkanApp", __VA_ARGS__)) // Helper Log

// Globais
VkInstance instance;             // Conexão com o Vulkan
VkPhysicalDevice physicalDevice; // A placa de vídeo física do celular
VkDevice logicalDevice;          // Sessão de uso (Device Lógico)
VkSwapchainKHR swapchain;        // A Fila de telas
VkSurfaceKHR surface;
VkRenderPass renderPass;
VkFormat swapchainImageFormat; // formato da nossa tela
VkExtent2D swapchainExtent;
VkCommandPool commandPool;     // gerenciador de memória dos comandos
VkCommandBuffer commandBuffer; // buffer onde os comandos de desenho serão gravados
VkPipelineLayout pipelineLayout; // Controla os parâmetros uniformes (matrizes, texturas) passados para os shaders
VkPipeline graphicsPipeline;     // objeto que consolida todo o estado do pipeline grafico
VkSemaphore imageAvailableSemaphore; // Sinaliza que a Swapchain liberou uma imagem para desenho
VkSemaphore renderFinishedSemaphore; // Sinaliza que a GPU terminou de desenhar o quadro
VkFence inFlightFence;               // Controla se a GPU terminou todo o trabalho do quadro atual
VkQueue graphicsQueue; // Fila para submissão dos comandos gráficos
VkQueue presentQueue;  // Fila para apresentação da imagem no ecrã

std::vector<VkImage> swapchainImages; // imagens brutas
std::vector<VkImageView> swapchainImagesViews;
std::vector<VkFramebuffer> swapchainFrameBuffers;

void createLogicalDevice() {
    VkDeviceQueueCreateInfo queueInfo{}; // {} <- iniciar sem sujeira

    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; // must
    queueInfo.queueFamilyIndex = 0; // Escolha da GPU
    queueInfo.queueCount = 1;

    // Must
    float queuePriority = 1.0f;
    queueInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;

    // Opcional Configs extras device por enquanto vazio
    deviceInfo.enabledExtensionCount = 0;
    deviceInfo.enabledLayerCount = 0;

    if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &logicalDevice) != VK_SUCCESS) {
        LOGI("Falha ao criar o Logical Device!");
        return;
    }
    LOGI("Logical Device criado com sucesso!");

    vkGetDeviceQueue(logicalDevice, 0, 0, &graphicsQueue);
    vkGetDeviceQueue(logicalDevice, 0, 0, &presentQueue);
}

// Ponte da GPU e a Tela device
void createSwapChain(struct android_app *app) {

    // CRIANDO A SUPERFÍCIE (A Ponte com a Janela do Android)
    VkAndroidSurfaceCreateInfoKHR surfaceInfo{};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.window = app->window; // aqui pegamos a janela do device

    // Criar a Surface
    if (vkCreateAndroidSurfaceKHR(instance, &surfaceInfo, nullptr, &surface) != VK_SUCCESS) {
        LOGI("Falha ao criar a Surface do Android!");
        return;
    }

    // PREPARANDO AS INFORMAÇÕES DA TELA (Resolução)
    int width = ANativeWindow_getWidth(app->window);
    int height = ANativeWindow_getHeight(app->window);

    swapchainExtent.width = (uint32_t) width;
    swapchainExtent.height = (uint32_t) height;

    // FORMULÁRIO DA SWAPCHAIN
    VkSwapchainCreateInfoKHR swapchainInfo{};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;

    // Conectar Swapchain à Surface
    swapchainInfo.surface = surface;

    // Quantas imagens na fila? (2 = Double Buffering)
    swapchainInfo.minImageCount = 2;

    // Formato de cor (8 bits por canal: Azul, Verde, Vermelho, Alpha)
    swapchainInfo.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
    swapchainInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    // O tamanho das imagens (a resolução do celular)
    swapchainInfo.imageExtent = swapchainExtent;

    // Sempre 1, a menos seja um app de Realidade Virtual
    swapchainInfo.imageArrayLayers = 1;

    // Usar essas imagens para desenhar cores nelas
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // Sem transformação (não queremos rotacionar a imagem
    swapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

    // Ignorar a transparência da janela do Android
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    // V-Sync -> Modo FIFO garante que a troca de imagens espere a tela piscar.
    // Assim economizar bateria
    // obriga a sua GPU a entrar em "modo de espera" se ela desenhar os quadros mais rápido do que a tela do celular consegue exibir
    swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;

    // Ignora o desenho de pixels que estejam escondidos atrás de menus do sistema
    swapchainInfo.clipped = VK_TRUE;

    // Se recriando a tela (ex: girou o celular), passaríamos a antiga aqui
    swapchainInfo.oldSwapchain = VK_NULL_HANDLE;

    // Device e o endereço do nosso formulário (&swapchainInfo)
    if (vkCreateSwapchainKHR(logicalDevice, &swapchainInfo, nullptr, &swapchain) != VK_SUCCESS) {
        LOGI("Falha ao criar a Swapchain!");
    } else {
        LOGI("Swapchain criada com sucesso! Resolução: %d x %d", width, height);
    }

}

void createImageViews() {
    // 1 RESGATANDO AS IMAGENS BRUTAS DA SWAPCHAIN
    uint32_t imageCount;

    // Chamada 1: ultimo parâmetro vazio (nullptr) só para descobrir o 'imageCount'
    vkGetSwapchainImagesKHR(logicalDevice, swapchain, &imageCount, nullptr);

    swapchainImages.resize(imageCount);

    // Chamada 2: Endereço da lista (o .data() do vector) para preenchê-la
    vkGetSwapchainImagesKHR(logicalDevice, swapchain, &imageCount, swapchainImages.data());

    LOGI("%d imagens da Swapchain", imageCount);

    // 2 - CRIANDO AS IMAGE VIEWS
    // View para CADA imagem
    swapchainImagesViews.resize(swapchainImages.size());

    for (size_t i = 0; i < swapchainImages.size(); i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

        viewInfo.image = swapchainImages[i];
        // textura 2D normal
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchainImageFormat;

        // nao misturar cores, deixar o padrão
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        // 'subresourceRange' descreve qual parte da imagem usaremos.
        // imagem de cor, sem múltiplas camadas (sem 3D ou Mipmaps)
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(logicalDevice, &viewInfo, nullptr, &swapchainImagesViews[i]) !=
            VK_SUCCESS) {
            LOGI("Falha ao criar a Image View número %zu!", i);
        }
    }

    LOGI("Image Views criadas com sucesso!");
}

// O Render Pass dita três regras fundamentais para este anexo:
//
//Load Op (Operação de Carregamento): O que fazer com o bloco de memória antes de desenhar? (Ex: Limpar com uma cor preta, ou manter o desenho do quadro anterior).
//
//Store Op (Operação de Armazenamento): O que fazer após finalizar o desenho? (Ex: Salvar o resultado final na memória RAM para que o Android possa exibi-lo).
//
//Layout Transitions (Transições de Layout): A memória das imagens da GPU muda de formato interno dependendo do que está sendo feito. A imagem deve transitar de "Indefinida" para "Otimizada para Desenho" e, finalmente, para "Pronta para Apresentação na Tela".
void createRenderPass() {
    // 1. DEFINIÇÃO DO ANEXO DE COR (A Tela)
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainImageFormat;

    // Sem anti-aliasing
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

    // Regras de início e fim para o Stencil (ignorado em desenhos básicos 2D)
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;   // Limpar a tela antes de desenhar
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Salvar o resultado na RAM no final

    // 2. REFERÊNCIA E SUBPASS
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0; // Índice do anexo (0, pois só existe um)
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // Indica que é uma operação gráfica, não computacional
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    // 3. CRIAÇÃO DO RENDER PASS
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    if (vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        LOGI("Falha ao criar o Render Pass!");
    } else {
        LOGI("Render Pass criado com sucesso!");
    }

}

void createFrameBuffers() {
    swapchainFrameBuffers.resize(swapchainImagesViews.size());

    for (size_t i = 0; i < swapchainImagesViews.size(); i++) {
        VkImageView attachments[] = {
                swapchainImagesViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;

        framebufferInfo.width = swapchainExtent.width;
        framebufferInfo.height = swapchainExtent.height;

        // Apenas 1 camada (imagens normais 2D)
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr,
                                &swapchainFrameBuffers[i]) != VK_SUCCESS) {
            LOGI("Falha ao criar o Framebuffer %zu!", i);
        }
    }
    LOGI("Framebuffers criados com sucesso!");
}

void createCommandPoolAndBuffer() {
    // 1. CRIANDO O COMMAND POOL
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

    // Indica que o buffer de comando será regravado a cada quadro
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    // O pool deve estar associado à mesma família de fila de gráficos definida no início
    poolInfo.queueFamilyIndex = 0;

    if (vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        LOGI("Falha ao criar o Command Pool!");
        return;
    }
    LOGI("Command Pool criado com sucesso");

    // 2. ALOCANDO O COMMAND BUFFER
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

    allocInfo.commandPool = commandPool;
    // PRIMARY significa que este buffer pode ser enviado diretamente para a fila da GPU
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        LOGI("Falha ao alocar o Command Buffer!");
    } else {
        LOGI("Command Buffer alocado com sucesso!");
    }
}

// Função utilitária para encapsular o binário do Shader | ler o arquivo binário .spv
VkShaderModule createShaderModule(const std::vector<char> &code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();

    // Vulkan espera um ponteiro do tipo uint32_t para os dados binários
    createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        LOGI("Falha ao criar o modulo de shader");
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}

void createGraphicsPipeline() {
    // 3.1 Carregamento simulado dos binários SPIR-V (Vértice e Fragmento)
    // Em um projeto real do Android NDK, utiliza-se a API 'AAssetManager' para ler estes arquivos.
    std::vector<char> vertShaderCode = { /* bytes do arquivo shader.vert.spv */ };
    std::vector<char> fragShaderCode = { /* bytes do arquivo shader.frag.spv */ };

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    // Configuração do estágio do Vertex Shader
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main"; // O ponto de entrada dentro do shader

    // Configuração do estágio do Fragment Shader
    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // 3.2 ESTADOS FIXOS DO PIPELINE (FIXED FUNCTIONS)

    // Vertex Input: Descreve o formato dos vértices (enviados via memcpy anteriormente)
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0; // Configuração simples sem buffers dinâmicos por enquanto
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    // Input Assembly: Define a topologia (Triângulos isolados)
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport e Scissor: Delimitam a região geométrica de desenho baseada na extensão da tela
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) swapchainExtent.width;
    viewport.height = (float) swapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // Rasterizer: Transforma a geometria em fragmentos/pixels e define o Culling (Descarte de faces)
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // Preenchimento completo do triângulo
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;   // Descarte de geometria traseira para performance
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    // Multisampling: Configurações de Anti-Aliasing (Desativado para simplicidade)
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Color Blending: Como misturar a nova cor com a cor que já existia no pixel (Modo Opaco)
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // 3.3 CRIAÇÃO DO PIPELINE LAYOUT
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) !=
        VK_SUCCESS) {
        LOGI("Falha ao criar o Pipeline Layout!");
        return;
    }

    // 3.4 CRIAÇÃO DO GRAPHICS PIPELINE CONSOLIDADO
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2; // Vertex + Fragment
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout;

    // Vinculação obrigatória ao Render Pass criado em passos anteriores
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                  &graphicsPipeline) != VK_SUCCESS) {
        LOGI("Falha ao criar o Graphics Pipeline!");
    } else {
        LOGI("Graphics Pipeline criado com sucesso!");
    }

    // Após a criação do pipeline, os módulos de shader individuais podem ser liberados
    vkDestroyShaderModule(logicalDevice, fragShaderModule, nullptr);
    vkDestroyShaderModule(logicalDevice, vertShaderModule, nullptr);

}

// Como o processador (CPU) e a placa de vídeo (GPU) operam de forma totalmente assíncrona,
// a CPU pode enviar comandos muito mais rápido do que a GPU consegue processá-los.
//Semaphores (Semáforos - VkSemaphore): Sincronizam operações dentro da GPU ou entre a GPU e a Swapchain.
// A CPU não enxerga e não espera por semáforos; eles servem para criar uma ordem de dependência entre tarefas da própria placa de vídeo.
//
//Fences (Cercas - VkFence): Sincronizam a CPU com a GPU.
// Permitem que a CPU pare a sua execução e espere até que a GPU termine uma tarefa específica e sinalize que o espaço de memória está livre.
void createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    // ATENÇÃO ARQUITETURAL: A cerca deve ser criada com a flag SIGNALED.
    // Caso contrário, na primeiríssima volta do Game Loop, a CPU travará para sempre
    // esperando por um quadro anterior que nunca existiu.
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphore) !=
        VK_SUCCESS ||
        vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphore) !=
        VK_SUCCESS ||
        vkCreateFence(logicalDevice, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS) {
        LOGI("Falha ao criar os objetos de sincronização!");
    } else {
        LOGI("Objetos de sincronização criados com sucesso!");
    }
}

void drawFrame() {
    // 1: Aguardar o quadro anterior terminar
    // -----------------------------------------------------------------

    // A CPU bloqueia aqui caso a GPU ainda esteja a processar o ciclo anterior
    vkWaitForFences(logicalDevice, 1, &inFlightFence, VK_TRUE, UINT64_MAX);

    // 2: Adquirir uma imagem da Swapchain
    uint32_t imageIndex;
    // Solicita o índice da próxima imagem disponível. Ativa o semáforo correspondente quando pronta.
    vkAcquireNextImageKHR(logicalDevice, swapchain, UINT64_MAX, imageAvailableSemaphore,
                          VK_NULL_HANDLE, &imageIndex);

    // Apenas após garantir a imagem, a cerca é reiniciada para fechar o acesso da CPU
    vkResetFences(logicalDevice, 1, &inFlightFence);

    // 3: Gravação do Command Buffer

    // Limpa os comandos anteriores do buffer antes de iniciar a nova gravação
    vkResetCommandBuffer(commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        LOGI("Falha ao inciar a gravação do Command Buffer");
        return;
    }

    // Configuração do início do Render Pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    // Vincula o Framebuffer específico da imagem adquirida neste frame
    renderPassInfo.framebuffer = swapchainFrameBuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchainExtent;

    // Define a cor de limpeza do ecrã (Preto opaco: R=0, G=0, B=0, A=1)
    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    // --- Início das instruções para a GPU ---
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Vincula o Pipeline Gráfico imutável (contendo os Shaders)
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    // Comando de desenho do triângulo
    // Parâmetros: Buffer, quantidade de vértices (3), quantidade de instâncias (1), primeiro vértice (0), primeira instância (0)
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);
    // --- Fim das instruções ---

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        LOGI("Falha ao finalizar a gravação do Command Buffer!");
        return;
    }

    // 4: Submissão para a Fila da GPU
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    // Determina que o estágio de escrita de cores deve esperar até a imagem estar disponível
    VkSemaphore waitSemaphores[]{imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    // Vincula o Command Buffer gravado para envio
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    // Semáforo que será sinalizado assim que a GPU terminar a renderização
    VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    // Envia o lote de comandos para a fila de gráficos, vinculando a cerca de controlo
    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
        LOGI("Falha ao submeter o Command Buffer para a fila!");
        return;
    }

    // 5: Apresentação da Imagem no Ecrã
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    // Configura a apresentação para esperar o término da renderização (renderFinishedSemaphore)
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = {swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    // Envia o resultado final para o sistema de exibição do Android
    vkQueuePresentKHR(presentQueue, &presentInfo);
}

void cleanup() {
    if (logicalDevice != VK_NULL_HANDLE) {
        // 1. Aguarda-se a GPU concluir qualquer operação pendente antes de iniciar a destruição
        vkDeviceWaitIdle(logicalDevice);

        // 2. Objetos de Sincronização (Filhos do Logical Device)
        vkDestroySemaphore(logicalDevice, renderFinishedSemaphore, nullptr);
        vkDestroySemaphore(logicalDevice, imageAvailableSemaphore, nullptr);
        vkDestroyFence(logicalDevice, inFlightFence, nullptr);

        // 3. Gerenciador de Comandos
        // (A destruição do Pool libera automaticamente todos os Command Buffers alocados por ele)
        vkDestroyCommandPool(logicalDevice, commandPool, nullptr);

        // 4. Alvos de Renderização (Filhos do RenderPass e das ImageViews)
        for (auto framebuffer: swapchainFrameBuffers) {
            vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
        }

        // 5. Pipeline Gráfico e suas configurações imutáveis
        vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
        vkDestroyRenderPass(logicalDevice, renderPass, nullptr);

        // 6. Lentes de Visualização das Imagens
        for (auto imageView: swapchainImagesViews) {
            vkDestroyImageView(logicalDevice, imageView, nullptr);
        }

        // 7. A Fila de Telas (Depende da Surface e do Device)
        vkDestroySwapchainKHR(logicalDevice, swapchain, nullptr);

        // 8. O Dispositivo Lógico (Deve ser destruído ANTES da Surface e da Instance)
        vkDestroyDevice(logicalDevice, nullptr);
    }

    // 9. Componentes globais da conexão com o sistema operacional
    if (instance != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
    }

    LOGI("Memória do Vulkan liberada");
}

// Ponto de entrada
void android_main(struct android_app *app) {
    LOGI("Aplicativo Vulkan Iniciado!");

    // 1: INICIALIZAÇÃO DA LINHA DE MONTAGEM

    createLogicalDevice();
    createSwapChain(app);
    createImageViews();
    createRenderPass();
    createFrameBuffers();
    createCommandPoolAndBuffer();
    createGraphicsPipeline();
    createSyncObjects();
    LOGI("Toda a infraestrutura do Vulkan foi inicializada com sucesso!");

    // 2: CARREGAMENTO DE DADOS ESTÁTICOS

    // Definição das coordenadas e cores do triângulo
    float vertices[] = {
            0.0f, -0.5f, 1.0f, 0.0f, 0.0f, // Topo (Vermelho)
            0.5f, 0.5f, 0.0f, 1.0f, 0.0f, // Direita (Verde)
            -0.5f, 0.5f, 0.0f, 0.0f, 1.0f  // Esquerda (Azul)
    };

    // O mapeamento de memória transfere estes dados para a GPU apenas uma vez.
    // Nota: A variável 'bufferMemory' deve ser alocada previamente (etapa do Vertex Buffer).
    /* void* data;
    vkMapMemory(logicalDevice, bufferMemory, 0, sizeof(vertices), 0, &data);
    memcpy(data, vertices, sizeof(vertices));
    vkUnmapMemory(logicalDevice, bufferMemory);
    */
    LOGI("Dados dos vértices enviados para a memória da GPU.");

    // 3: GAME LOOP (CICLO PRINCIPAL)
    while (true) {
        int ident;
        int events;
        struct android_app_glue_state *state;
        struct android_poll_source *source;

        // 3.1 Verificação de eventos do sistema operacional Android
        // O timeout '0' impede o travamento do loop, permitindo a renderização contínua.
        while ((ident = ALooper_pollAll(0, nullptr, &events, (void **) &source)) >= 0) {
            if (source != nullptr) {
                source->process(app, source);
            }
            if (app->destroyRequested != 0) {
                LOGI("Encerrando a aplicação...");
                cleanup();
                return;
            }
        }

        // 3.2 RENDERIZAÇÃO DO QUADRO
        // Executado continuamente em cada ciclo livre do processador
        drawFrame();
    }
}