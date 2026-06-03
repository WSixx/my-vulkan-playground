#include <vulkan/vulkan.h>           // A API principal do Vulkan
#include <vulkan/vulkan_android.h>
#include <game-activity/native_app_glue/android_native_app_glue.h> // A ponte entre o ciclo de vida do Android e o C++
#include <android/log.h>             //  Logcat
#include <cstring>
#include <jni.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "VulkanApp", __VA_ARGS__)) // Helper Log

// Globais
VkInstance instance;             // Conexão com o Vulkan
VkPhysicalDevice physicalDevice; // A placa de vídeo física do celular
VkDevice logicalDevice;          // Sessão de uso (Device Lógico)
VkSwapchainKHR swapchain;        // A Fila de telas
VkSurfaceKHR surface;

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

    VkExtent2D imageExtend;
    imageExtend.width = (uint32_t) width;
    imageExtend.height = (uint32_t) height;

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
    swapchainInfo.imageExtent = imageExtend;

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


