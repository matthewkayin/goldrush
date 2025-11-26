#include "render.h"

#include "core/logger.h"
#include "core/filesystem.h"
#include "core/asserts.h"
#include "math/mat4.h"
#include <SDL3/SDL_image.h>
#include <SDL3/SDL_ttf.h>
#include <glad/glad.h>
#include <vector>
#include <unordered_map>

#define MAX_BATCH_VERTICES 32768
#define FONT_GLYPH_COUNT 95
#define FONT_FIRST_CHAR 32U // space
#define FONT_HFRAMES 16
#define FONT_VFRAMES 6
#define ATLAS_SIZE 2048
#define MINIMAP_TEXTURE_WIDTH 512
#define MINIMAP_TEXTURE_HEIGHT 256
#define TILE_SRC_SIZE 16

static const int RENDER_SCALE_INT = 2;
static const int RENDER_SCALE_FLOAT = 2.0f;

struct SpriteVertex {
    float position[2];
    float tex_coord[3];
};

struct FontGlyph {
    int bearing_x;
    int bearing_y;
    int advance;
};

struct Font {
    int atlas;
    int atlas_x;
    int atlas_y;
    int glyph_width;
    int glyph_height;
    FontGlyph glyphs[FONT_GLYPH_COUNT];
};

static const SDL_Color RECOLOR_CLOTHES_REF = (SDL_Color) { .r = 255, .g = 0, .b = 255, .a = 255 };
static const SDL_Color RECOLOR_SKIN_REF = (SDL_Color) { .r = 123, .g = 174, .b = 121, .a = 255 };
static const std::unordered_map<RenderColor, SDL_Color> RENDER_COLOR_VALUES = {
    { RENDER_COLOR_WHITE, (SDL_Color) { .r = 255, .g = 255, .b = 255, .a = 255 }},
    { RENDER_COLOR_OFFBLACK, (SDL_Color) { .r = 40, .g = 37, .b = 45, .a = 255 }},
    { RENDER_COLOR_OFFBLACK_TRANSPARENT, (SDL_Color) { .r = 40, .g = 37, .b = 45, .a = 128 }},
    { RENDER_COLOR_DARK_GRAY, (SDL_Color) { .r = 94, .g = 88, .b = 89, .a = 255 }},
    { RENDER_COLOR_BLUE, (SDL_Color) { .r = 92, .g = 132, .b = 153, .a = 255 }},
    { RENDER_COLOR_DIM_BLUE, (SDL_Color) { .r = 70, .g = 100, .b = 115, .a = 255 }},
    { RENDER_COLOR_LIGHT_BLUE, (SDL_Color) { .r = 134, .g = 191, .b = 186, .a = 255 }},
    { RENDER_COLOR_RED, (SDL_Color) { .r = 186, .g = 97, .b = 95, .a = 255 }},
    { RENDER_COLOR_RED_TRANSPARENT, (SDL_Color) { .r = 186, .g = 97, .b = 95, .a = 128 }},
    { RENDER_COLOR_LIGHT_RED, (SDL_Color) { .r = 219, .g = 151, .b = 114, .a = 255 }},
    { RENDER_COLOR_GOLD, (SDL_Color) { .r = 238, .g = 209, .b = 158, .a = 255 }},
    { RENDER_COLOR_DIM_SAND, (SDL_Color) { .r = 204, .g = 162, .b = 139, .a = 255 }},
    { RENDER_COLOR_GREEN, (SDL_Color) { .r = 123, .g = 174, .b = 121, .a = 255 }},
    { RENDER_COLOR_GREEN_TRANSPARENT, (SDL_Color) { .r = 123, .g = 174, .b = 121, .a = 128 }},
    { RENDER_COLOR_DARK_GREEN, (SDL_Color) { .r = 77, .g = 135, .b = 115, .a = 255 }},
    { RENDER_COLOR_PURPLE, (SDL_Color) { .r = 144, .g = 119, .b = 153, .a = 255 }},
    { RENDER_COLOR_LIGHT_PURPLE, (SDL_Color) { .r = 184, .g = 169, .b = 204, .a = 255 }}
};

enum LoadedSurfaceType {
    LOADED_SURFACE_FONT,
    LOADED_SURFACE_SPRITE
};

struct LoadedSurface {
    LoadedSurfaceType type;
    int name;
    SDL_Surface* surface;
};

struct RenderState {
    SDL_Window* window;
    SDL_GLContext context;

    GLuint screen_shader;
    GLuint screen_framebuffer;
    GLuint screen_texture;
    GLuint screenshot_texture;
    GLuint screen_vao;

    GLuint sprite_shader;
    GLuint sprite_vao;
    GLuint sprite_vbo;
    uint32_t sprite_texture_array;
    SpriteInfo sprite_info[SPRITE_COUNT];
    std::vector<SpriteVertex> sprite_vertices;

    GLuint minimap_texture;
    GLuint minimap_shader;
    uint32_t minimap_texture_pixels[MINIMAP_TEXTURE_WIDTH * MINIMAP_TEXTURE_HEIGHT];
    uint32_t minimap_pixel_values[MINIMAP_PIXEL_COUNT];

    Font fonts[FONT_COUNT];
};
static RenderState state;

// Init
bool render_load_sprites();
void render_flip_sdl_surface_vertically(SDL_Surface* surface);
SDL_Surface* render_recolor_surface(SDL_Surface* sprite_surface, bool recolor_low_alpha);
SDL_Surface* render_create_single_tile_surface(SDL_Surface* tileset_surface, const SpriteParams& params);
SDL_Surface* render_create_auto_tile_surface(SDL_Surface* tileset_surface, const SpriteParams& params);
SDL_Surface* render_load_font(FontName name);
int render_sort_surfaces_partition(LoadedSurface* surfaces, int low, int high);
void render_sort_surfaces(LoadedSurface* surfaces, int low, int high);
bool render_compile_shader(uint32_t* id, GLenum type, const char* path_suffix);
bool render_load_shader(uint32_t* id, const char* vertex_path, const char* fragment_path);

