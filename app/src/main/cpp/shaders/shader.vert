#version 450

# Commando para compilar: glslc shader.vert -o shader.vert.spv

// Uniform Buffer
layout(binding = 0) uniform UniformBufferObject {
    float angle;
} ubo;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

void main() {
    // rotação 2D (Seno e Cosseno)
    float s = sin(ubo.angle);
    float c = cos(ubo.angle);

    // nova posição rodada
    float x = inPosition.x * c - inPosition.y * s;
    float y = inPosition.x * s + inPosition.y * c;

    gl_Position = vec4(x, y, 0.0, 1.0);
    fragColor = inColor;
}