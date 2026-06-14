#version 450

// Escrevemos as posições (x, y) diretamente na GPU
vec2 positions[3] = vec2[](
    vec2(0.0, -0.5), // Topo
    vec2(0.5, 0.5),  // Direita inferior
    vec2(-0.5, 0.5)  // Esquerda inferior
);

// Escrevemos as cores (R, G, B) diretamente na GPU
vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0), // Vermelho
    vec3(0.0, 1.0, 0.0), // Verde
    vec3(0.0, 0.0, 1.0)  // Azul
);

// A variável de saída que enviaremos para o Fragment Shader
layout(location = 0) out vec3 fragColor;

void main() {
    // gl_VertexIndex é uma variável nativa (0, 1, ou 2, já que pedimos para desenhar 3 vértices no vkCmdDraw)
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}