void render_update_screen_scale();
void render_flush_batch();

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
    gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress);
    if (glGenVertexArrays == NULL) {
        log_error("Error loading OpenGL.");
        return false;
    }

    GLint max_texture_size;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
    if (max_texture_size < ATLAS_SIZE) {
        log_error("Max texture size of %u is too small", max_texture_size);
        return false;
    }

    // Init quad VAO
    {
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

    // Init sprite VAO
    {
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

    // Init minimap texture
    {
        glGenTextures(1, &state.minimap_texture);

        glBindTexture(GL_TEXTURE_2D, state.minimap_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, MINIMAP_TEXTURE_WIDTH, MINIMAP_TEXTURE_HEIGHT, GL_FALSE, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glBindTexture(GL_TEXTURE_2D, 0);

        const SDL_PixelFormatDetails* format = SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_ABGR8888);
        state.minimap_pixel_values[MINIMAP_PIXEL_TRANSPARENT] = SDL_MapRGBA(format, NULL, 0, 0, 0, 0);
        state.minimap_pixel_values[MINIMAP_PIXEL_OFFBLACK] = SDL_MapRGBA(format, NULL, 40, 37, 45, 255);
        state.minimap_pixel_values[MINIMAP_PIXEL_OFFBLACK_TRANSPARENT] = SDL_MapRGBA(format, NULL, 40, 37, 45, 128);
        state.minimap_pixel_values[MINIMAP_PIXEL_WHITE] = SDL_MapRGBA(format, NULL, 255, 255, 255, 255);
        for (int player = 0; player < MAX_PLAYERS; player++) {
            SDL_Color player_color = RENDER_COLOR_VALUES.at(RENDER_PLAYER_COLORS[player]);
            state.minimap_pixel_values[MINIMAP_PIXEL_PLAYER0 + player] = SDL_MapRGBA(format, NULL, player_color.r, player_color.g, player_color.b, player_color.a);
        }
        state.minimap_pixel_values[MINIMAP_PIXEL_GOLD] = SDL_MapRGBA(format, NULL, 238, 209, 158, 255);
        state.minimap_pixel_values[MINIMAP_PIXEL_SAND] = SDL_MapRGBA(format, NULL, 204, 162, 139, 255);
        state.minimap_pixel_values[MINIMAP_PIXEL_WATER] = SDL_MapRGBA(format, NULL, 70, 100, 115, 255);
        state.minimap_pixel_values[MINIMAP_PIXEL_WALL] = SDL_MapRGBA(format, NULL, 94, 88, 89, 255);

        memset(state.minimap_texture_pixels, 0, sizeof(state.minimap_texture_pixels));
    }

    // Init screen framebuffer
    {
        glGenFramebuffers(1, &state.screen_framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, state.screen_framebuffer);

        glGenTextures(1, &state.screen_texture);
        glBindTexture(GL_TEXTURE_2D, state.screen_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, state.screen_texture, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            log_error("Screen framebuffer incomplete.");
            return false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glGenTextures(1, &state.screenshot_texture);
        glBindTexture(GL_TEXTURE_2D, state.screenshot_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }

    // Load screen shader
    if (!render_load_shader(&state.screen_shader, "screen.vert.glsl", "screen.frag.glsl")) {
        return false;
    }
    glUseProgram(state.screen_shader);
    glUniform1ui(glGetUniformLocation(state.screen_shader, "screen_texture"), 0);
    float screen_size[2] = { (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT };
    glUniform2fv(glGetUniformLocation(state.screen_shader, "screen_size"), 1, screen_size);

    // Load sprite shader
    if (!render_load_shader(&state.sprite_shader, "sprite.vert.glsl", "sprite.frag.glsl")) {
        return false;
    }
    glUseProgram(state.sprite_shader);
    glUniform1ui(glGetUniformLocation(state.sprite_shader, "sprite_texture_array"), 0);
    mat4 projection = mat4_ortho(0.0f, (float)SCREEN_WIDTH, 0.0f, (float)SCREEN_HEIGHT, 0.0f, 1.0f);
    glUniformMatrix4fv(glGetUniformLocation(state.sprite_shader, "projection"), 1, GL_FALSE, projection.data);

    // Load minimap shader
    if (!render_load_shader(&state.minimap_shader, "sprite.vert.glsl", "minimap.frag.glsl")) {
        return false;
    }
    glUseProgram(state.minimap_shader);
    glUniform1ui(glGetUniformLocation(state.minimap_shader, "sprite_texture"), 0);
    glUniformMatrix4fv(glGetUniformLocation(state.minimap_shader, "projection"), 1, GL_FALSE, projection.data);

    render_load_sprites();

    render_update_screen_scale();
    SDL_GL_SetSwapInterval(1);

    log_info("Initialized renderer. Vendor: %s. Renderer: %s. Version: %s.", glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION));
    return true;
}

bool render_load_sprites() {
    // First, load all of the surfaces
    LoadedSurface surfaces[(int)FONT_COUNT + (int)SPRITE_COUNT];

    // Load the font surfaces
    for (int font = 0; font < FONT_COUNT; font++) {
        surfaces[font].surface = render_load_font((FontName)font);
        if (surfaces[font].surface == NULL) {
            return false;
        }
        surfaces[font].type = LOADED_SURFACE_FONT;
        surfaces[font].name = font;
    }

    // Load the tileset surfaces because we'll need them for the tile sprites
    SDL_Surface* tileset_surfaces[TILESET_COUNT];
    for (int tileset = 0; tileset < TILESET_COUNT; tileset++) {
        const TilesetParams& params = render_get_tileset_params((Tileset)tileset);
        std::string tileset_path = filesystem_get_resource_path() + "sprite/" + params.path;
        tileset_surfaces[tileset] = IMG_Load(tileset_path.c_str());
        if (tileset_surfaces[tileset] == NULL) {
            log_error("Unable to load tileset %s: %s", tileset_path.c_str(), SDL_GetError());
            return false;
        }
    }

    // Load the sprite surfaces
    for (int sprite = 0; sprite < SPRITE_COUNT; sprite++) {
        const SpriteParams& params = render_get_sprite_params((SpriteName)sprite);
        surfaces[FONT_COUNT + sprite].type = LOADED_SURFACE_SPRITE;
        surfaces[FONT_COUNT + sprite].name = sprite;
        if (params.strategy == SPRITE_IMPORT_TILE) {
            SDL_Surface* tile_surface = params.tile.type == TILE_TYPE_SINGLE
                                                ? render_create_single_tile_surface(tileset_surfaces[params.tile.tileset], params)
                                                : render_create_auto_tile_surface(tileset_surfaces[params.tile.tileset], params);
            if (tile_surface == NULL) {
                return false;
            }

            surfaces[FONT_COUNT + sprite].surface = tile_surface;
        } else if (params.strategy == SPRITE_IMPORT_SWATCH) {
            SDL_Surface* swatch_surface = SDL_CreateSurface(RENDER_COLOR_COUNT, 1, SDL_PIXELFORMAT_BGRA8888);
            if (swatch_surface == NULL) {
                log_error("Unable to create swatch surface: %s", SDL_GetError());
                return false;
            }
            const SDL_PixelFormatDetails* format_details = SDL_GetPixelFormatDetails(swatch_surface->format);
            uint32_t* swatch_pixels = (uint32_t*)swatch_surface->pixels;
            for (uint32_t render_color = 0; render_color < RENDER_COLOR_COUNT; render_color++) {
                SDL_Color value = RENDER_COLOR_VALUES.at((RenderColor)render_color);
                uint32_t color_value = SDL_MapRGBA(format_details, NULL, value.r, value.g, value.b, value.a);
                swatch_pixels[render_color] = color_value;
            }

            surfaces[FONT_COUNT + sprite].surface = swatch_surface;
        } else {
            std::string sprite_path = filesystem_get_resource_path() + "sprite/" + params.sheet.path;
            SDL_Surface* sprite_surface = IMG_Load(sprite_path.c_str());
            if (sprite_surface == NULL) {
                log_error("Unable to load sprite %s: %s", sprite_path.c_str(), SDL_GetError());
                return false;
            }

            if (params.strategy == SPRITE_IMPORT_RECOLOR || params.strategy == SPRITE_IMPORT_RECOLOR_AND_LOW_ALPHA) {
                sprite_surface = render_recolor_surface(sprite_surface, params.strategy == SPRITE_IMPORT_RECOLOR_AND_LOW_ALPHA);
                if (sprite_surface == NULL) {
                    return false;
                }
            }
            surfaces[FONT_COUNT + sprite].surface = sprite_surface;
        }
        SDL_SetSurfaceBlendMode(surfaces[FONT_COUNT + sprite].surface, SDL_BLENDMODE_NONE);
    }

    // Sort the surfaces by size, from biggest to smallest
    render_sort_surfaces(surfaces, 0, (int)FONT_COUNT + (int)SPRITE_COUNT - 1);

    // Setup a list to keep track of which surfaces have been stored
    bool surface_has_been_stored[(int)FONT_COUNT + (int)SPRITE_COUNT];
    memset(surface_has_been_stored, 0, sizeof(surface_has_been_stored));
    uint32_t surface_stored_count = 0;

    // Place the surfaces inside packed texture atlases
    std::vector<SDL_Surface*> atlas_surfaces;
    while (surface_stored_count < (int)FONT_COUNT + (int)SPRITE_COUNT) {
        // Create the atlas surface
        SDL_Surface* atlas_surface = SDL_CreateSurface(ATLAS_SIZE, ATLAS_SIZE, SDL_PIXELFORMAT_ABGR8888);
        if (atlas_surface == NULL) {
            log_error("Error creating atlas surface: %s", SDL_GetError());
            return false;
        }

        // Store of a list of empty spaces inside the atlas
        std::vector<Rect> empty_spaces;
        empty_spaces.push_back({ .x = 0, .y = 0, .w = ATLAS_SIZE, .h = ATLAS_SIZE });

        // For each surface that still needs to be stored, search through the empty spaces list to find a place for the surface to go
        for (int surface_index = 0; surface_index < (int)FONT_COUNT + (int)SPRITE_COUNT; surface_index++) {
            if (surface_has_been_stored[surface_index]) {
                continue;
            }
            SDL_Surface* surface = surfaces[surface_index].surface;

            // Search through the empty spaces backwards so that we choose the smallest one that will fit first
            int space_index_int;
            for (space_index_int = (int)empty_spaces.size() - 1; space_index_int >= 0; space_index_int--) {
                if (surface->w <= empty_spaces[(size_t)space_index_int].w && surface->h <= empty_spaces[(size_t)space_index_int].h) {
                    break;
                }
            }
            // No space was found, so skip this surface (it will be rendered to a different atlas)
            if (space_index_int == -1) {
                continue;
            }
            size_t space_index = (size_t)space_index_int;

            // Render the surface onto the chosen empty space
            SDL_Rect dst_rect = (SDL_Rect) { 
                .x = empty_spaces[space_index].x,
                .y = empty_spaces[space_index].y,
                .w = surface->w, .h = surface->h
            };
            SDL_BlitSurface(surface, NULL, atlas_surface, &dst_rect);

            // Set sprite info for this surface
            if (surfaces[surface_index].type == LOADED_SURFACE_FONT) {
                int font_name = surfaces[surface_index].name;
                state.fonts[font_name].atlas = (int)atlas_surfaces.size();
                state.fonts[font_name].atlas_x = empty_spaces[space_index].x;
                state.fonts[font_name].atlas_y = empty_spaces[space_index].y;
            } else {
                int sprite_name = surfaces[surface_index].name;
                const SpriteParams& params = render_get_sprite_params((SpriteName)sprite_name);
                SpriteInfo sprite_info; 
                sprite_info.atlas = (int)atlas_surfaces.size();
                sprite_info.atlas_x = empty_spaces[space_index].x;
                sprite_info.atlas_y = empty_spaces[space_index].y;
                if (params.strategy == SPRITE_IMPORT_TILE) {
                    if (params.tile.type == TILE_TYPE_SINGLE) {
                        sprite_info.hframes = 1;
                        sprite_info.vframes = 1;
                    } else if (params.tile.type == TILE_TYPE_AUTO) {
                        sprite_info.hframes = AUTOTILE_HFRAMES;
                        sprite_info.vframes = AUTOTILE_VFRAMES;
                    }
                    sprite_info.frame_width = TILE_SRC_SIZE;
                    sprite_info.frame_height = TILE_SRC_SIZE;
                } else if (params.strategy == SPRITE_IMPORT_SWATCH) {
                    sprite_info.hframes = RENDER_COLOR_COUNT;
                    sprite_info.vframes = 1;
                    sprite_info.frame_width = 1;
                    sprite_info.frame_height = 1;
                } else {
                    sprite_info.hframes = params.sheet.hframes;
                    sprite_info.vframes = params.sheet.vframes;
                    sprite_info.frame_width = surface->w / sprite_info.hframes;
                    sprite_info.frame_height = (params.strategy == SPRITE_IMPORT_RECOLOR || params.strategy == SPRITE_IMPORT_RECOLOR_AND_LOW_ALPHA)
                                                    ? surface->h / (sprite_info.vframes * MAX_PLAYERS)
                                                    : surface->h / sprite_info.vframes;
                }
                state.sprite_info[sprite_name] = sprite_info;
            }
            surface_has_been_stored[surface_index] = true;
            surface_stored_count++;

            // Split the empty space
            Rect vsplit;
            vsplit.x = -1; // mark as uninitialized
            if (surface->w < empty_spaces[space_index].w) {
                vsplit = (Rect) {
                    .x = empty_spaces[space_index].x + surface->w,
                    .y = empty_spaces[space_index].y,
                    .w = empty_spaces[space_index].w - surface->w,
                    .h = surface->h
                };
            }

            Rect hsplit;
            hsplit.x = -1;
            if (surface->h < empty_spaces[space_index].h) {
                hsplit = (Rect) {
                    .x = empty_spaces[space_index].x,
                    .y = empty_spaces[space_index].y + surface->h,
                    .w = empty_spaces[space_index].w,
                    .h = empty_spaces[space_index].h - surface->h
                };
            }

            // Remove the empty space by swapping and popping
            empty_spaces[space_index] = empty_spaces.back();
            empty_spaces.pop_back();

            // Add the splits to the list
            // Note: it is possible for no splits to be added if the surface was a perfect fit
            if (vsplit.x != -1 && hsplit.x != -1) {
                // Push back the bigger of the two first
                if (vsplit.w * vsplit.h >= hsplit.w * hsplit.h) {
                    empty_spaces.push_back(vsplit);
                    empty_spaces.push_back(hsplit);
                } else {
                    empty_spaces.push_back(hsplit);
                    empty_spaces.push_back(vsplit);
                }
            } else if (vsplit.x != -1) {
                empty_spaces.push_back(vsplit);
            } else if (hsplit.x != -1) {
                empty_spaces.push_back(hsplit);
            }
        } // End for each surface

        render_flip_sdl_surface_vertically(atlas_surface);
        atlas_surfaces.push_back(atlas_surface);
    }

    glGenTextures(1, &state.sprite_texture_array);
    glBindTexture(GL_TEXTURE_2D_ARRAY, state.sprite_texture_array);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, ATLAS_SIZE, ATLAS_SIZE, (GLsizei)atlas_surfaces.size(), 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    for (size_t atlas = 0; atlas < atlas_surfaces.size(); atlas++) {
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, (GLint)atlas, ATLAS_SIZE, ATLAS_SIZE, 1, GL_RGBA, GL_UNSIGNED_BYTE, atlas_surfaces[atlas]->pixels);
        SDL_DestroySurface(atlas_surfaces[atlas]);
    }

    // Cleanup 
    for (int surface_index = 0; surface_index < (int)FONT_COUNT + (int)SPRITE_COUNT; surface_index++) {
        SDL_DestroySurface(surfaces[surface_index].surface);
    }
    for (int tileset = 0; tileset < TILESET_COUNT; tileset++) {
        SDL_DestroySurface(tileset_surfaces[tileset]);
    }
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    return true;
}

void render_flip_sdl_surface_vertically(SDL_Surface* surface) {
    SDL_LockSurface(surface);
    int sprite_surface_pitch = surface->pitch;
    uint8_t* temp = (uint8_t*)malloc((size_t)sprite_surface_pitch);
    uint8_t* sprite_surface_pixels = (uint8_t*)surface->pixels;

    for (int row = 0; row < surface->h / 2; ++row) {
        uint8_t* row1 = sprite_surface_pixels + (row * sprite_surface_pitch);
        uint8_t* row2 = sprite_surface_pixels + ((surface->h - row - 1) * sprite_surface_pitch);

        memcpy(temp, row1, (size_t)sprite_surface_pitch);
        memcpy(row1, row2, (size_t)sprite_surface_pitch);
        memcpy(row2, temp, (size_t)sprite_surface_pitch);
    }
    free(temp);
    SDL_UnlockSurface(surface);
}

SDL_Surface* render_recolor_surface(SDL_Surface* sprite_surface, bool recolor_low_alpha) {
    // Create a surface big enough to hold the recolor atlas
    SDL_Surface* recolor_surface = SDL_CreateSurface(sprite_surface->w, sprite_surface->h * MAX_PLAYERS, sprite_surface->format);
    if (recolor_surface == NULL) {
        log_error("Error creating recolor surface for sprite: %s", SDL_GetError());
        return NULL;
    }

    // Get the reference pixels into a packed byte
    const SDL_PixelFormatDetails* format_details = SDL_GetPixelFormatDetails(sprite_surface->format);
    uint32_t clothes_reference_pixel = SDL_MapRGBA(format_details, NULL, RECOLOR_CLOTHES_REF.r, RECOLOR_CLOTHES_REF.g, RECOLOR_CLOTHES_REF.b, RECOLOR_CLOTHES_REF.a);
    uint32_t skin_reference_pixel = SDL_MapRGBA(format_details, NULL, RECOLOR_SKIN_REF.r, RECOLOR_SKIN_REF.g, RECOLOR_SKIN_REF.b, RECOLOR_SKIN_REF.a);

    // Lock the surface so that we can edit pixel values
    SDL_LockSurface(recolor_surface);
    uint32_t* recolor_surface_pixels = (uint32_t*)recolor_surface->pixels;
    uint32_t* sprite_surface_pixels = (uint32_t*)sprite_surface->pixels;

    // Copy the original sprite onto our recolor atlas 4 times, once for each player color
    for (int recolor_id = 0; recolor_id < MAX_PLAYERS; recolor_id++) {
        // Get the replacement pixel bytes from the player color
        SDL_Color player_color = RENDER_COLOR_VALUES.at(RENDER_PLAYER_COLORS[recolor_id]);
        uint32_t clothes_replacement_pixel = SDL_MapRGBA(format_details, NULL, player_color.r, player_color.g, player_color.b, player_color.a);
        SDL_Color skin_color = RENDER_COLOR_VALUES.at(RENDER_PLAYER_SKIN_COLORS[recolor_id]);
        uint32_t skin_replacement_pixel = SDL_MapRGBA(format_details, NULL, skin_color.r, skin_color.g, skin_color.b, skin_color.a);

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
                if (recolor_low_alpha) {
                    uint8_t r, g, b, a;
                    SDL_GetRGBA(source_pixel, format_details, NULL, &r, &g, &b, &a);
                    if (a != 0) {
                        a = 200;
                    }
                    source_pixel = SDL_MapRGBA(format_details, NULL, r, g, b, a);
                }

                // Put the source pixel onto the recolor surface
                recolor_surface_pixels[((y + (sprite_surface->h * recolor_id)) * recolor_surface->w) + x] = source_pixel;
            }
        }
    }

    SDL_UnlockSurface(recolor_surface);

    // Handoff the recolor surface into the sprite surface variable
    // Allows the rest of the sprite loading to work the same as with normal sprites
    SDL_DestroySurface(sprite_surface);
    return recolor_surface;
}

