#include "render.h"

#define STB_IMAGE_IMPLEMENTATION
#define MAX_BATCH_VERTICES 32768
#define MAX_TEXT_CHARS 128
#define FONT_GLYPH_COUNT 96
#define FONT_FIRST_CHAR 32 // space
#define MINIMAP_TEXTURE_WIDTH 512
#define MINIMAP_TEXTURE_HEIGHT 256

#include "core/logger.h"
#include "core/asserts.h"
#include "math/gmath.h"
#include "math/mat4.h"
#include "resource/sprite.h"
#include "resource/font.h"
#include "shader.h"
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <glad/glad.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>

struct sprite_vertex_t {
    float position[3];
    float tex_coord[3];
};

struct font_vertex_t {
    float position[3];
    float tex_coord[2];
};

struct atlas_t {
    uint32_t texture;
    ivec2 texture_size; 
    ivec2 image_size;
};

struct font_glyph_t {
    int bearing_x;
    int bearing_y;
    int advance;
};

struct font_t {
    uint32_t texture;
    int atlas_width;
    int atlas_height;
    int glyph_max_width;
    int glyph_max_height;
    font_glyph_t glyphs[FONT_GLYPH_COUNT];
};

struct player_color_t {
    const char* name;
    SDL_Color skin_color;
    SDL_Color clothes_color;
};

static const SDL_Color RECOLOR_CLOTHES_REF = { .r = 255, .g = 0, .b = 255, .a = 255 };
static const SDL_Color RECOLOR_SKIN_REF = { .r = 123, .g = 174, .b = 121, .a = 255 };
static const player_color_t PLAYER_COLORS[MAX_PLAYERS] = {
    (player_color_t) {
        .name = "Blue",
        .skin_color = (SDL_Color) { .r = 125, .g = 181, .b = 164, .a = 255 },
        .clothes_color = (SDL_Color) { .r = 92, .g = 132, .b = 153, .a = 255 }
    },
    (player_color_t) {
        .name = "Red",
        .skin_color = (SDL_Color) { .r = 219, .g = 151, .b = 114, .a = 255 },
        .clothes_color = (SDL_Color) { .r = 186, .g = 97, .b = 95, .a = 255 }
    },
    (player_color_t) {
        .name = "Green",
        .skin_color = (SDL_Color) { .r = 123, .g = 174, .b = 121, .a = 255 },
        .clothes_color = (SDL_Color) { .r = 77, .g = 135, .b = 115, .a = 255 },
    },
    (player_color_t) {
        .name = "Purple",
        .skin_color = (SDL_Color) { .r = 184, .g = 169, .b = 204, .a = 255 },
        .clothes_color = (SDL_Color) { .r = 144, .g = 119, .b = 153, .a = 255 },
    }
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
    std::vector<sprite_vertex_t> sprite_vertices;
    atlas_t atlases[ATLAS_COUNT];

    GLuint minimap_texture;
    uint32_t minimap_texture_pixels[MINIMAP_TEXTURE_WIDTH * MINIMAP_TEXTURE_HEIGHT];
    uint32_t minimap_pixel_values[MINIMAP_PIXEL_COUNT];

    GLuint font_shader;
    GLint font_shader_font_color_location;
    GLuint font_vao;
    GLuint font_vbo;
    font_t fonts[FONT_COUNT];

    GLuint line_shader;
    GLint line_shader_color_location;
    GLuint line_vao;
    GLuint line_vbo;
};
static render_state_t state;

void render_init_quad_vao();
void render_init_sprite_vao();
void render_init_font_vao();
void render_init_line_vao();
void render_init_minimap_texture();
bool render_init_screen_framebuffer();

