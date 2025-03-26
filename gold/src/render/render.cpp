#include "render.h"

#define MAX_BATCH_VERTICES 32768
#define FONT_GLYPH_COUNT 96
#define FONT_FIRST_CHAR 32 // space
#define MINIMAP_TEXTURE_WIDTH 512
#define MINIMAP_TEXTURE_HEIGHT 256
#define ATLAS_WIDTH 1024
#define ATLAS_HEIGHT 1024

#include "core/logger.h"
#include "core/asserts.h"
#include "math/gmath.h"
#include "math/mat4.h"
#include "render/sprite.h"
#include "render/font.h"
#include "shader.h"
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <glad/glad.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>

struct SpriteVertex {
    float position[2];
    float tex_coord[3];
};

struct LineVertex {
    float position[2];
};

struct FontGlyph {
    int atlas_x;
    int atlas_y;
    int bearing_x;
    int bearing_y;
    int advance;
};

struct Font {
    int glyph_width;
    int glyph_height;
    FontGlyph glyphs[FONT_GLYPH_COUNT];
};

struct PlayerColor {
    const char* name;
    SDL_Color skin_color;
    SDL_Color clothes_color;
};

static const SDL_Color RECOLOR_CLOTHES_REF = { .r = 255, .g = 0, .b = 255, .a = 255 };
static const SDL_Color RECOLOR_SKIN_REF = { .r = 123, .g = 174, .b = 121, .a = 255 };
static const PlayerColor PLAYER_COLORS[MAX_PLAYERS] = {
    (PlayerColor) {
        .name = "Blue",
        .skin_color = (SDL_Color) { .r = 125, .g = 181, .b = 164, .a = 255 },
        .clothes_color = (SDL_Color) { .r = 92, .g = 132, .b = 153, .a = 255 }
    },
    (PlayerColor) {
        .name = "Red",
        .skin_color = (SDL_Color) { .r = 219, .g = 151, .b = 114, .a = 255 },
        .clothes_color = (SDL_Color) { .r = 186, .g = 97, .b = 95, .a = 255 }
    },
    (PlayerColor) {
        .name = "Green",
        .skin_color = (SDL_Color) { .r = 123, .g = 174, .b = 121, .a = 255 },
        .clothes_color = (SDL_Color) { .r = 77, .g = 135, .b = 115, .a = 255 },
    },
    (PlayerColor) {
        .name = "Purple",
        .skin_color = (SDL_Color) { .r = 184, .g = 169, .b = 204, .a = 255 },
        .clothes_color = (SDL_Color) { .r = 144, .g = 119, .b = 153, .a = 255 },
    }
};

struct RenderState {
    SDL_Window* window;
    SDL_GLContext context;
    ivec2 window_size;

    GLuint screen_shader;
    GLuint screen_framebuffer;
    GLuint screen_texture;
    GLuint screen_vao;

    GLuint sprite_shader;
    GLuint sprite_vao;
    GLuint sprite_vbo;
    uint32_t sprite_texture_array;
    ivec2 sprite_image_size[ATLAS_COUNT];
    std::vector<SpriteVertex> sprite_vertices;

    GLuint minimap_texture;
    uint32_t minimap_texture_pixels[MINIMAP_TEXTURE_WIDTH * MINIMAP_TEXTURE_HEIGHT];
    uint32_t minimap_pixel_values[MINIMAP_PIXEL_COUNT];

    GLuint font_vao;
    GLuint font_vbo;
    Font fonts[FONT_COUNT];

    GLuint line_shader;
    GLint line_shader_color_location;
    GLuint line_vao;
    GLuint line_vbo;
    std::vector<LineVertex> line_vertices;
};
static RenderState state;

void render_init_quad_vao();
void render_init_sprite_vao();
void render_init_line_vao();
void render_init_minimap_texture();
bool render_init_screen_framebuffer();

