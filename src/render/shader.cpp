#include "shader.h"

#include "defines.h"
#include "core/logger.h"
#include "core/filesystem.h"
#include <glad/glad.h>
#include <cstdio>
#include <cstdlib>

bool shader_compile(uint32_t* id, GLenum type, const char* path_suffix) {
    // Read the shader file
    char subpath[128];
    sprintf(subpath, "shader/%s", path_suffix);
    char path[256];
    filesystem_get_resource_path(path, subpath);

    FILE* shader_file = fopen(path, "rb");
    if (!shader_file) {
        log_error("Unable to open shader file at path %s.", path);
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

    return true;
}

bool shader_load(uint32_t* id, const char* vertex_path, const char* fragment_path) {
    // Compile shaders
    GLuint vertex_shader;
    if (!shader_compile(&vertex_shader, GL_VERTEX_SHADER, vertex_path)) {
        return false;
    }
    GLuint fragment_shader;
    if (!shader_compile(&fragment_shader, GL_FRAGMENT_SHADER, fragment_path)) {
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
        log_error("Failed linking program. vertex path: %s. fragment path: %s. error: %s", vertex_path, fragment_path, info_log);
        return false;
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    return true;
}