SDL_Surface* render_create_single_tile_surface(SDL_Surface* tileset_surface, const SpriteParams& params) {
    SDL_Surface* tile_surface = SDL_CreateSurface(TILE_SRC_SIZE, TILE_SRC_SIZE, tileset_surface->format);
    if (tile_surface == NULL) {
        log_error("Could not create single tile surface: %s", SDL_GetError());
        return NULL;
    }

    SDL_Rect src_rect = (SDL_Rect) { 
        .x = params.tile.source_x, 
        .y = params.tile.source_y,
        .w = TILE_SRC_SIZE,
        .h = TILE_SRC_SIZE
    };
    SDL_Rect dst_rect = (SDL_Rect) {
        .x = 0, .y = 0,
        .w = TILE_SRC_SIZE, .h = TILE_SRC_SIZE
    };

    SDL_BlitSurface(tileset_surface, &src_rect, tile_surface, &dst_rect);

    return tile_surface;
}

SDL_Surface* render_create_auto_tile_surface(SDL_Surface* tileset_surface, const SpriteParams& params) {
    SDL_Surface* tile_surface = SDL_CreateSurface(TILE_SRC_SIZE * AUTOTILE_HFRAMES, TILE_SRC_SIZE * AUTOTILE_VFRAMES, tileset_surface->format);
    if (tile_surface == NULL) {
        log_error("Could not create auto tile surface: %s", SDL_GetError());
        return NULL;
    }

    // To generate an autotile, we iterate through each combination of neighbors
    ivec2 autotile_frame = ivec2(0, 0);
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
            ivec2 edge_source_pos = ivec2(params.tile.source_x, params.tile.source_y) + (autotile_edge_lookup(edge, neighbors & AUTOTILE_EDGE_MASK[edge]) * (TILE_SRC_SIZE / 2));
            SDL_Rect subtile_src_rect = (SDL_Rect) {
                .x = edge_source_pos.x,
                .y = edge_source_pos.y,
                .w = TILE_SRC_SIZE / 2,
                .h = TILE_SRC_SIZE / 2
            };
            SDL_Rect subtile_dst_rect = (SDL_Rect) {
                .x = (autotile_frame.x * TILE_SRC_SIZE) + (AUTOTILE_EDGE_OFFSETS[edge].x * (TILE_SRC_SIZE / 2)),
                .y = (autotile_frame.y * TILE_SRC_SIZE) + (AUTOTILE_EDGE_OFFSETS[edge].y * (TILE_SRC_SIZE / 2)),
                .w = TILE_SRC_SIZE / 2,
                .h = TILE_SRC_SIZE / 2
            };

            SDL_BlitSurface(tileset_surface, &subtile_src_rect, tile_surface, &subtile_dst_rect);
        } 

        autotile_frame.x++;
        if (autotile_frame.x == AUTOTILE_HFRAMES) {
            autotile_frame.x = 0;
            autotile_frame.y++;
        }
    } // End for each neighbor combo

    return tile_surface;
}

