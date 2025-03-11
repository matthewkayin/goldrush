#include "sprite.h"

#include "defines.h"
#include "core/logger.h"
#include <cstdio>
#include <stb/stb_image.h>
#include <glad/glad.h>

sprite_t* sprite_load(const char* name) {
    char path[128];
    sprintf(path, "%ssprite/%s.png", RESOURCE_PATH, name);

    int width, height, number_of_components;
    stbi_uc* data = stbi_load(path, &width, &height, &number_of_components, 0);
    if (!data) {
        log_error("Could not load sprite %s", path);
        return NULL;
    }

    GLenum texture_format;
    if (number_of_components == 1) {
        texture_format = GL_RED;
    } else if (number_of_components == 3) {
        texture_format = GL_RGB;
    } else if (number_of_components == 4) {
        texture_format = GL_RGBA;
    } else {
        log_error("Texture format for sprite %s recognized. Number of components: %i", path, number_of_components);
        return NULL;
    }

    sprite_t* sprite = (sprite_t*)malloc(sizeof(sprite));
    glGenTextures(1, &sprite->texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sprite->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, texture_format, width, height, GL_FALSE, texture_format, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);

    return sprite;
}

void sprite_free(sprite_t* sprite) {
    free(sprite);
}