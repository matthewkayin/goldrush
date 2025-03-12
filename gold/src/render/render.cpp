#include "render.h"

#define STB_IMAGE_IMPLEMENTATION
#define MAX_BATCH_VERTICES 32768

#include "core/logger.h"
#include "core/asserts.h"
#include "math/gmath.h"
#include "math/mat4.h"
#include "resource/sprite.h"
#include "shader.h"
#include <glad/glad.h>
#include <cstdio>
#include <cstring>
#include <stb/stb_image.h>
#include <vector>

struct sprite_vertex_t {
    float position[3];
    float tex_coord[2];
};

struct sprite_t {
    uint32_t texture;
    ivec2 size;
    ivec2 frame_size;
    ivec2 frame_count;
};

struct render_state_t {
    SDL_Window* window;
    SDL_GLContext context;

    GLuint screen_shader;
    GLuint screen_framebuffer;
    GLuint screen_texture;
    GLuint screen_vao;

    GLuint sprite_shader;
    GLuint sprite_vao;
    GLuint sprite_vbo;
    std::vector<sprite_vertex_t> sprite_vertices[SPRITE_COUNT];
    sprite_t sprites[SPRITE_COUNT];
};
static render_state_t state;

void render_init_quad_vao();
void render_init_sprite_vao();
bool render_init_screen_framebuffer();

bool render_load_sprite(sprite_t* sprite, const sprite_params_t params);

bool render_init(SDL_Window* window) {
    state.window = window;

    // Set GL version
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_LoadLibrary(NULL);

    // Create GL context
    state.context = SDL_GL_CreateContext(state.window);
    if (state.context == NULL) {
        log_error("Error creating GL context: %s", SDL_GetError());
        return false;
    }

    // Setup GLAD
    gladLoadGLLoader(SDL_GL_GetProcAddress);
    if (glGenVertexArrays == NULL) {
        log_error("Error loading OpenGL.");
        return false;
    }

    // Configure STBI
    stbi_set_flip_vertically_on_load(true);

    render_init_quad_vao();
    render_init_sprite_vao();
    if (!render_init_screen_framebuffer()) {
        return false;
    }

    // Load screen shader
    if (!shader_load(&state.screen_shader, "screen")) {
        return false;
    }
    glUseProgram(state.screen_shader);
    glUniform1ui(glGetUniformLocation(state.screen_shader, "screen_texture"), 0);

    ivec2 screen_size = ivec2(SCREEN_WIDTH, SCREEN_HEIGHT);
    glUniform2iv(glGetUniformLocation(state.screen_shader, "screen_size"), 1, &screen_size.x);

    ivec2 window_size;
    SDL_GetWindowSize(state.window, &window_size.x, &window_size.y);
    glUniform2iv(glGetUniformLocation(state.screen_shader, "window_size"), 1, &window_size.x);

    // Load sprite shader
    if (!shader_load(&state.sprite_shader, "sprite")) {
        return false;
    }
    glUseProgram(state.sprite_shader);
    glUniform1ui(glGetUniformLocation(state.sprite_shader, "sprite_texture"), 0);

    mat4 projection = mat4_ortho(0.0f, (float)SCREEN_WIDTH, 0.0f, (float)SCREEN_HEIGHT, 0.0f, 100.0f);
    glUniformMatrix4fv(glGetUniformLocation(state.sprite_shader, "projection"), 1, GL_FALSE, projection.data);

    // Load sprites
    for (int name = 0; name < SPRITE_COUNT; name++) {
        if (!render_load_sprite(&state.sprites[name], resource_get_sprite_params((sprite_name)name))) {
            return false;
        }
    }

    log_info("Initialized renderer. Vendor: %s. Renderer: %s. Version: %s.", glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION));

    return true;
}