SDL_Surface* render_load_font(FontName name) {
    const FontParams& params = resource_get_font_params((FontName)name);
    Font& font = state.fonts[name];

    // Open the font
    std::string font_path = filesystem_get_resource_path() + "font/" + params.path;
    TTF_Font* ttf_font = TTF_OpenFont(font_path.c_str(), (float)params.size);
    if (ttf_font == NULL) {
        log_error("Unable to open font %s: %s", font_path.c_str(), SDL_GetError());
        return NULL;
    }

    // Render each glyph to a surface
    SDL_Surface* glyphs[FONT_GLYPH_COUNT];
    ivec2 glyph_surface_position = ivec2(0, 0);
    int glyph_max_width = 0;
    int glyph_max_height = 0;

    SDL_Color font_color = (SDL_Color) { .r = params.r, .g = params.g, .b = params.b, .a = 255 };
    for (uint32_t glyph_index = 0; glyph_index < FONT_GLYPH_COUNT; glyph_index++) {
        char text[2] = { (char)(FONT_FIRST_CHAR + glyph_index), '\0'};
        glyphs[glyph_index] = TTF_RenderText_Solid(ttf_font, text, 0, font_color);
        if (glyphs[glyph_index] == NULL) {
            log_error("Error rendering glyph %s for font %s: %s", text, font_path.c_str(), SDL_GetError());
            return NULL;
        }

        glyph_max_width = std::max(glyph_max_width, glyphs[glyph_index]->w);
        glyph_max_height = std::max(glyph_max_height, glyphs[glyph_index]->h);
    }

    // Create a surface to render each glyph onto
    SDL_Surface* font_surface = SDL_CreateSurface(glyph_max_width * FONT_HFRAMES, glyph_max_height * FONT_VFRAMES, SDL_PIXELFORMAT_RGBA8888);
    if (font_surface == NULL) {
        log_error("Error creating font surface: %s", SDL_GetError());
        return NULL;
    }

    // Render each surface glyph onto a single atlas surface
    for (int glyph_index = 0; glyph_index < FONT_GLYPH_COUNT; glyph_index++) {
        int glyph_index_x = glyph_index % FONT_HFRAMES;
        int glyph_index_y = glyph_index / FONT_HFRAMES;

        SDL_Rect dest_rect = (SDL_Rect) { 
            .x = glyph_index_x * glyph_max_width, 
            .y = glyph_index_y * glyph_max_height, 
            .w = glyphs[glyph_index]->w, 
            .h = glyphs[glyph_index]->h 
        };
        if (!SDL_BlitSurface(glyphs[glyph_index], NULL, font_surface, &dest_rect)) {
            log_error("Error blitting surface: %s", SDL_GetError());
        }
    }

    // Finish filling out the font struct
    font.glyph_width = glyph_max_width;
    font.glyph_height = glyph_max_height;
    for (uint32_t glyph_index = 0; glyph_index < FONT_GLYPH_COUNT; glyph_index++) {
        int bearing_y;
        TTF_GetGlyphMetrics(ttf_font, FONT_FIRST_CHAR + glyph_index, &font.glyphs[glyph_index].bearing_x, NULL, NULL, &bearing_y, &font.glyphs[glyph_index].advance);
        font.glyphs[glyph_index].bearing_y = glyph_max_height - bearing_y;
        if (params.strategy == FONT_IMPORT_IGNORE_BEARING) {
            font.glyphs[glyph_index].bearing_x = 0;
            font.glyphs[glyph_index].bearing_y = 0;
        }
    }

    // Free all the resources
    for (int glyph_index = 0; glyph_index < FONT_GLYPH_COUNT; glyph_index++) {
        SDL_DestroySurface(glyphs[glyph_index]);
    }
    TTF_CloseFont(ttf_font);

    return font_surface;
}

