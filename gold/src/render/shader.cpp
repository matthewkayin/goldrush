#include "shader.h"

#include "defines.h"
#include "core/logger.h"
#include <glad/glad.h>
#include <cstdio>
#include <cstdlib>

bool shader_compile(uint32_t* id, GLenum type, const char* name) {
    // Read the shader file
    char path[128];
    sprintf(path, "%sshader/%s.%s.glsl", RESOURCE_PATH, name, type == GL_VERTEX_SHADER ? "vert" : "frag");

    FILE* shader_file = fopen(path, "rb");
    if (!shader_file) {
        log_error("Unable to open shader file at path %s.");
        return false;
    }

    fseek(shader_file, 0, SEEK_END);
    size_t file_size = ftell(shader_file);
    fseek(shader_file, 0, SEEK_SET);

    char* shader_source = (char*)malloc(file_size + 1);
    fread(shader_source, file_size, 1, shader_file);
    shader_source[file_size] = '\0';

    fclose(shader_file);

    // Compile the shader

    *id = glCreateShader(type);
    glShaderSource(*id, 1, &shader_source, NULL);
    glCompileShader(*id);
    int success;
    glGetShaderiv(*id, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(*id, 512, NULL, info_log);
        log_error("Shader %s failed to compile: %s", path, info_log);
        return false;
    }

    free(shader_source);
    log_info("Shader %s compiled successfully.", path);

    return true;
}

bool shader_load(uint32_t* id, const char* name) {
    // Compile shaders
    GLuint vertex_shader;
    if (!shader_compile(&vertex_shader, GL_VERTEX_SHADER, name)) {
        return false;
    }
    GLuint fragment_shader;
    if (!shader_compile(&fragment_shader, GL_FRAGMENT_SHADER, name)) {
        return false;
    }

    // Link program
    *id = glCreateProgram();
    glAttachShader(*id, vertex_shader);
    glAttachShader(*id, fragment_shader);
    glLinkProgram(*id);
    int success;
    glGetProgramiv(*id, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(*id, 512, NULL, info_log);
        log_error("Failed linking program with name %s: %s", name, info_log);
        return false;
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    log_info("Shader %s linked successfully.", name);

    return true;
}