#include <vulkan/vulkan.h>           // A API principal do Vulkan
#include <game-activity/native_app_glue/android_native_app_glue.h> // A ponte entre o ciclo de vida do Android e o C++
#include <android/log.h>             //  Logcat
#include <cstring>
#include <jni.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "VulkanApp", __VA_ARGS__))

// Ponto de entrada
void android_main(struct android_app *app) {
    LOGI("Aplicativo Vulkan Iniciado!");

    // =========================================================
    // FASE 1: INICIALIZAÇÃO
    // =========================================================
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