int render_sort_surfaces_partition(LoadedSurface* surfaces, int low, int high) {
    LoadedSurface pivot = surfaces[high];
    int i = low - 1;

    for (int j = low; j <= high - 1; j++) {
        if (surfaces[j].surface->w * surfaces[j].surface->h > pivot.surface->w * pivot.surface->h) {
            i++;
            LoadedSurface temp = surfaces[j];
            surfaces[j] = surfaces[i];
            surfaces[i] = temp;
        }
    }

    LoadedSurface temp = surfaces[high];
    surfaces[high] = surfaces[i + 1];
    surfaces[i + 1] = temp;

    return i + 1;
}

void render_sort_surfaces(LoadedSurface* surfaces, int low, int high) {
    if (low < high) {
        int partition_index = render_sort_surfaces_partition(surfaces, low, high);
        render_sort_surfaces(surfaces, low, partition_index - 1);
        render_sort_surfaces(surfaces, partition_index + 1, high);
    }
}

bool render_compile_shader(uint32_t* id, GLenum type, const char* path_suffix) {
    // Read the shader file
    std::string path = filesystem_get_resource_path() + "shader/" + path_suffix;
    FILE* shader_file = fopen(path.c_str(), "rb");
    if (!shader_file) {
        log_error("Unable to open shader file at path %s.", path.c_str());
        return false;
    }

    fseek(shader_file, 0, SEEK_END);
    size_t file_size = (size_t)ftell(shader_file);
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
        log_error("Shader %s failed to compile: %s", path.c_str(), info_log);
        return false;
    }

    free(shader_source);

    return true;
}