void render_init_quad_vao() {
    float quad_vertices[] = {
        -1.0f, 1.0f, 0.0f, 1.0f,
        1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
        
        -1.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, -1.0f, 1.0f, 0.0f
    };
    GLuint quad_vbo;

    glGenVertexArrays(1, &state.screen_vao);
    glGenBuffers(1, &quad_vbo);
    glBindVertexArray(state.screen_vao);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), &quad_vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void render_init_sprite_vao() {
    glGenVertexArrays(1, &state.sprite_vao);
    glGenBuffers(1, &state.sprite_vbo);
    glBindVertexArray(state.sprite_vao);
    glBindBuffer(GL_ARRAY_BUFFER, state.sprite_vbo);
    glBufferData(GL_ARRAY_BUFFER, MAX_BATCH_VERTICES * sizeof(sprite_vertex_t), NULL, GL_DYNAMIC_DRAW);

    // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    // tex coord
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

bool render_init_screen_framebuffer() {
    glGenFramebuffers(1, &state.screen_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, state.screen_framebuffer);

    glGenTextures(1, &state.screen_texture);
    glBindTexture(GL_TEXTURE_2D, state.screen_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    GLuint screen_depthbuffer;
    glGenTextures(1, &screen_depthbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, screen_depthbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCREEN_WIDTH, SCREEN_HEIGHT);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, screen_depthbuffer);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, state.screen_texture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        log_error("Screen framebuffer incomplete.");
        return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}

bool render_load_sprite(sprite_t* sprite, const sprite_params_t params) {
    char path[128];
    sprintf(path, "%s%s", RESOURCE_PATH, params.path);

    log_info("Loading sprite %s", path);

    int number_of_components;
    stbi_uc* data = stbi_load(path, &sprite->size.x, &sprite->size.y, &number_of_components, 0);
    if (!data) {
        log_error("Could not load sprite %s", path);
        return false;
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
        return false;
    }

    glGenTextures(1, &sprite->texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sprite->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, texture_format, sprite->size.x, sprite->size.y, GL_FALSE, texture_format, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);

    sprite->frame_count = ivec2(params.hframes, params.vframes);
    sprite->frame_size = ivec2(sprite->size.x / sprite->frame_count.x, sprite->size.y / sprite->frame_count.y);
    GOLD_ASSERT(sprite->size.x % sprite->frame_size.x == 0 && sprite->size.y % sprite->frame_size.y == 0);

    return true;
}

void render_quit() {
    SDL_GL_DeleteContext(state.context);
    log_info("Quit renderer.");
}

void render_prepare_frame() {
    glBindFramebuffer(GL_FRAMEBUFFER, state.screen_framebuffer);
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void render_present_frame() {
    render_sprite(SPRITE_DECORATION, ivec2(0, 0), ivec2(0, 0), 1);
    render_sprite(SPRITE_DECORATION, ivec2(0, 0), ivec2(16, 0), 1, RENDER_SPRITE_FLIP_H);

    // Buffer sprite batch data
    glBindBuffer(GL_ARRAY_BUFFER, state.sprite_vbo);
    glUseProgram(state.sprite_shader);
    glBindVertexArray(state.sprite_vao);
    glActiveTexture(GL_TEXTURE0);
    for (int name = 0; name < SPRITE_COUNT; name++) {
        if (state.sprite_vertices[name].empty()) {
            continue;
        }

        glBufferSubData(GL_ARRAY_BUFFER, 0, state.sprite_vertices[name].size() * sizeof(sprite_vertex_t), &state.sprite_vertices[name][0]);

        // Render sprite batch
        glBindTexture(GL_TEXTURE_2D, state.sprites[name].texture);
        glDrawArrays(GL_TRIANGLES, 0, state.sprite_vertices[name].size());
        glBindVertexArray(0);
        state.sprite_vertices[name].clear();
    }

    // Switch to default framebuffer
    ivec2 window_size;
    SDL_GetWindowSize(state.window, &window_size.x, &window_size.y);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, window_size.x, window_size.y);
    glBlendFunc(GL_ONE, GL_ZERO);
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Render screen texture onto window
    glUseProgram(state.screen_shader);
    glBindVertexArray(state.screen_vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, state.screen_texture);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    SDL_GL_SwapWindow(state.window);
}

void render_sprite(sprite_name name, ivec2 frame, ivec2 position, int z_index, uint32_t options, uint8_t recolor) {
    bool flip_h = (options & RENDER_SPRITE_FLIP_H) == RENDER_SPRITE_FLIP_H;
    bool centered = (options & RENDER_SPRITE_CENTERED) == RENDER_SPRITE_CENTERED;
    bool cull = !((options & RENDER_SPRITE_NO_CULL) == RENDER_SPRITE_NO_CULL);

    if (centered) {
        position -= state.sprites[name].frame_size / 2;
    }

    float position_left = (float)position.x;
    float position_right = (float)(position.x + state.sprites[name].frame_size.x);
    float position_top = (float)(SCREEN_HEIGHT - position.y);
    float position_bottom = (float)(SCREEN_HEIGHT - (position.y + state.sprites[name].frame_size.y));

    if (cull) {
        if (position_right <= 0.0f || position_left >= (float)SCREEN_WIDTH || position_top <= 0.0f || position_bottom >= (float)SCREEN_HEIGHT) {
            return;
        }
    }

    float frame_size_x = 16.0f / 80.0f;
    float frame_size_y = 16.0f / 16.0f;
    float tex_coord_left = frame_size_x * frame.x;
    float tex_coord_right = tex_coord_left + frame_size_x;
    float tex_coord_bottom = frame_size_y * frame.y;
    float tex_coord_top = tex_coord_bottom + frame_size_y;

    if (flip_h) {
        float temp = tex_coord_left;
        tex_coord_left = tex_coord_right;
        tex_coord_right = temp;
    }

    state.sprite_vertices[name].push_back((sprite_vertex_t) {
        .position = { position_left, position_top, (float)-z_index },
        .tex_coord = { tex_coord_left, tex_coord_top }
    });
    state.sprite_vertices[name].push_back((sprite_vertex_t) {
        .position = { position_right, position_bottom, (float)-z_index },
        .tex_coord = { tex_coord_right, tex_coord_bottom }
    });
    state.sprite_vertices[name].push_back((sprite_vertex_t) {
        .position = { position_left, position_bottom, (float)-z_index },
        .tex_coord = { tex_coord_left, tex_coord_bottom }
    });
    state.sprite_vertices[name].push_back((sprite_vertex_t) {
        .position = { position_left, position_top, (float)-z_index },
        .tex_coord = { tex_coord_left, tex_coord_top }
    });
    state.sprite_vertices[name].push_back((sprite_vertex_t) {
        .position = { position_right, position_top, (float)-z_index },
        .tex_coord = { tex_coord_right, tex_coord_top }
    });
    state.sprite_vertices[name].push_back((sprite_vertex_t) {
        .position = { position_right, position_bottom, (float)-z_index },
        .tex_coord = { tex_coord_right, tex_coord_bottom }
    });
}