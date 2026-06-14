#version 450

// Recebe a cor do Vertex Shader (a GPU faz o degradê entre as pontas automaticamente!)
layout(location = 0) in vec3 fragColor;

// A cor final que vai para o Framebuffer (e depois para a tela)
layout(location = 0) out vec4 outColor;

void main() {
    // rgba (adicionamos 1.0 para opacidade total)
    outColor = vec4(fragColor, 1.0);
}