bool render_load_atlases();

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

    float screen_size[2] = { (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT };
    glUniform2fv(glGetUniformLocation(state.screen_shader, "screen_size"), 1, screen_size);

    ivec2 window_size;
    SDL_GetWindowSize(state.window, &window_size.x, &window_size.y);
    float window_size_float[2] = { (float)window_size.x, (float)window_size.y };
    glUniform2fv(glGetUniformLocation(state.screen_shader, "window_size"), 1, window_size_float);

    // Load sprite shader
    if (!shader_load(&state.sprite_shader, "sprite.vert.glsl", "sprite.frag.glsl")) {
        return false;
    }
    glUseProgram(state.sprite_shader);
    glUniform1ui(glGetUniformLocation(state.sprite_shader, "sprite_texture_array"), 0);
    mat4 projection = mat4_ortho(0.0f, (float)SCREEN_WIDTH, 0.0f, (float)SCREEN_HEIGHT, 0.0f, 1.0f);
    glUniformMatrix4fv(glGetUniformLocation(state.sprite_shader, "projection"), 1, GL_FALSE, projection.data);

    // Load line shader
    if (!shader_load(&state.line_shader, "line.vert.glsl", "line.frag.glsl")) {
        return false;
    }
    glUseProgram(state.line_shader);
    glUniformMatrix4fv(glGetUniformLocation(state.line_shader, "projection"), 1, GL_FALSE, projection.data);

    if (!render_load_atlases()) {
        return false;
    }

    log_info("Initialized renderer. Vendor: %s. Renderer: %s. Version: %s.", glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION));
    SDL_GL_SetSwapInterval(1);

    return true;
}