bool render_load_shader(uint32_t* id, const char* vertex_path, const char* fragment_path) {
    // Compile shaders
    GLuint vertex_shader;
    if (!render_compile_shader(&vertex_shader, GL_VERTEX_SHADER, vertex_path)) {
        return false;
    }
    GLuint fragment_shader;
    if (!render_compile_shader(&fragment_shader, GL_FRAGMENT_SHADER, fragment_path)) {
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

void render_quit() {
    SDL_GL_DestroyContext(state.context);
    log_info("Quit renderer.");
}

void render_update_screen_scale() {
    ivec2 window_size;
    SDL_GetWindowSize(state.window, &window_size.x, &window_size.y);

    float screen_scale[2];
    float screen_aspect = (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT;
    float window_aspect = (float)window_size.x / (float)window_size.y;
    if (screen_aspect >= window_aspect) {
        // Letterbox
        float ratio = (float)window_size.x / (float)SCREEN_WIDTH;
        float scaled_height = (float)SCREEN_HEIGHT * ratio;
        screen_scale[0] = 1.0f;
        screen_scale[1] = scaled_height / (float)window_size.y;
    } else {
        // Pillarbox
        float ratio = (float)window_size.y / (float)SCREEN_HEIGHT;
        float scaled_width = (float)SCREEN_WIDTH * ratio;
        screen_scale[0] = scaled_width / (float)window_size.x;
        screen_scale[1] = 1.0f;
    }

    glUseProgram(state.screen_shader);
    glUniform2fv(glGetUniformLocation(state.screen_shader, "screen_scale"), 1, screen_scale);
}

void render_set_display(RenderDisplay display) {
    SDL_DisplayID* display_id = SDL_GetDisplays(NULL);
    const SDL_DisplayMode* display_mode = SDL_GetDesktopDisplayMode(display_id[0]);

    SDL_SyncWindow(state.window);

    if (display == RENDER_DISPLAY_WINDOWED) {
        SDL_SetWindowFullscreen(state.window, false);
        SDL_SyncWindow(state.window);
        SDL_SetWindowSize(state.window, 1280, 720);
        SDL_SetWindowPosition(state.window, (display_mode->w / 2) - (1280 / 2), (display_mode->h / 2) - (720 / 2));
        SDL_SyncWindow(state.window);
        render_update_screen_scale();
    } else {
        SDL_SetWindowSize(state.window, display_mode->w, display_mode->h);
        SDL_SyncWindow(state.window);
        render_update_screen_scale();
        SDL_SetWindowFullscreen(state.window, true);
    }
}

void render_set_vsync(RenderVsync vsync) {
    int interval = vsync == RENDER_VSYNC_ADAPTIVE ? -1 : (int)vsync;
    int result = SDL_GL_SetSwapInterval(interval);
    if (result == -1) {
        log_error("GL set swap interval failed. Defaulting to vsync on");
        SDL_GL_SetSwapInterval(1);
    }
}

void render_prepare_frame() {
    glBindFramebuffer(GL_FRAMEBUFFER, state.screen_framebuffer);
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    state.sprite_vertices.clear();
}

void render_present_frame() {
    render_flush_batch();

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

void render_flush_batch() {
    if (state.sprite_vertices.empty()) {
        return;
    }
    GOLD_ASSERT(state.sprite_vertices.size() <= MAX_BATCH_VERTICES);

    // Buffer sprite batch data
    glBindVertexArray(state.sprite_vao);
    glBindBuffer(GL_ARRAY_BUFFER, state.sprite_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(state.sprite_vertices.size() * sizeof(SpriteVertex)), &state.sprite_vertices[0]);

    // Render sprite batch
    glUseProgram(state.sprite_shader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, state.sprite_texture_array);

    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)state.sprite_vertices.size());

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    state.sprite_vertices.clear();
}

const SpriteInfo& render_get_sprite_info(SpriteName name) {
    return state.sprite_info[name];
}

void render_sprite_frame(SpriteName name, ivec2 frame, ivec2 position, uint32_t options, int recolor_id) {
    const SpriteInfo& sprite_info = render_get_sprite_info(name);
    GOLD_ASSERT(frame.x <= sprite_info.frame_width && frame.y <= sprite_info.frame_height);

    Rect src_rect = (Rect) { 
        .x = frame.x * sprite_info.frame_width, 
        .y = (recolor_id * sprite_info.frame_height * sprite_info.vframes) + (frame.y * sprite_info.frame_height), 
        .w = sprite_info.frame_width,
        .h = sprite_info.frame_height
    };
    Rect dst_rect = (Rect) { 
        .x = position.x, 
        .y = position.y, 
        .w = sprite_info.frame_width * RENDER_SCALE_INT,
        .h = sprite_info.frame_height * RENDER_SCALE_INT
    };
    render_sprite(name, src_rect, dst_rect, options);
}

void render_ninepatch(SpriteName sprite, Rect rect) {
    const SpriteInfo& sprite_info = render_get_sprite_info(sprite);

    Rect src_rect = (Rect) {
        .x = 0,
        .y = 0,
        .w = sprite_info.frame_width,
        .h = sprite_info.frame_height
    };
    Rect dst_rect = (Rect) {
        .x = rect.x,
        .y = rect.y,
        .w = sprite_info.frame_width * RENDER_SCALE_INT,
        .h = sprite_info.frame_height * RENDER_SCALE_INT
    };

    // Top left
    render_sprite(sprite, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);

    // Top right
    src_rect.x = 0 + (2 * sprite_info.frame_width);
    dst_rect.x = rect.x + rect.w - (sprite_info.frame_width * RENDER_SCALE_INT);
    render_sprite(sprite, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);

    // Bottom left
    src_rect.x = 0;
    src_rect.y = 0 + (2 * sprite_info.frame_height);
    dst_rect.x = rect.x;
    dst_rect.y = rect.y + rect.h - (sprite_info.frame_height * RENDER_SCALE_INT);
    render_sprite(sprite, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);

    // Bottom right
    src_rect.x = 0 + (2 * sprite_info.frame_width);
    dst_rect.x = rect.x + rect.w - (sprite_info.frame_width * RENDER_SCALE_INT);
    render_sprite(sprite, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);

    // Top edge
    src_rect.x = 0 + sprite_info.frame_width;
    src_rect.y = 0;
    dst_rect.x = rect.x + (sprite_info.frame_width * 2);
    dst_rect.y = rect.y;
    dst_rect.w = (rect.x + rect.w - (sprite_info.frame_width * RENDER_SCALE_INT)) - dst_rect.x;
    render_sprite(sprite, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);

    // Bottom edge
    src_rect.y = 0 + (2 * sprite_info.frame_height);
    dst_rect.y = (rect.y + rect.h - (sprite_info.frame_height * RENDER_SCALE_INT));
    render_sprite(sprite, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);

    // Left edge
    src_rect.x = 0;
    src_rect.y = 0 + sprite_info.frame_height;
    dst_rect.x = rect.x;
    dst_rect.w = sprite_info.frame_width * RENDER_SCALE_INT;
    dst_rect.y = rect.y + (sprite_info.frame_height * RENDER_SCALE_INT);
    dst_rect.h = (rect.y + rect.h - (sprite_info.frame_height * RENDER_SCALE_INT)) - dst_rect.y;
    render_sprite(sprite, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);

    // Right edge
    src_rect.x = 0 + (2 * sprite_info.frame_width);
    dst_rect.x = rect.x + rect.w - (sprite_info.frame_width * RENDER_SCALE_INT);
    render_sprite(sprite, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);

    // Center
    src_rect.x = 0 + sprite_info.frame_width;
    src_rect.y = 0 + sprite_info.frame_height;
    dst_rect.x = rect.x + (sprite_info.frame_width * RENDER_SCALE_INT);
    dst_rect.y = rect.y + (sprite_info.frame_height * RENDER_SCALE_INT);
    dst_rect.w = (rect.x + rect.w - (sprite_info.frame_width * RENDER_SCALE_INT)) - dst_rect.x;
    dst_rect.h = (rect.y + rect.h - (sprite_info.frame_height * RENDER_SCALE_INT)) - dst_rect.y;
    render_sprite(sprite, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);
}

void render_vertical_line(int x, int start_y, int end_y, RenderColor color) {
    const SpriteInfo& sprite_info = render_get_sprite_info(SPRITE_UI_SWATCH);
    Rect src_rect = (Rect) {
        .x = sprite_info.frame_width * color,
        .y = 0,
        .w = sprite_info.frame_width,
        .h = sprite_info.frame_height
    };
    Rect dst_rect = (Rect) {
        .x = x, .y = start_y,
        .w = RENDER_SCALE_INT, .h = end_y - start_y
    };
    render_sprite(SPRITE_UI_SWATCH, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);
}

void render_draw_rect(Rect rect, RenderColor color) {
    const SpriteInfo& sprite_info = render_get_sprite_info(SPRITE_UI_SWATCH);
    Rect src_rect = (Rect) {
        .x = sprite_info.frame_width * color,
        .y = 0,
        .w = sprite_info.frame_width,
        .h = sprite_info.frame_height
    };
    Rect dst_rect = (Rect) {
        .x = rect.x, .y = rect.y,
        .w = RENDER_SCALE_INT, .h = rect.h
    };
    render_sprite(SPRITE_UI_SWATCH, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);
    dst_rect.x = rect.x + rect.w - RENDER_SCALE_INT;
    render_sprite(SPRITE_UI_SWATCH, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);
    dst_rect.x = rect.x;
    dst_rect.w = rect.w;
    dst_rect.h = RENDER_SCALE_INT;
    render_sprite(SPRITE_UI_SWATCH, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);
    dst_rect.y = rect.y + rect.h - RENDER_SCALE_INT;
    render_sprite(SPRITE_UI_SWATCH, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);
}

void render_fill_rect(Rect rect, RenderColor color) {
    const SpriteInfo& sprite_info = render_get_sprite_info(SPRITE_UI_SWATCH);
    Rect src_rect = (Rect) {
        .x = sprite_info.frame_width * color,
        .y = 0,
        .w = sprite_info.frame_width,
        .h = sprite_info.frame_height
    };
    render_sprite(SPRITE_UI_SWATCH, src_rect, rect, RENDER_SPRITE_NO_CULL);
}

void render_sprite(SpriteName sprite, Rect src_rect, Rect dst_rect, uint32_t options) {
    if (state.sprite_vertices.size() + 6 > MAX_BATCH_VERTICES) {
        render_flush_batch();
    }

    const SpriteInfo& sprite_info = render_get_sprite_info(sprite);

    src_rect.x += sprite_info.atlas_x;
    src_rect.y += sprite_info.atlas_y;

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

    float tex_coord_left = (float)src_rect.x / (float)ATLAS_SIZE;
    float tex_coord_right = tex_coord_left + ((float)src_rect.w / (float)ATLAS_SIZE);
    float tex_coord_top = 1.0f - ((float)src_rect.y / (float)ATLAS_SIZE);
    float tex_coord_bottom = tex_coord_top - ((float)src_rect.h / (float)ATLAS_SIZE);

    if (flip_h) {
        float temp = tex_coord_left;
        tex_coord_left = tex_coord_right;
        tex_coord_right = temp;
    }

    float atlas = (float)sprite_info.atlas;
    state.sprite_vertices.push_back((SpriteVertex) {
        .position = { position_left, position_top },
        .tex_coord = { tex_coord_left, tex_coord_top, atlas }
    });
    state.sprite_vertices.push_back((SpriteVertex) {
        .position = { position_right, position_bottom },
        .tex_coord = { tex_coord_right, tex_coord_bottom, atlas }
    });
    state.sprite_vertices.push_back((SpriteVertex) {
        .position = { position_left, position_bottom },
        .tex_coord = { tex_coord_left, tex_coord_bottom, atlas }
    });
    state.sprite_vertices.push_back((SpriteVertex) {
        .position = { position_left, position_top },
        .tex_coord = { tex_coord_left, tex_coord_top, atlas }
    });
    state.sprite_vertices.push_back((SpriteVertex) {
        .position = { position_right, position_top },
        .tex_coord = { tex_coord_right, tex_coord_top, atlas }
    });
    state.sprite_vertices.push_back((SpriteVertex) {
        .position = { position_right, position_bottom },
        .tex_coord = { tex_coord_right, tex_coord_bottom, atlas }
    });
}

void render_text(FontName name, const char* text, ivec2 position) {
    if (state.sprite_vertices.size() + 6 > MAX_BATCH_VERTICES) {
        render_flush_batch();
    }

    ivec2 glyph_position = position;
    size_t text_index = 0;

    float frame_size_x = (float)(state.fonts[name].glyph_width) / (float)ATLAS_SIZE;
    float frame_size_y = (float)(state.fonts[name].glyph_height) / (float)ATLAS_SIZE;

    while (text[text_index] != '\0') {
        uint32_t glyph_index = (uint32_t)text[text_index] - FONT_FIRST_CHAR;
        if (glyph_index < 0 || glyph_index >= FONT_GLYPH_COUNT) {
            glyph_index = (uint32_t)('|' - FONT_FIRST_CHAR);
        }

        float position_top = (float)(SCREEN_HEIGHT - (glyph_position.y + (state.fonts[name].glyphs[glyph_index].bearing_y * RENDER_SCALE_FLOAT)));
        float position_bottom = position_top - ((float)state.fonts[name].glyph_height * RENDER_SCALE_FLOAT);
        float position_left = (float)(glyph_position.x + (state.fonts[name].glyphs[glyph_index].bearing_x * RENDER_SCALE_FLOAT));
        float position_right = position_left + ((float)state.fonts[name].glyph_width * RENDER_SCALE_FLOAT);

        ivec2 glyph_frame = ivec2(glyph_index % FONT_HFRAMES, glyph_index / FONT_HFRAMES);
        float tex_coord_left = (float)(state.fonts[name].atlas_x + (glyph_frame.x * state.fonts[name].glyph_width)) / (float)ATLAS_SIZE;
        float tex_coord_right = tex_coord_left + frame_size_x;
        float tex_coord_top = 1.0f - ((float)(state.fonts[name].atlas_y + (glyph_frame.y * state.fonts[name].glyph_height)) / (float)ATLAS_SIZE);
        float tex_coord_bottom = tex_coord_top - frame_size_y;

        float font_atlas = (float)state.fonts[name].atlas;
        state.sprite_vertices.push_back((SpriteVertex) {
            .position = { position_left, position_top },
            .tex_coord = { tex_coord_left, tex_coord_top, font_atlas }
        });
        state.sprite_vertices.push_back((SpriteVertex) {
            .position = { position_right, position_bottom },
            .tex_coord = { tex_coord_right, tex_coord_bottom, font_atlas }
        });
        state.sprite_vertices.push_back((SpriteVertex) {
            .position = { position_left, position_bottom },
            .tex_coord = { tex_coord_left, tex_coord_bottom, font_atlas }
        });
        state.sprite_vertices.push_back((SpriteVertex) {
            .position = { position_left, position_top },
            .tex_coord = { tex_coord_left, tex_coord_top, font_atlas }
        });
        state.sprite_vertices.push_back((SpriteVertex) {
            .position = { position_right, position_top },
            .tex_coord = { tex_coord_right, tex_coord_top, font_atlas}
        });
        state.sprite_vertices.push_back((SpriteVertex) {
            .position = { position_right, position_bottom },
            .tex_coord = { tex_coord_right, tex_coord_bottom, font_atlas }
        });

        glyph_position.x += state.fonts[name].glyphs[glyph_index].advance * RENDER_SCALE_FLOAT;
        text_index++;
    }
}

ivec2 render_get_text_size(FontName name, const char* text) {
    ivec2 size = ivec2(0, state.fonts[name].glyph_height);
    size_t text_index = 0;

    while (text[text_index] != '\0') {
        uint32_t glyph_index = (uint32_t)text[text_index] - FONT_FIRST_CHAR;
        if (glyph_index < 0 || glyph_index >= FONT_GLYPH_COUNT) {
            glyph_index = (uint32_t)('|' - FONT_FIRST_CHAR);
        }

        size.x += state.fonts[name].glyphs[glyph_index].advance;
        text_index++;
    }

    return size * RENDER_SCALE_INT;
}

void render_minimap_putpixel(MinimapLayer layer, ivec2 position, MinimapPixel pixel) {
    if (layer == MINIMAP_LAYER_FOG) {
        position.x += MINIMAP_TEXTURE_WIDTH / 2;
    }
    int index = position.x + (position.y * MINIMAP_TEXTURE_WIDTH);
    if (index < 0 || index >= MINIMAP_TEXTURE_WIDTH * MINIMAP_TEXTURE_HEIGHT) {
        return;
    }
    state.minimap_texture_pixels[index] = state.minimap_pixel_values[pixel];
}

void render_minimap_draw_rect(MinimapLayer layer, Rect rect, MinimapPixel pixel) {
    for (int y = rect.y; y < rect.y + rect.h + 1; y++) {
        render_minimap_putpixel(layer, ivec2(rect.x, y), pixel);
        render_minimap_putpixel(layer, ivec2(rect.x + rect.w, y), pixel);
    }
    for (int x = rect.x + 1; x < rect.x + rect.w; x++) {
        render_minimap_putpixel(layer, ivec2(x, rect.y), pixel);
        render_minimap_putpixel(layer, ivec2(x, rect.y + rect.h), pixel);
    }
}

void render_minimap_fill_rect(MinimapLayer layer, Rect rect, MinimapPixel pixel) {
    for (int y = rect.y; y < rect.y + rect.h + 1; y++) {
        for (int x = rect.x; x < rect.x + rect.w + 1; x++) {
            render_minimap_putpixel(layer, ivec2(x, y), pixel);
        }
    }
}

void render_minimap(ivec2 position, ivec2 src_size, ivec2 dst_size) {
    render_flush_batch();

    float position_left = (float)position.x;
    float position_right = (float)(position.x + dst_size.x);
    float position_top = (float)(SCREEN_HEIGHT - position.y);
    float position_bottom = (float)(SCREEN_HEIGHT - (position.y + dst_size.y));

    float tex_coord_left = 0.0f;
    float tex_coord_right = (float)src_size.x / (float)MINIMAP_TEXTURE_WIDTH;
    float tex_coord_fog_left = 0.5f;
    float tex_coord_fog_right = tex_coord_fog_left + ((float)src_size.x / (float)MINIMAP_TEXTURE_WIDTH);
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

        // minimap fog
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
        }
    };

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, state.minimap_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, MINIMAP_TEXTURE_WIDTH, MINIMAP_TEXTURE_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, state.minimap_texture_pixels);

    glUseProgram(state.minimap_shader);
    glBindVertexArray(state.sprite_vao);
    glBindBuffer(GL_ARRAY_BUFFER, state.sprite_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(minimap_vertices), minimap_vertices);
    glDrawArrays(GL_TRIANGLES, 0, 12);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}