bool render_load_atlas(atlas_t* atlas, const atlas_params_t params);
bool render_load_font(font_t* font,  const font_params_t params);

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

    render_init_quad_vao();
    render_init_sprite_vao();
    render_init_font_vao();
    render_init_line_vao();
    render_init_minimap_texture();
    if (!render_init_screen_framebuffer()) {
        return false;
    }

    // Load screen shader
    if (!shader_load(&state.screen_shader, "screen.vert.glsl", "screen.frag.glsl")) {
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
    if (!shader_load(&state.sprite_shader, "sprite.vert.glsl", "sprite.frag.glsl")) {
        return false;
    }
    glUseProgram(state.sprite_shader);
    for (uint32_t index = 0; index < 16; index++) {
        char uniform_name[32];
        sprintf(uniform_name, "sprite_textures[%u]", index);
        glUniform1ui(glGetUniformLocation(state.sprite_shader, uniform_name), index);
    }
    mat4 projection = mat4_ortho(0.0f, (float)SCREEN_WIDTH, 0.0f, (float)SCREEN_HEIGHT, 0.0f, 100.0f);
    glUniformMatrix4fv(glGetUniformLocation(state.sprite_shader, "projection"), 1, GL_FALSE, projection.data);

    // Load text shader
    if (!shader_load(&state.font_shader, "text.vert.glsl", "text.frag.glsl")) {
        return false;
    }
    glUseProgram(state.font_shader);
    glUniform1ui(glGetUniformLocation(state.font_shader, "font_texture"), 0);
    glUniformMatrix4fv(glGetUniformLocation(state.font_shader, "projection"), 1, GL_FALSE, projection.data);
    state.font_shader_font_color_location = glGetUniformLocation(state.font_shader, "font_color");

    // Load line shader
    if (!shader_load(&state.line_shader, "line.vert.glsl", "line.frag.glsl")) {
        return false;
    }
    glUseProgram(state.line_shader);
    glUniformMatrix4fv(glGetUniformLocation(state.line_shader, "projection"), 1, GL_FALSE, projection.data);
    state.line_shader_color_location = glGetUniformLocation(state.line_shader, "color");

    // Load sprites
    for (int name = 0; name < ATLAS_COUNT; name++) {
        if (!render_load_atlas(&state.atlases[name], resource_get_atlas_params((atlas_name)name))) {
            return false;
        }
    }

    // Load fonts
    for (int name = 0; name < FONT_COUNT; name++) {
        if (!render_load_font(&state.fonts[name], resource_get_font_params((font_name)name))) {
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
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    // tex coord
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void render_init_font_vao() {
    glGenVertexArrays(1, &state.font_vao);
    glGenBuffers(1, &state.font_vbo);
    glBindVertexArray(state.font_vao);
    glBindBuffer(GL_ARRAY_BUFFER, state.font_vbo);
    glBufferData(GL_ARRAY_BUFFER, MAX_TEXT_CHARS * 6 * sizeof(font_vertex_t), NULL, GL_DYNAMIC_DRAW);

    // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    // tex coord
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void render_init_line_vao() {
    glGenVertexArrays(1, &state.line_vao);
    glGenBuffers(1, &state.line_vbo);
    glBindVertexArray(state.line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, state.line_vbo);
    glBufferData(GL_ARRAY_BUFFER, 24 * sizeof(float), NULL, GL_DYNAMIC_DRAW);

    // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void render_init_minimap_texture() {
    glGenTextures(1, &state.minimap_texture);
    glBindTexture(GL_TEXTURE_2D, state.minimap_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, MINIMAP_TEXTURE_WIDTH, MINIMAP_TEXTURE_HEIGHT, GL_FALSE, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, 0);

    SDL_PixelFormat* format = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888);
    state.minimap_pixel_values[MINIMAP_PIXEL_TRANSPARENT] = SDL_MapRGBA(format, 0, 0, 0, 0);
    state.minimap_pixel_values[MINIMAP_PIXEL_OFFBLACK] = SDL_MapRGBA(format, 40, 37, 45, 255);
    state.minimap_pixel_values[MINIMAP_PIXEL_OFFBLACK_TRANSPARENT] = SDL_MapRGBA(format, 40, 37, 45, 128);
    state.minimap_pixel_values[MINIMAP_PIXEL_WHITE] = SDL_MapRGBA(format, 255, 255, 255, 255);
    for (int player = 0; player < MAX_PLAYERS; player++) {
        state.minimap_pixel_values[MINIMAP_PIXEL_PLAYER0 + player] = SDL_MapRGBA(format, PLAYER_COLORS[player].clothes_color.r, PLAYER_COLORS[player].clothes_color.g, PLAYER_COLORS[player].clothes_color.b, PLAYER_COLORS[player].clothes_color.a);
    }
    SDL_FreeFormat(format);

    memset(state.minimap_texture_pixels, 0, sizeof(state.minimap_texture_pixels));
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

bool render_load_atlas(atlas_t* atlas, const atlas_params_t params) {
    char path[128];
    sprintf(path, "%ssprite/%s", RESOURCE_PATH, params.path);

    log_info("Loading sprite %s", path);

    SDL_Surface* sprite_surface = IMG_Load(path);
    if (sprite_surface == NULL) {
        log_error("Error loading surface for sprite %s: %s", path, IMG_GetError());
        return false;
    }
    GOLD_ASSERT(sprite_surface->format->BytesPerPixel == 4);

    atlas->image_size = ivec2(sprite_surface->w, sprite_surface->h);

    if (params.strategy == ATLAS_IMPORT_RECOLOR || params.strategy == ATLAS_IMPORT_RECOLOR_AND_LOW_ALPHA) {
        // Create a surface big enough to hold the recolor atlas
        int sprite_width = next_largest_power_of_two(sprite_surface->w);
        int sprite_height = next_largest_power_of_two(sprite_surface->h * MAX_PLAYERS);
        SDL_Surface* recolor_surface = SDL_CreateRGBSurfaceWithFormat(0, sprite_width, sprite_height, 32, sprite_surface->format->format);
        if (recolor_surface == NULL) {
            log_error("Error creating recolor surface for sprite %s: %s", path, SDL_GetError());
            return false;
        }

        // Get the reference pixels into a packed byte
        uint32_t clothes_reference_pixel = SDL_MapRGBA(sprite_surface->format, RECOLOR_CLOTHES_REF.r, RECOLOR_CLOTHES_REF.g, RECOLOR_CLOTHES_REF.b, RECOLOR_CLOTHES_REF.a);
        uint32_t skin_reference_pixel = SDL_MapRGBA(sprite_surface->format, RECOLOR_SKIN_REF.r, RECOLOR_SKIN_REF.g, RECOLOR_SKIN_REF.b, RECOLOR_SKIN_REF.a);

        // Lock the surface so that we can edit pixel values
        SDL_LockSurface(recolor_surface);
        uint32_t* recolor_surface_pixels = (uint32_t*)recolor_surface->pixels;
        uint32_t* sprite_surface_pixels = (uint32_t*)sprite_surface->pixels;

        // Copy the original sprite onto our recolor atlas 4 times, once for each player color
        for (int recolor_id = 0; recolor_id < MAX_PLAYERS; recolor_id++) {
            // Get the replacement pixel bytes from the player color
            uint32_t clothes_replacement_pixel = SDL_MapRGBA(recolor_surface->format, PLAYER_COLORS[recolor_id].clothes_color.r, PLAYER_COLORS[recolor_id].clothes_color.g, PLAYER_COLORS[recolor_id].clothes_color.b, PLAYER_COLORS[recolor_id].clothes_color.a);
            uint32_t skin_replacement_pixel = SDL_MapRGBA(recolor_surface->format, PLAYER_COLORS[recolor_id].skin_color.r, PLAYER_COLORS[recolor_id].skin_color.g, PLAYER_COLORS[recolor_id].skin_color.b, PLAYER_COLORS[recolor_id].skin_color.a);

            // Loop through each pixel of the original sprite
            for (int y = 0; y < sprite_surface->h; y++) {
                for (int x = 0; x < sprite_surface->w; x++) {
                    // Determine which source pixel to use
                    uint32_t source_pixel = sprite_surface_pixels[(y * sprite_surface->w) + x];
                    if (source_pixel == clothes_reference_pixel) {
                        source_pixel = clothes_replacement_pixel;
                    } else if (source_pixel == skin_reference_pixel) {
                        source_pixel = skin_replacement_pixel;
                    }
                    if (params.strategy == ATLAS_IMPORT_RECOLOR_AND_LOW_ALPHA) {
                        uint8_t r, g, b, a;
                        SDL_GetRGBA(source_pixel, recolor_surface->format, &r, &g, &b, &a);
                        if (a != 0) {
                            a = 200;
                        }
                        source_pixel = SDL_MapRGBA(recolor_surface->format, r, g, b, a);
                    }

                    // Put the source pixel onto the recolor surface
                    recolor_surface_pixels[((y + (sprite_surface->h * recolor_id)) * recolor_surface->w) + x] = source_pixel;
                }
            }
        }

        SDL_UnlockSurface(recolor_surface);

        // Handoff the recolor surface into the sprite surface variable
        // Allows the rest of the sprite loading to work the same as with normal sprites
        SDL_FreeSurface(sprite_surface);
        sprite_surface = recolor_surface;
    }

    // Flip the surface vertically
    SDL_LockSurface(sprite_surface);
    int sprite_surface_pitch = sprite_surface->pitch;
    uint8_t temp[sprite_surface_pitch];
    uint8_t* sprite_surface_pixels = (uint8_t*)sprite_surface->pixels;

    for (int row = 0; row < sprite_surface->h / 2; ++row) {
        uint8_t* row1 = sprite_surface_pixels + (row * sprite_surface_pitch);
        uint8_t* row2 = sprite_surface_pixels + ((sprite_surface->h - row - 1) * sprite_surface_pitch);

        memcpy(temp, row1, sprite_surface_pitch);
        memcpy(row1, row2, sprite_surface_pitch);
        memcpy(row2, temp, sprite_surface_pitch);
    }
    SDL_UnlockSurface(sprite_surface);

    atlas->texture_size = ivec2(sprite_surface->w, sprite_surface->h);

    // Create the GL texture
    GLenum texture_format = GL_RGBA;
    glGenTextures(1, &atlas->texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, texture_format, sprite_surface->w, sprite_surface->h, GL_FALSE, texture_format, GL_UNSIGNED_BYTE, sprite_surface->pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, 0);

    SDL_FreeSurface(sprite_surface);

    return true;
}

bool render_load_font(font_t* font, const font_params_t params) {
    static const SDL_Color COLOR_WHITE = { 255, 255, 255, 255 };

    char path[128];
    sprintf(path, "%sfont/%s", RESOURCE_PATH, params.path);

    log_info("Loading font %s", path);

    // Open the font
    TTF_Font* ttf_font = TTF_OpenFont(path, params.size);
    if (ttf_font == NULL) {
        log_error("Unable to open font %s: %s", path, TTF_GetError());
        return false;
    }

    // Render each glyph to a surface
    SDL_Surface* glyphs[FONT_GLYPH_COUNT];
    int glyph_max_width = 0;
    int glyph_max_height = 0;
    for (int glyph_index = 0; glyph_index < FONT_GLYPH_COUNT; glyph_index++) {
        char text[2] = { (char)(FONT_FIRST_CHAR + glyph_index), '\0'};
        glyphs[glyph_index] = TTF_RenderText_Solid(ttf_font, text, COLOR_WHITE);
        if (glyphs[glyph_index] == NULL) {
            log_error("Error rendering glyph %s for font %s: %s", text, path, TTF_GetError());
            return false;
        }

        glyph_max_width = std::max(glyph_max_width, glyphs[glyph_index]->w);
        glyph_max_height = std::max(glyph_max_height, glyphs[glyph_index]->h);
    }

    // Render each surface glyph onto a single atlas surface
    int atlas_width = next_largest_power_of_two(glyph_max_width * FONT_GLYPH_COUNT);
    int atlas_height = next_largest_power_of_two(glyph_max_height);
    SDL_Surface* atlas_surface = SDL_CreateRGBSurface(0, atlas_width, atlas_height, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
    for (int glyph_index = 0; glyph_index < FONT_GLYPH_COUNT; glyph_index++) {
        SDL_Rect dest_rect = (SDL_Rect) { .x = glyph_max_width * glyph_index, .y = 0, .w = glyphs[glyph_index]->w, .h = glyphs[glyph_index]->h };
        SDL_BlitSurface(glyphs[glyph_index], NULL, atlas_surface, &dest_rect);
    }

    // Render the atlas surface onto a texture
    glGenTextures(1, &font->texture);
    glBindTexture(GL_TEXTURE_2D, font->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlas_width, atlas_height, 0, GL_BGRA, GL_UNSIGNED_BYTE, atlas_surface->pixels);

    // Finish filling out the font struct
    font->atlas_width = atlas_width;
    font->atlas_height = atlas_height;
    font->glyph_max_width = glyph_max_width;
    font->glyph_max_height = glyph_max_height;
    for (int glyph_index = 0; glyph_index < FONT_GLYPH_COUNT; glyph_index++) {
        TTF_GlyphMetrics(ttf_font, FONT_FIRST_CHAR + glyph_index, &font->glyphs[glyph_index].bearing_x, NULL, NULL, &font->glyphs[glyph_index].bearing_y, &font->glyphs[glyph_index].advance);
    }

    // Free all the resources
    for (int glyph_index = 0; glyph_index < FONT_GLYPH_COUNT; glyph_index++) {
        SDL_FreeSurface(glyphs[glyph_index]);
    }
    SDL_FreeSurface(atlas_surface);
    TTF_CloseFont(ttf_font);

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

void render_sprite_vertices() {
    if (state.sprite_vertices.empty()) {
        return;
    }
    GOLD_ASSERT(state.sprite_vertices.size() <= MAX_BATCH_VERTICES);

    // Buffer sprite batch data
    glBindBuffer(GL_ARRAY_BUFFER, state.sprite_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, state.sprite_vertices.size() * sizeof(sprite_vertex_t), &state.sprite_vertices[0]);

    // Render sprite batch
    glBindVertexArray(state.sprite_vao);
    glUseProgram(state.sprite_shader);
    for (int atlas = 0; atlas < ATLAS_COUNT; atlas++) {
        glActiveTexture(GL_TEXTURE0 + atlas);
        glBindTexture(GL_TEXTURE_2D, state.atlases[atlas].texture);
    }

    glDrawArrays(GL_TRIANGLES, 0, state.sprite_vertices.size());
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void render_present_frame() {
    render_ninepatch(SPRITE_UI_FRAME, (rect_t) { .x = 16, .y = 16, .w = 128, .h = 64 }, 0);
    render_sprite_vertices();

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

void render_sprite(sprite_name name, ivec2 frame, ivec2 position, int z_index, uint32_t options, int recolor_id) {
    sprite_info_t sprite_info = resource_get_sprite_info(name);

    rect_t src_rect = (rect_t) { 
        .x = sprite_info.atlas_x + (frame.x * sprite_info.frame_width), 
        .y = sprite_info.atlas_y + ((recolor_id == RECOLOR_NONE ? 0 : recolor_id) * state.atlases[sprite_info.atlas].image_size.y) + (frame.y * sprite_info.frame_height), 
        .w = sprite_info.frame_width,
        .h = sprite_info.frame_height
    };
    rect_t dst_rect = (rect_t) { 
        .x = position.x, 
        .y = position.y, 
        .w = sprite_info.frame_width,
        .h = sprite_info.frame_height
    };
    render_atlas(sprite_info.atlas, src_rect, dst_rect, z_index, options);
}

void render_ninepatch(sprite_name sprite, rect_t rect, int z_index) {
    sprite_info_t sprite_info = resource_get_sprite_info(sprite);
    GOLD_ASSERT(rect.w > sprite_info.frame_width * 2 && rect.h > sprite_info.frame_height * 2);

    rect_t src_rect = (rect_t) {
        .x = sprite_info.atlas_x, 
        .y = sprite_info.atlas_y,
        .w = sprite_info.frame_width,
        .h = sprite_info.frame_height
    };
    rect_t dst_rect = (rect_t) {
        .x = rect.x,
        .y = rect.y,
        .w = sprite_info.frame_width,
        .h = sprite_info.frame_height
    };

    // Top left
    render_atlas(sprite_info.atlas, src_rect, dst_rect, z_index, RENDER_SPRITE_NO_CULL);

    // Top right
    src_rect.x = sprite_info.atlas_x + (2 * sprite_info.frame_width);
    dst_rect.x = rect.x + rect.w - sprite_info.frame_width;
    render_atlas(sprite_info.atlas, src_rect, dst_rect, z_index, RENDER_SPRITE_NO_CULL);

    // Bottom left
    src_rect.x = sprite_info.atlas_x;
    src_rect.y = sprite_info.atlas_y + (2 * sprite_info.frame_height);
    dst_rect.x = rect.x;
    dst_rect.y = rect.y + rect.h - sprite_info.frame_height;
    render_atlas(sprite_info.atlas, src_rect, dst_rect, z_index, RENDER_SPRITE_NO_CULL);

    // Bottom right
    src_rect.x = sprite_info.atlas_x + (2 * sprite_info.frame_width);
    dst_rect.x = rect.x + rect.w - sprite_info.frame_width;
    render_atlas(sprite_info.atlas, src_rect, dst_rect, z_index, RENDER_SPRITE_NO_CULL);

    // Top edge
    src_rect.x = sprite_info.atlas_x + sprite_info.frame_width;
    src_rect.y = sprite_info.atlas_y;
    dst_rect.x = rect.x + sprite_info.frame_width;
    dst_rect.y = rect.y;
    dst_rect.w = (rect.x + rect.w - sprite_info.frame_width) - dst_rect.x;
    render_atlas(sprite_info.atlas, src_rect, dst_rect, z_index, RENDER_SPRITE_NO_CULL);

    // Bottom edge
    src_rect.y = sprite_info.atlas_y + (2 * sprite_info.frame_height);
    dst_rect.y = (rect.y + rect.h - sprite_info.frame_height);
    render_atlas(sprite_info.atlas, src_rect, dst_rect, z_index, RENDER_SPRITE_NO_CULL);

    // Left edge
    src_rect.x = sprite_info.atlas_x;
    src_rect.y = sprite_info.atlas_y + sprite_info.frame_height;
    dst_rect.x = rect.x;
    dst_rect.w = sprite_info.frame_width;
    dst_rect.y = rect.y + sprite_info.frame_height;
    dst_rect.h = (rect.y + rect.h - sprite_info.frame_height) - dst_rect.y;
    render_atlas(sprite_info.atlas, src_rect, dst_rect, z_index, RENDER_SPRITE_NO_CULL);

    // Right edge
    src_rect.x = sprite_info.atlas_x + (2 * sprite_info.frame_width);
    dst_rect.x = rect.x + rect.w - sprite_info.frame_width;
    render_atlas(sprite_info.atlas, src_rect, dst_rect, z_index, RENDER_SPRITE_NO_CULL);

    // Center
    src_rect.x = sprite_info.atlas_x + sprite_info.frame_width;
    src_rect.y = sprite_info.atlas_y + sprite_info.frame_height;
    dst_rect.x = rect.x + sprite_info.frame_width;
    dst_rect.y = rect.y + sprite_info.frame_height;
    dst_rect.w = (rect.x + rect.w - sprite_info.frame_width) - dst_rect.x;
    dst_rect.h = (rect.y + rect.h - sprite_info.frame_height) - dst_rect.y;
    render_atlas(sprite_info.atlas, src_rect, dst_rect, z_index, RENDER_SPRITE_NO_CULL);
}

void render_atlas(atlas_name atlas, rect_t src_rect, rect_t dst_rect, int z_index, uint32_t options) {
    bool flip_h = (options & RENDER_SPRITE_FLIP_H) == RENDER_SPRITE_FLIP_H;
    bool centered = (options & RENDER_SPRITE_CENTERED) == RENDER_SPRITE_CENTERED;
    bool cull = !((options & RENDER_SPRITE_NO_CULL) == RENDER_SPRITE_NO_CULL);

    if (centered) {
        dst_rect.x -= dst_rect.w / 2;
        dst_rect.y -= dst_rect.h / 2;
    }

    float position_left = (float)dst_rect.x;
    float position_right = (float)(dst_rect.x + dst_rect.w);
    float position_top = (float)(SCREEN_HEIGHT - dst_rect.y);
    float position_bottom = (float)(SCREEN_HEIGHT - (dst_rect.y + dst_rect.h));

    if (cull) {
        if (position_right <= 0.0f || position_left >= (float)SCREEN_WIDTH || position_top <= 0.0f || position_bottom >= (float)SCREEN_HEIGHT) {
            return;
        }
    }

    float tex_coord_left = (float)src_rect.x / (float)state.atlases[atlas].texture_size.x;
    float tex_coord_right = tex_coord_left + ((float)src_rect.w / (float)state.atlases[atlas].texture_size.x);
    float tex_coord_top = 1.0f - ((float)src_rect.y / (float)state.atlases[atlas].texture_size.y);
    float tex_coord_bottom = tex_coord_top - ((float)src_rect.h / (float)state.atlases[atlas].texture_size.y);

    if (flip_h) {
        float temp = tex_coord_left;
        tex_coord_left = tex_coord_right;
        tex_coord_right = temp;
    }

    state.sprite_vertices.push_back((sprite_vertex_t) {
        .position = { position_left, position_top, (float)-z_index },
        .tex_coord = { tex_coord_left, tex_coord_top, (float)atlas }
    });
    state.sprite_vertices.push_back((sprite_vertex_t) {
        .position = { position_right, position_bottom, (float)-z_index },
        .tex_coord = { tex_coord_right, tex_coord_bottom, (float)atlas }
    });
    state.sprite_vertices.push_back((sprite_vertex_t) {
        .position = { position_left, position_bottom, (float)-z_index },
        .tex_coord = { tex_coord_left, tex_coord_bottom, (float)atlas }
    });
    state.sprite_vertices.push_back((sprite_vertex_t) {
        .position = { position_left, position_top, (float)-z_index },
        .tex_coord = { tex_coord_left, tex_coord_top, (float)atlas }
    });
    state.sprite_vertices.push_back((sprite_vertex_t) {
        .position = { position_right, position_top, (float)-z_index },
        .tex_coord = { tex_coord_right, tex_coord_top, (float)atlas }
    });
    state.sprite_vertices.push_back((sprite_vertex_t) {
        .position = { position_right, position_bottom, (float)-z_index },
        .tex_coord = { tex_coord_right, tex_coord_bottom, (float)atlas }
    });
}

void render_text(font_name name, const char* text, ivec2 position, int z_index, color_t color) {
    ivec2 glyph_position = position;
    const char* text_ptr = text;

    // Don't render empty string
    if (text_ptr == '\0') {
        return;
    }

    float frame_size_x = (float)state.fonts[name].glyph_max_width / (float)state.fonts[name].atlas_width;
    float tex_coord_bottom = 1.0f;
    float tex_coord_top = 0.0f;
    float position_top = (float)(SCREEN_HEIGHT - glyph_position.y);
    float position_bottom = (float)(SCREEN_HEIGHT - (glyph_position.y + state.fonts[name].atlas_height));

    std::vector<font_vertex_t> font_vertices;

    while (*text_ptr != '\0') {
        int glyph_index = (int)*text_ptr - FONT_FIRST_CHAR;
        if (glyph_index < 0 || glyph_index >= FONT_GLYPH_COUNT) {
            glyph_index = (int)('|' - FONT_FIRST_CHAR);
        }

        float position_left = (float)(glyph_position.x + state.fonts[name].glyphs[glyph_index].bearing_x);
        float position_right = (float)(glyph_position.x + state.fonts[name].glyphs[glyph_index].bearing_x + state.fonts[name].glyph_max_width);

        float tex_coord_left = frame_size_x * glyph_index;
        float tex_coord_right = tex_coord_left + frame_size_x;

        font_vertices.push_back((font_vertex_t) {
            .position = { position_left, position_top, (float)-z_index },
            .tex_coord = { tex_coord_left, tex_coord_top }
        });
        font_vertices.push_back((font_vertex_t) {
            .position = { position_right, position_bottom, (float)-z_index },
            .tex_coord = { tex_coord_right, tex_coord_bottom }
        });
        font_vertices.push_back((font_vertex_t) {
            .position = { position_left, position_bottom, (float)-z_index },
            .tex_coord = { tex_coord_left, tex_coord_bottom }
        });
        font_vertices.push_back((font_vertex_t) {
            .position = { position_left, position_top, (float)-z_index },
            .tex_coord = { tex_coord_left, tex_coord_top }
        });
        font_vertices.push_back((font_vertex_t) {
            .position = { position_right, position_top, (float)-z_index },
            .tex_coord = { tex_coord_right, tex_coord_top }
        });
        font_vertices.push_back((font_vertex_t) {
            .position = { position_right, position_bottom, (float)-z_index },
            .tex_coord = { tex_coord_right, tex_coord_bottom }
        });

        glyph_position.x += state.fonts[name].glyphs[glyph_index].advance;
        text_ptr++;
    }

    glBindBuffer(GL_ARRAY_BUFFER, state.font_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, font_vertices.size() * sizeof(font_vertex_t), &font_vertices[0]);

    glUseProgram(state.font_shader);
    glUniform3fv(state.font_shader_font_color_location, 1, &color.r);

    glBindVertexArray(state.font_vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, state.fonts[name].texture);

    glDrawArrays(GL_TRIANGLES, 0, font_vertices.size());

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void render_line(ivec2 start, ivec2 end, int z_index, color_t color) {
    glUseProgram(state.line_shader);
    glLineWidth(1);
    glUniform3fv(state.line_shader_color_location, 1, &color.r);

    float line_vertices[6] = { (float)start.x, (float)(SCREEN_HEIGHT - start.y), (float)-z_index, (float)end.x, (float)(SCREEN_HEIGHT - end.y), (float)-z_index };
    glBindVertexArray(state.line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, state.line_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(line_vertices), line_vertices);
    glDrawArrays(GL_LINES, 0, 2);

    glBindVertexArray(0);
}

void render_rect(ivec2 start, ivec2 end, int z_index, color_t color) {
    glUseProgram(state.line_shader);
    glLineWidth(1);
    glUniform3fv(state.line_shader_color_location, 1, &color.r);

    float line_vertices[24] = { 
        (float)start.x, (float)(SCREEN_HEIGHT - start.y), (float)-z_index, (float)end.x, (float)(SCREEN_HEIGHT - start.y), (float)-z_index,
        (float)end.x, (float)(SCREEN_HEIGHT - start.y), (float)-z_index, (float)end.x, (float)(SCREEN_HEIGHT - end.y), (float)-z_index,
        (float)start.x, (float)(SCREEN_HEIGHT - end.y), (float)-z_index, (float)end.x, (float)(SCREEN_HEIGHT - end.y), (float)-z_index,
        (float)start.x, (float)(SCREEN_HEIGHT - start.y), (float)-z_index, (float)start.x, (float)(SCREEN_HEIGHT - end.y), (float)-z_index
    };

    glBindVertexArray(state.line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, state.line_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(line_vertices), line_vertices);
    glDrawArrays(GL_LINES, 0, 8);

    glBindVertexArray(0);
}

void render_minimap_putpixel(minimap_layer layer, ivec2 position, minimap_pixel pixel) {
    if (layer == MINIMAP_LAYER_FOG) {
        position.x += MINIMAP_TEXTURE_WIDTH / 2;
    }
    state.minimap_texture_pixels[position.x + (position.y * MINIMAP_TEXTURE_WIDTH)] = state.minimap_pixel_values[pixel];
}

void render_minimap(ivec2 position, int z_index, ivec2 src_size, ivec2 dst_size) {
    GOLD_ASSERT(size.x <= MINIMAP_TEXTURE_WIDTH / 2 && size.y <= MINIMAP_TEXTURE_HEIGHT);

    float position_left = (float)position.x;
    float position_right = (float)(position.x + dst_size.x);
    float position_top = (float)(SCREEN_HEIGHT - position.y);
    float position_bottom = (float)(SCREEN_HEIGHT - (position.y + dst_size.y));

    float tex_coord_left = 0.0f;
    float tex_coord_right = (float)src_size.x / (float)MINIMAP_TEXTURE_WIDTH;
    float tex_coord_fog_left = 0.5f;
    float tex_coord_fog_right = 0.5f + tex_coord_right;
    float tex_coord_top = 0.0f;
    float tex_coord_bottom = ((float)src_size.y / (float)MINIMAP_TEXTURE_HEIGHT);

    sprite_vertex_t minimap_vertices[12] = {
        // minimap tiles
        (sprite_vertex_t) {
            .position = { position_left, position_top, (float)-z_index },
            .tex_coord = { tex_coord_left, tex_coord_top, 0.0f }
        },
        (sprite_vertex_t) {
            .position = { position_right, position_bottom, (float)-z_index },
            .tex_coord = { tex_coord_right, tex_coord_bottom, 0.0f }
        },
        (sprite_vertex_t) {
            .position = { position_left, position_bottom, (float)-z_index },
            .tex_coord = { tex_coord_left, tex_coord_bottom, 0.0f }
        },
        (sprite_vertex_t) {
            .position = { position_left, position_top, (float)-z_index },
            .tex_coord = { tex_coord_left, tex_coord_top, 0.0f }
        },
        (sprite_vertex_t) {
            .position = { position_right, position_top, (float)-z_index },
            .tex_coord = { tex_coord_right, tex_coord_top, 0.0f }
        },
        (sprite_vertex_t) {
            .position = { position_right, position_bottom, (float)-z_index },
            .tex_coord = { tex_coord_right, tex_coord_bottom, 0.0f }
        },

        // minimap fog of war
        (sprite_vertex_t) {
            .position = { position_left, position_top, (float)-z_index },
            .tex_coord = { tex_coord_fog_left, tex_coord_top, 0.0f }
        },
        (sprite_vertex_t) {
            .position = { position_right, position_bottom, (float)-z_index },
            .tex_coord = { tex_coord_fog_right, tex_coord_bottom, 0.0f }
        },
        (sprite_vertex_t) {
            .position = { position_left, position_bottom, (float)-z_index },
            .tex_coord = { tex_coord_fog_left, tex_coord_bottom, 0.0f }
        },
        (sprite_vertex_t) {
            .position = { position_left, position_top, (float)-z_index },
            .tex_coord = { tex_coord_fog_left, tex_coord_top, 0.0f }
        },
        (sprite_vertex_t) {
            .position = { position_right, position_top, (float)-z_index },
            .tex_coord = { tex_coord_fog_right, tex_coord_top, 0.0f }
        },
        (sprite_vertex_t) {
            .position = { position_right, position_bottom, (float)-z_index },
            .tex_coord = { tex_coord_fog_right, tex_coord_bottom, 0.0f }
        },
    };

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, state.minimap_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, MINIMAP_TEXTURE_WIDTH, MINIMAP_TEXTURE_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, state.minimap_texture_pixels);

    glUseProgram(state.sprite_shader);
    glBindVertexArray(state.sprite_vao);
    glBindBuffer(GL_ARRAY_BUFFER, state.sprite_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(minimap_vertices), minimap_vertices);
    glDrawArrays(GL_TRIANGLES, 0, 12);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}