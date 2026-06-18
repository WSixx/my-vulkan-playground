#version 450

layout(location = 0) in vec2 inPosition; // X e Y
layout(location = 1) in vec3 inColor;    // R, G e B

// SAÍDAS (Vão para o Fragment Shader para pintar os pixels)
layout(location = 0) out vec3 fragColor;

void main() {
    // gl_Position é uma variável nativa do Vulkan para a posição final do ponto no ecrã (X, Y, Z, W)
    gl_Position = vec4(inPosition, 0.0, 1.0);

    // Passamos a cor recebida do C++ diretamente para o Fragment Shader
    fragColor = inColor;
}