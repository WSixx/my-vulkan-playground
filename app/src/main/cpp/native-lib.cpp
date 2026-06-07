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
    } else {
        LOGI("Logical Device criado com sucesso!");
    }
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

// Ponto de entrada
void android_main(struct android_app *app) {
    LOGI("Aplicativo Vulkan Iniciado!");

    // INICIALIZAÇÃO
    // Aqui criaríamos a Instance, Physical Device, Logical Device e Swapchain.
    // É nesta fase de inicialização que enviamos os dados estáticos para a GPU
    // Fazemos isso apenas UMA VEZ antes de começar a desenhar.

    float vertices[] = {
            0.0f, -0.5f, 1.0f, 0.0f, 0.0f,
            0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
            -0.5f, 0.5f, 0.0f, 0.0f, 1.0f
    };

    /* void* data;
    vkMapMemory(logicalDevice, bufferMemory, 0, sizeof(vertices), 0, &data);
    memcpy(data, vertices, sizeof(vertices));
    vkUnmapMemory(logicalDevice, bufferMemory);
    */
    LOGI("Memória mapeada e vértices enviados para a GPU.");

    // =========================================================
    // FASE 2: O GAME LOOP
    // =========================================================
    while (true) {
        int ident;
        int events;
        struct android_poll_source *source;

        // 2.1 Verifica os eventos do sistema Android (A "caixa de correio")
        // O timeout '0' significa que ele não vai travar o loop esperando.
        while ((ident = ALooper_pollAll(0, nullptr, &events, (void **) &source)) >= 0) {
            // Processa o evento (ex: mudou a orientação da tela, usuário tocou)
            if (source != nullptr) {
                source->process(app, source);
            }
            // Se o sistema pediu para destruir a Activity, saímos do loop de forma limpa.
            if (app->destroyRequested != 0) {
                LOGI("Encerrando o aplicativo...");
                // TODO: limpar (destruir) os recursos do Vulkan antes de sair
                return;
            }
        }
        // 2.2 RENDERIZAÇÃO
        // Se o app não foi encerrado e a janela está visível, montamos os
        // Command Buffers e pedimos para a GPU desenhar o quadro na tela.
    }
}