void render_set_window_size(ivec2 window_size) {
    SDL_SetWindowSize(state.window, window_size.x, window_size.y);
    glUseProgram(state.screen_shader);
    glUniform2iv(glGetUniformLocation(state.screen_shader, "window_size"), 1, &window_size.x);
    state.window_size = window_size;
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
    glBufferData(GL_ARRAY_BUFFER, MAX_BATCH_VERTICES * sizeof(SpriteVertex), NULL, GL_DYNAMIC_DRAW);

    // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    // tex coord
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void render_init_line_vao() {
    glGenVertexArrays(1, &state.line_vao);
    glGenBuffers(1, &state.line_vbo);
    glBindVertexArray(state.line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, state.line_vbo);
    glBufferData(GL_ARRAY_BUFFER, 256 * sizeof(LineVertex), NULL, GL_DYNAMIC_DRAW);

    // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

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
    glGenRenderbuffers(1, &screen_depthbuffer);
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

void render_flip_sdl_surface_vertically(SDL_Surface* surface) {
    SDL_LockSurface(surface);
    int sprite_surface_pitch = surface->pitch;
    uint8_t* temp = (uint8_t*)malloc(sprite_surface_pitch);
    uint8_t* sprite_surface_pixels = (uint8_t*)surface->pixels;

    for (int row = 0; row < surface->h / 2; ++row) {
        uint8_t* row1 = sprite_surface_pixels + (row * sprite_surface_pitch);
        uint8_t* row2 = sprite_surface_pixels + ((surface->h - row - 1) * sprite_surface_pitch);

        memcpy(temp, row1, sprite_surface_pitch);
        memcpy(row1, row2, sprite_surface_pitch);
        memcpy(row2, temp, sprite_surface_pitch);
    }
    free(temp);
    SDL_UnlockSurface(surface);
}

SDL_Surface* render_load_atlas_recolor(SDL_Surface* sprite_surface, const AtlasParams params) {
    // Create a surface big enough to hold the recolor atlas
    SDL_Surface* recolor_surface = SDL_CreateRGBSurfaceWithFormat(0, ATLAS_WIDTH, ATLAS_HEIGHT, 32, sprite_surface->format->format);
    if (recolor_surface == NULL) {
        log_error("Error creating recolor surface for sprite: %s", SDL_GetError());
        return NULL;
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
    return recolor_surface;
}

SDL_Surface* render_load_atlas_tileset(SDL_Surface* sprite_surface, const AtlasParams params) {
    SDL_Surface* tileset_surface = SDL_CreateRGBSurfaceWithFormat(0, ATLAS_WIDTH, ATLAS_HEIGHT, 32, sprite_surface->format->format);
    if (tileset_surface == NULL) {
        log_error("Unable to create tileset surface: %s", SDL_GetError());
        return NULL;
    }

    SDL_Rect src_rect = (SDL_Rect) { .x = 0, .y = 0, .w = TILE_SIZE, .h = TILE_SIZE };
    SDL_Rect dst_rect = (SDL_Rect) { .x = 0, .y = 0, .w = TILE_SIZE, .h = TILE_SIZE };

    for (int tile = params.tileset.begin; tile < params.tileset.end + 1; tile++) {
        const TileData& tile_data = resource_get_tile_data((SpriteName)tile);

        // We can go ahead and set this now since the code is the same for both tile types
        // and the dst_rect is already pointing in the place where the tile will go on the atlas
        SpriteInfo tile_info = (SpriteInfo) {
            .atlas = ATLAS_TILESET,
            .atlas_x = dst_rect.x,
            .atlas_y = dst_rect.y,
            .frame_width = TILE_SIZE,
            .frame_height = TILE_SIZE
        };
        resource_set_sprite_info((SpriteName)tile, tile_info);

        if (tile_data.type == TILE_TYPE_SINGLE) {
            src_rect.x = tile_data.source_x;
            src_rect.y = tile_data.source_y;
            SDL_BlitSurface(sprite_surface, &src_rect, tileset_surface, &dst_rect);

            dst_rect.x += TILE_SIZE;
            if (dst_rect.x == ATLAS_WIDTH) {
                dst_rect.x = 0;
                dst_rect.y += TILE_SIZE;
            }
        } else if (tile_data.type == TILE_TYPE_AUTO) {
            // To generate an autotile, we iterate through each combination of neighbors
            for (uint32_t neighbors = 0; neighbors < 256; neighbors++) {
                // There are 256 neighbor combinations, but only 47 of them are unique because
                // a diagonal neighbor does not affect what tile we render when autotiling 
                // unless there is an adjacent tile in both directions. 
                // for example: neighbor in NE by itself does not affect which tile we render
                // but neighbor in NE and neighbor in N and neighbor in E does (both adjacent directions are required)
                bool is_unique = true;
                for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
                    if (direction % 2 == 1 && (DIRECTION_MASK[direction] & neighbors) == DIRECTION_MASK[direction]) {
                        int prev_direction = direction - 1;
                        int next_direction = (direction + 1) % DIRECTION_COUNT;
                        if ((DIRECTION_MASK[prev_direction] & neighbors) != DIRECTION_MASK[prev_direction] ||
                            (DIRECTION_MASK[next_direction] & neighbors) != DIRECTION_MASK[next_direction]) {
                            is_unique = false;
                            break;
                        }
                    }
                }
                if (!is_unique) {
                    continue;
                }

                // Each unique autotile is formed by sampling corners off of the source tile
                // https://gamedev.stackexchange.com/questions/46594/elegant-autotiling
                for (uint32_t edge = 0; edge < 4; edge++) {
                    ivec2 edge_source_pos = ivec2(tile_data.source_x, tile_data.source_y) + (autotile_edge_lookup(edge, neighbors & AUTOTILE_EDGE_MASK[edge]) * (TILE_SIZE / 2));
                    SDL_Rect subtile_src_rect = (SDL_Rect) {
                        .x = edge_source_pos.x,
                        .y = edge_source_pos.y,
                        .w = TILE_SIZE / 2,
                        .h = TILE_SIZE / 2
                    };
                    SDL_Rect subtile_dst_rect = (SDL_Rect) {
                        .x = dst_rect.x + (AUTOTILE_EDGE_OFFSETS[edge].x * (TILE_SIZE / 2)),
                        .y = AUTOTILE_EDGE_OFFSETS[edge].y * (TILE_SIZE / 2),
                        .w = TILE_SIZE / 2,
                        .h = TILE_SIZE / 2
                    };

                    SDL_BlitSurface(sprite_surface, &subtile_src_rect, tileset_surface, &subtile_dst_rect);
                } 

                dst_rect.x += TILE_SIZE;
                if (dst_rect.x == ATLAS_WIDTH) {
                    dst_rect.x = 0;
                    dst_rect.y += TILE_SIZE;
                }
            } // End for each neighbor combo
        } // End if tile type is auto
    } // End for each tile

    SDL_FreeSurface(sprite_surface);
    return tileset_surface;
}

SDL_Surface* render_load_atlas_fonts() {
    ivec2 font_atlas_position = ivec2(0, 0);
    SDL_Surface* atlas_surface = SDL_CreateRGBSurface(0, ATLAS_WIDTH, ATLAS_HEIGHT, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
    if (atlas_surface == NULL) {
        log_error("Unable to create font atlas: %s", SDL_GetError());
        return NULL;
    }

    for (int name = 0; name < FONT_COUNT; name++) {
        const FontParams& params = resource_get_font_params((FontName)name);
        Font& font = state.fonts[name];

        char path[128];
        sprintf(path, "%sfont/%s", RESOURCE_PATH, params.path);

        log_info("Loading font %s size %u color %u,%u,%u", path, params.size, params.r, params.g, params.b);

        // Open the font
        TTF_Font* ttf_font = TTF_OpenFont(path, params.size);
        if (ttf_font == NULL) {
            log_error("Unable to open font %s: %s", path, TTF_GetError());
            return NULL;
        }

        // Render each glyph to a surface
        SDL_Surface* glyphs[FONT_GLYPH_COUNT];
        int glyph_max_width = 0;
        int glyph_max_height = 0;

        // Hack alert!
        // Since TTF creates surfaces as BGRA but the rest of the atlas rendering
        // code expects RGBA, I'm just flipping the R and the B values here
        SDL_Color font_color = (SDL_Color) { .r = params.b, .g = params.g, .b = params.r, .a = 255 };
        for (int glyph_index = 0; glyph_index < FONT_GLYPH_COUNT; glyph_index++) {
            char text[2] = { (char)(FONT_FIRST_CHAR + glyph_index), '\0'};
            glyphs[glyph_index] = TTF_RenderText_Solid(ttf_font, text, font_color);
            if (glyphs[glyph_index] == NULL) {
                log_error("Error rendering glyph %s for font %s: %s", text, path, TTF_GetError());
                return NULL;
            }

            glyph_max_width = std::max(glyph_max_width, glyphs[glyph_index]->w);
            glyph_max_height = std::max(glyph_max_height, glyphs[glyph_index]->h);
        }

        // Render each surface glyph onto a single atlas surface
        for (int glyph_index = 0; glyph_index < FONT_GLYPH_COUNT; glyph_index++) {
            if (font_atlas_position.x + glyphs[glyph_index]->w >= ATLAS_WIDTH) {
                font_atlas_position.x = 0;
                font_atlas_position.y += glyph_max_height;
            }

            SDL_Rect dest_rect = (SDL_Rect) { 
                .x = font_atlas_position.x, 
                .y = font_atlas_position.y, 
                .w = glyphs[glyph_index]->w, 
                .h = glyphs[glyph_index]->h 
            };
            SDL_BlitSurface(glyphs[glyph_index], NULL, atlas_surface, &dest_rect);

            font.glyphs[glyph_index].atlas_x = font_atlas_position.x;
            font.glyphs[glyph_index].atlas_y = font_atlas_position.y;
            font_atlas_position.x += glyph_max_width;
        }

        // Finish filling out the font struct
        font.glyph_width = glyph_max_width;
        font.glyph_height = glyph_max_height;
        for (int glyph_index = 0; glyph_index < FONT_GLYPH_COUNT; glyph_index++) {
            int bearing_y;
            TTF_GlyphMetrics(ttf_font, FONT_FIRST_CHAR + glyph_index, &font.glyphs[glyph_index].bearing_x, NULL, NULL, &bearing_y, &font.glyphs[glyph_index].advance);
            font.glyphs[glyph_index].bearing_y = glyph_max_height - bearing_y;
            if (params.ignore_bearing) {
                font.glyphs[glyph_index].bearing_x = 0;
                font.glyphs[glyph_index].bearing_y = 0;
            }
        }

        // Free all the resources
        for (int glyph_index = 0; glyph_index < FONT_GLYPH_COUNT; glyph_index++) {
            SDL_FreeSurface(glyphs[glyph_index]);
        }
        TTF_CloseFont(ttf_font);

        font_atlas_position.x = 0;
        font_atlas_position.y += glyph_max_height;
    }

    return atlas_surface;
}

bool render_load_atlases() {
    glGenTextures(1, &state.sprite_texture_array);
    glBindTexture(GL_TEXTURE_2D_ARRAY, state.sprite_texture_array);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, ATLAS_WIDTH, ATLAS_HEIGHT, ATLAS_COUNT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    for (int atlas = 0; atlas < ATLAS_COUNT; atlas++) {
        const AtlasParams& params = resource_get_atlas_params((AtlasName)atlas);

        SDL_Surface* sprite_surface;
        if (params.strategy == ATLAS_IMPORT_FONTS) {
            sprite_surface = render_load_atlas_fonts();
        } else {
            char path[128];
            sprintf(path, "%ssprite/%s", RESOURCE_PATH, params.path);

            log_info("Loading atlas %s", path);

            sprite_surface = IMG_Load(path);
            if (sprite_surface == NULL) {
                log_error("Error loading surface for sprite %s: %s", path, IMG_GetError());
                return false;
            }
            GOLD_ASSERT(sprite_surface->format->BytesPerPixel == 4);

            state.sprite_image_size[atlas] = ivec2(sprite_surface->w, sprite_surface->h);

            if (params.strategy == ATLAS_IMPORT_DEFAULT) {
                GOLD_ASSERT(sprite_surface->w == ATLAS_WIDTH && sprite_surface->h == ATLAS_HEIGHT);
            } else if (params.strategy == ATLAS_IMPORT_RECOLOR || params.strategy == ATLAS_IMPORT_RECOLOR_AND_LOW_ALPHA) {
                sprite_surface = render_load_atlas_recolor(sprite_surface, params);
            } else if (params.strategy == ATLAS_IMPORT_TILESET) {
                sprite_surface = render_load_atlas_tileset(sprite_surface, params);
            }
        }

        if (sprite_surface == NULL) {
            return false;
        }

        render_flip_sdl_surface_vertically(sprite_surface);

        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, atlas, sprite_surface->w, sprite_surface->h, 1, GL_RGBA, GL_UNSIGNED_BYTE, sprite_surface->pixels);

        SDL_FreeSurface(sprite_surface);
    }

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

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
    glDepthFunc(GL_LEQUAL);
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
    glBindVertexArray(state.sprite_vao);
    glBindBuffer(GL_ARRAY_BUFFER, state.sprite_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, state.sprite_vertices.size() * sizeof(SpriteVertex), &state.sprite_vertices[0]);

    // Render sprite batch
    glUseProgram(state.sprite_shader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, state.sprite_texture_array);

    glDrawArrays(GL_TRIANGLES, 0, state.sprite_vertices.size());

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    state.sprite_vertices.clear();
}

void render_line_vertices() {
    glUseProgram(state.line_shader);
    glLineWidth(1);

    glBindVertexArray(state.line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, state.line_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(LineVertex) * state.line_vertices.size(), &state.line_vertices[0]);
    glDrawArrays(GL_LINES, 0, state.line_vertices.size());

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    state.line_vertices.clear();
}

void render_present_frame() {
    render_sprite_vertices();
    render_line_vertices();

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

void render_sprite(SpriteName name, ivec2 frame, ivec2 position, uint32_t options, int recolor_id) {
    const SpriteInfo& sprite_info = resource_get_sprite_info(name);

    Rect src_rect = (Rect) { 
        .x = sprite_info.atlas_x + (frame.x * sprite_info.frame_width), 
        .y = sprite_info.atlas_y + (recolor_id * state.sprite_image_size[sprite_info.atlas].y) + (frame.y * sprite_info.frame_height), 
        .w = sprite_info.frame_width,
        .h = sprite_info.frame_height
    };
    Rect dst_rect = (Rect) { 
        .x = position.x, 
        .y = position.y, 
        .w = sprite_info.frame_width,
        .h = sprite_info.frame_height
    };
    render_atlas(sprite_info.atlas, src_rect, dst_rect, options);
}

ivec2 render_get_sprite_image_size(AtlasName atlas) {
    return state.sprite_image_size[atlas];
}

void render_ninepatch(SpriteName sprite, Rect rect) {
    const SpriteInfo& sprite_info = resource_get_sprite_info(sprite);
    GOLD_ASSERT(rect.w > sprite_info.frame_width * 2 && rect.h > sprite_info.frame_height * 2);

    Rect src_rect = (Rect) {
        .x = sprite_info.atlas_x, 
        .y = sprite_info.atlas_y,
        .w = sprite_info.frame_width,
        .h = sprite_info.frame_height
    };
    Rect dst_rect = (Rect) {
        .x = rect.x,
        .y = rect.y,
        .w = sprite_info.frame_width,
        .h = sprite_info.frame_height
    };

    // Top left
    render_atlas(sprite_info.atlas, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);

    // Top right
    src_rect.x = sprite_info.atlas_x + (2 * sprite_info.frame_width);
    dst_rect.x = rect.x + rect.w - sprite_info.frame_width;
    render_atlas(sprite_info.atlas, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);

    // Bottom left
    src_rect.x = sprite_info.atlas_x;
    src_rect.y = sprite_info.atlas_y + (2 * sprite_info.frame_height);
    dst_rect.x = rect.x;
    dst_rect.y = rect.y + rect.h - sprite_info.frame_height;
    render_atlas(sprite_info.atlas, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);

    // Bottom right
    src_rect.x = sprite_info.atlas_x + (2 * sprite_info.frame_width);
    dst_rect.x = rect.x + rect.w - sprite_info.frame_width;
    render_atlas(sprite_info.atlas, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);

    // Top edge
    src_rect.x = sprite_info.atlas_x + sprite_info.frame_width;
    src_rect.y = sprite_info.atlas_y;
    dst_rect.x = rect.x + sprite_info.frame_width;
    dst_rect.y = rect.y;
    dst_rect.w = (rect.x + rect.w - sprite_info.frame_width) - dst_rect.x;
    render_atlas(sprite_info.atlas, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);

    // Bottom edge
    src_rect.y = sprite_info.atlas_y + (2 * sprite_info.frame_height);
    dst_rect.y = (rect.y + rect.h - sprite_info.frame_height);
    render_atlas(sprite_info.atlas, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);

    // Left edge
    src_rect.x = sprite_info.atlas_x;
    src_rect.y = sprite_info.atlas_y + sprite_info.frame_height;
    dst_rect.x = rect.x;
    dst_rect.w = sprite_info.frame_width;
    dst_rect.y = rect.y + sprite_info.frame_height;
    dst_rect.h = (rect.y + rect.h - sprite_info.frame_height) - dst_rect.y;
    render_atlas(sprite_info.atlas, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);

    // Right edge
    src_rect.x = sprite_info.atlas_x + (2 * sprite_info.frame_width);
    dst_rect.x = rect.x + rect.w - sprite_info.frame_width;
    render_atlas(sprite_info.atlas, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);

    // Center
    src_rect.x = sprite_info.atlas_x + sprite_info.frame_width;
    src_rect.y = sprite_info.atlas_y + sprite_info.frame_height;
    dst_rect.x = rect.x + sprite_info.frame_width;
    dst_rect.y = rect.y + sprite_info.frame_height;
    dst_rect.w = (rect.x + rect.w - sprite_info.frame_width) - dst_rect.x;
    dst_rect.h = (rect.y + rect.h - sprite_info.frame_height) - dst_rect.y;
    render_atlas(sprite_info.atlas, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);
}

void render_atlas(AtlasName atlas, Rect src_rect, Rect dst_rect, uint32_t options) {
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

    float tex_coord_left = (float)src_rect.x / (float)ATLAS_WIDTH;
    float tex_coord_right = tex_coord_left + ((float)src_rect.w / (float)ATLAS_WIDTH);
    float tex_coord_top = 1.0f - ((float)src_rect.y / (float)ATLAS_HEIGHT);
    float tex_coord_bottom = tex_coord_top - ((float)src_rect.h / (float)ATLAS_HEIGHT);

    if (flip_h) {
        float temp = tex_coord_left;
        tex_coord_left = tex_coord_right;
        tex_coord_right = temp;
    }

    state.sprite_vertices.push_back((SpriteVertex) {
        .position = { position_left, position_top },
        .tex_coord = { tex_coord_left, tex_coord_top, (float)atlas }
    });
    state.sprite_vertices.push_back((SpriteVertex) {
        .position = { position_right, position_bottom },
        .tex_coord = { tex_coord_right, tex_coord_bottom, (float)atlas }
    });
    state.sprite_vertices.push_back((SpriteVertex) {
        .position = { position_left, position_bottom },
        .tex_coord = { tex_coord_left, tex_coord_bottom, (float)atlas }
    });
    state.sprite_vertices.push_back((SpriteVertex) {
        .position = { position_left, position_top },
        .tex_coord = { tex_coord_left, tex_coord_top, (float)atlas }
    });
    state.sprite_vertices.push_back((SpriteVertex) {
        .position = { position_right, position_top },
        .tex_coord = { tex_coord_right, tex_coord_top, (float)atlas }
    });
    state.sprite_vertices.push_back((SpriteVertex) {
        .position = { position_right, position_bottom },
        .tex_coord = { tex_coord_right, tex_coord_bottom, (float)atlas }
    });
}

void render_text(FontName name, const char* text, ivec2 position) {
    ivec2 glyph_position = position;
    size_t text_index = 0;

    float frame_size_x = (float)(state.fonts[name].glyph_width) / (float)ATLAS_WIDTH;
    float frame_size_y = (float)(state.fonts[name].glyph_height) / (float)ATLAS_HEIGHT;

    while (text[text_index] != '\0') {
        int glyph_index = (int)text[text_index] - FONT_FIRST_CHAR;
        if (glyph_index < 0 || glyph_index >= FONT_GLYPH_COUNT) {
            glyph_index = (int)('|' - FONT_FIRST_CHAR);
        }

        float position_top = (float)(SCREEN_HEIGHT - (glyph_position.y + state.fonts[name].glyphs[glyph_index].bearing_y));
        float position_bottom = position_top - (float)state.fonts[name].glyph_height;
        float position_left = (float)(glyph_position.x + state.fonts[name].glyphs[glyph_index].bearing_x);
        float position_right = position_left + (float)state.fonts[name].glyph_width;

        float tex_coord_left = (float)state.fonts[name].glyphs[glyph_index].atlas_x / (float)ATLAS_WIDTH;
        float tex_coord_right = tex_coord_left + frame_size_x;
        float tex_coord_top = 1.0f - ((float)state.fonts[name].glyphs[glyph_index].atlas_y / (float)ATLAS_HEIGHT);
        float tex_coord_bottom = tex_coord_top - frame_size_y;

        state.sprite_vertices.push_back((SpriteVertex) {
            .position = { position_left, position_top },
            .tex_coord = { tex_coord_left, tex_coord_top, (float)ATLAS_FONT }
        });
        state.sprite_vertices.push_back((SpriteVertex) {
            .position = { position_right, position_bottom },
            .tex_coord = { tex_coord_right, tex_coord_bottom, (float)ATLAS_FONT }
        });
        state.sprite_vertices.push_back((SpriteVertex) {
            .position = { position_left, position_bottom },
            .tex_coord = { tex_coord_left, tex_coord_bottom, (float)ATLAS_FONT }
        });
        state.sprite_vertices.push_back((SpriteVertex) {
            .position = { position_left, position_top },
            .tex_coord = { tex_coord_left, tex_coord_top, (float)ATLAS_FONT }
        });
        state.sprite_vertices.push_back((SpriteVertex) {
            .position = { position_right, position_top },
            .tex_coord = { tex_coord_right, tex_coord_top, (float)ATLAS_FONT }
        });
        state.sprite_vertices.push_back((SpriteVertex) {
            .position = { position_right, position_bottom },
            .tex_coord = { tex_coord_right, tex_coord_bottom, (float)ATLAS_FONT }
        });

        glyph_position.x += state.fonts[name].glyphs[glyph_index].advance;
        text_index++;
    }
}

ivec2 render_get_text_size(FontName name, const char* text) {
    ivec2 size = ivec2(0, state.fonts[name].glyph_height);
    size_t text_index = 0;

    while (text[text_index] != '\0') {
        int glyph_index = (int)text[text_index] - FONT_FIRST_CHAR;
        if (glyph_index < 0 || glyph_index >= FONT_GLYPH_COUNT) {
            glyph_index = (int)('|' - FONT_FIRST_CHAR);
        }

        size.x += state.fonts[name].glyphs[glyph_index].advance;
        text_index++;
    }

    return size;
}

void render_line(ivec2 start, ivec2 end) {
    state.line_vertices.push_back((LineVertex) {
        .position = { (float)start.x, (float)(SCREEN_HEIGHT - start.y) }
    });
    state.line_vertices.push_back((LineVertex) {
        .position = { (float)end.x, (float)(SCREEN_HEIGHT - end.y) }
    });
}

void render_rect(ivec2 start, ivec2 end) {
    const SpriteInfo& sprite_info = resource_get_sprite_info(SPRITE_UI_WHITE);
    Rect src_rect = (Rect) {
        .x = sprite_info.atlas_x,
        .y = sprite_info.atlas_y,
        .w = sprite_info.frame_width,
        .h = sprite_info.frame_height
    };
    Rect dst_rect = (Rect) {
        .x = start.x, .y = start.y,
        .w = 1, .h = end.y - start.y
    };
    render_atlas(ATLAS_UI, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);
    dst_rect.x = end.x - 1;
    render_atlas(ATLAS_UI, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);
    dst_rect.x = start.x;
    dst_rect.w = end.x - start.x;
    dst_rect.h = 1;
    render_atlas(ATLAS_UI, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);
    dst_rect.y = end.y - 1;
    render_atlas(ATLAS_UI, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);
}

void render_minimap_putpixel(MinimapLayer layer, ivec2 position, MinimapPixel pixel) {
    if (layer == MINIMAP_LAYER_FOG) {
        position.x += MINIMAP_TEXTURE_WIDTH / 2;
    }
    state.minimap_texture_pixels[position.x + (position.y * MINIMAP_TEXTURE_WIDTH)] = state.minimap_pixel_values[pixel];
}

void render_minimap(ivec2 position, ivec2 src_size, ivec2 dst_size) {
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

    SpriteVertex minimap_vertices[12] = {
        // minimap tiles
        (SpriteVertex) {
            .position = { position_left, position_top },
            .tex_coord = { tex_coord_left, tex_coord_top, 0.0f }
        },
        (SpriteVertex) {
            .position = { position_right, position_bottom },
            .tex_coord = { tex_coord_right, tex_coord_bottom, 0.0f }
        },
        (SpriteVertex) {
            .position = { position_left, position_bottom },
            .tex_coord = { tex_coord_left, tex_coord_bottom, 0.0f }
        },
        (SpriteVertex) {
            .position = { position_left, position_top },
            .tex_coord = { tex_coord_left, tex_coord_top, 0.0f }
        },
        (SpriteVertex) {
            .position = { position_right, position_top },
            .tex_coord = { tex_coord_right, tex_coord_top, 0.0f }
        },
        (SpriteVertex) {
            .position = { position_right, position_bottom },
            .tex_coord = { tex_coord_right, tex_coord_bottom, 0.0f }
        },

        // minimap fog of war
        (SpriteVertex) {
            .position = { position_left, position_top },
            .tex_coord = { tex_coord_fog_left, tex_coord_top, 0.0f }
        },
        (SpriteVertex) {
            .position = { position_right, position_bottom },
            .tex_coord = { tex_coord_fog_right, tex_coord_bottom, 0.0f }
        },
        (SpriteVertex) {
            .position = { position_left, position_bottom },
            .tex_coord = { tex_coord_fog_left, tex_coord_bottom, 0.0f }
        },
        (SpriteVertex) {
            .position = { position_left, position_top },
            .tex_coord = { tex_coord_fog_left, tex_coord_top, 0.0f }
        },
        (SpriteVertex) {
            .position = { position_right, position_top },
            .tex_coord = { tex_coord_fog_right, tex_coord_top, 0.0f }
        },
        (SpriteVertex) {
            .position = { position_right, position_bottom },
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