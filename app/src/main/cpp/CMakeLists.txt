# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 4.1.2)

# Declares the project name. The project name can be accessed via ${ PROJECT_NAME},
# Since this is the top level CMakeLists.txt, the project name is also accessible
# with ${CMAKE_PROJECT_NAME} (both CMake variables are in-sync within the top level
# build script scope).
project("myvulkanplayground")

find_package(Vulkan REQUIRED)
# Encontra a biblioteca de Log do Android (para a nossa macro LOGI funcionar)
find_library(log-lib log)
# Encontra a GameActivity (A ponte moderna entre C++ e Kotlin)
find_package(game-activity REQUIRED CONFIG)

add_library(${CMAKE_PROJECT_NAME} SHARED
        # List C/C++ source files with relative paths to this CMakeLists.txt.
        native-lib.cpp)

target_link_libraries(${CMAKE_PROJECT_NAME}
        Vulkan::Vulkan  # A API do Vulkan
        android
        game-activity::game-activity_static
        log)

# Força o Android a não descartar a função de inicialização da GameActivity
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE "-Wl,-u,Java_com_google_androidgamesdk_GameActivity_initializeNativeCode")

# COMPILAÇÃO DOS SHADERS (GLSL -> SPIR-V)
find_program(GLSLC glslc REQUIRED)

set(SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
# Pede ao CMake para calcular o caminho ABSOLUTO e exato da pasta assets
get_filename_component(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets" ABSOLUTE)
set(SHADER_DST_DIR "${ASSETS_DIR}/shaders")

file(MAKE_DIRECTORY ${SHADER_DST_DIR})

add_custom_command(
        OUTPUT ${SHADER_DST_DIR}/shader.vert.spv
        COMMAND ${GLSLC} -c ${SHADER_SRC_DIR}/shader.vert -o ${SHADER_DST_DIR}/shader.vert.spv
        DEPENDS ${SHADER_SRC_DIR}/shader.vert
)

# Comando para compilar o Fragment Shader
add_custom_command(
        OUTPUT ${SHADER_DST_DIR}/shader.frag.spv
        COMMAND ${GLSLC} -c ${SHADER_SRC_DIR}/shader.frag -o ${SHADER_DST_DIR}/shader.frag.spv
        DEPENDS ${SHADER_SRC_DIR}/shader.frag
)

# Cria um "pacote" com os dois shaders gerados
add_custom_target(compile_shaders DEPENDS
        ${SHADER_DST_DIR}/shader.vert.spv
        ${SHADER_DST_DIR}/shader.frag.spv
)

# Avisa ao CMake: "Só compile a biblioteca C++ DEPOIS de compilar os shaders"
add_dependencies(${CMAKE_PROJECT_NAME} compile_shaders)