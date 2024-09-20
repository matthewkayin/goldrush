#include "editor.h"

#ifdef GOLD_DEBUG

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_sdl2.h>
#include <imgui/imgui_impl_sdlrenderer2.h>
#include <cstdint>
#include <string>
#include <algorithm>
#include "defines.h"
#include "logger.h"
#include "map.h"
#include "util.h"
#include "sprite.h"

#define EDITOR_WINDOW_WIDTH 1280
#define EDITOR_WINDOW_HEIGHT 720
#define MINIMAP_CANVAS_SIZE 196
#define MINIMAP_PADDING 8
#define EDITOR_STATUSBAR_HEIGHT 28
#define MENUBAR_SIZE 21
#define EDITOR_ZOOM 2
#define EDITOR_TILE_SIZE (TILE_SIZE * EDITOR_ZOOM)

const SDL_Color COLOR_WINDOWBG = (SDL_Color) { .r = 191, .g = 191, .b = 191, .a = 255 };
const SDL_Color COLOR_SAND_DARK = (SDL_Color) { .r = 204, .g = 162, .b = 139, .a = 255 };
const SDL_Color COLOR_GOLD = (SDL_Color) { .r = 238, .g = 209, .b = 158, .a = 255 };

const SDL_Rect MINIMAP_FRAME_RECT = (SDL_Rect) { .x = 0, . y = MENUBAR_SIZE, .w = MINIMAP_CANVAS_SIZE + (MINIMAP_PADDING * 2), .h = MINIMAP_CANVAS_SIZE + (MINIMAP_PADDING * 2) };
const SDL_Rect MINIMAP_RECT = (SDL_Rect) { .x = MINIMAP_FRAME_RECT.x + MINIMAP_PADDING, .y = MINIMAP_FRAME_RECT.y + MINIMAP_PADDING, .w = MINIMAP_CANVAS_SIZE, .h = MINIMAP_CANVAS_SIZE };
const SDL_Rect CANVAS_RECT = (SDL_Rect) { .x = MINIMAP_FRAME_RECT.x + MINIMAP_FRAME_RECT.w, .y = MENUBAR_SIZE, .w = EDITOR_WINDOW_WIDTH - MINIMAP_FRAME_RECT.w, .h = EDITOR_WINDOW_HEIGHT - MENUBAR_SIZE - EDITOR_STATUSBAR_HEIGHT };

struct sprite_t {
    union {
        SDL_Texture* texture;
        SDL_Texture* colored_texture[MAX_PLAYERS];
    };
    xy frame_size;
    int hframes;
    int vframes;
};

enum EditorMode {
    EDITOR_MODE_DEFAULT,
    EDITOR_MODE_NOT_RUNNING,
    EDITOR_MODE_PAN,
    EDITOR_MODE_MINIMAP_DRAG
};

struct editor_t {
    SDL_Window* window;
    SDL_Renderer* renderer;

    SDL_Texture* minimap_texture;
    sprite_t sprite_tileset;
    sprite_t sprite_decorations;
    sprite_t sprite_mine;

    EditorMode mode;
    bool show_demo_window;
    map_data_t map;
    xy camera_offset;
    xy mouse_pos;
};
static editor_t editor;

bool sdl_rect_has_point(const SDL_Rect& rect, const xy& point) {
    return !(point.x < rect.x || point.x >= rect.x + rect.w || point.y < rect.y || point.y >= rect.y + rect.h);
}

xy editor_camera_clamp(xy camera_offset) {
    return xy(std::clamp(camera_offset.x, 0, ((int)editor.map.width * EDITOR_TILE_SIZE) - CANVAS_RECT.w),
                 std::clamp(camera_offset.y, 0, ((int)editor.map.height * EDITOR_TILE_SIZE) - CANVAS_RECT.h));
}

xy editor_camera_centered_on_cell(xy cell) {
    return editor_camera_clamp(xy((cell.x * EDITOR_TILE_SIZE) + (EDITOR_TILE_SIZE / 2) - (CANVAS_RECT.w / 2), (cell.y * EDITOR_TILE_SIZE) + (EDITOR_TILE_SIZE / 2) - (CANVAS_RECT.h / 2)));
}


sprite_t load_sprite(Sprite sprite);
void imgui_style_init();
void map_new(uint32_t width, uint32_t height);

int editor_run() {
    log_info("Opening in edit mode.");

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        log_error("SDL failed to initialize: %s", SDL_GetError());
        return 1;
    }

    int img_flags = IMG_INIT_PNG;
    if (!(IMG_Init(img_flags) & img_flags)) {
        log_error("SDL_image failed to initialize: %s", IMG_GetError());
        return 1;
    }

    editor.window = SDL_CreateWindow("GoldRush Map Editor", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, EDITOR_WINDOW_WIDTH, EDITOR_WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (editor.window == NULL) {
        log_error("Error creating window: %s", SDL_GetError());
        return 1;
    }

    editor.renderer = SDL_CreateRenderer(editor.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (editor.renderer == NULL) {
        log_error("Error creating renderer: %s", SDL_GetError());
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsClassic();

    ImGui_ImplSDL2_InitForSDLRenderer(editor.window, editor.renderer);
    ImGui_ImplSDLRenderer2_Init(editor.renderer);

    imgui_style_init();

    std::string resource_base_path = std::string(RESOURCE_BASE_PATH);
    editor.sprite_tileset = load_sprite(SPRITE_TILES);
    editor.sprite_decorations = load_sprite(SPRITE_TILE_DECORATION);
    editor.sprite_mine = load_sprite(SPRITE_BUILDING_MINE);

    editor.minimap_texture = NULL;
    map_new(64, 64);

    log_info("Initialized GoldRush Map Editor.");

    editor.mode = EDITOR_MODE_DEFAULT;
    editor.show_demo_window = false;

    while (editor.mode != EDITOR_MODE_NOT_RUNNING) {
        // INPUT
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            // Handle quit
            if (event.type == SDL_QUIT) {
                editor.mode = EDITOR_MODE_NOT_RUNNING;
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_BACKQUOTE) {
                editor.show_demo_window = !editor.show_demo_window;
            } else if (event.type == SDL_MOUSEMOTION) {
                editor.mouse_pos = xy(event.motion.x, event.motion.y);
                if (editor.mode == EDITOR_MODE_PAN) {
                    editor.camera_offset = editor_camera_clamp(editor.camera_offset - xy(event.motion.xrel, event.motion.yrel));
                } else if (editor.mode == EDITOR_MODE_MINIMAP_DRAG) {
                    xy minimap_pos = editor.mouse_pos - xy(MINIMAP_RECT.x, MINIMAP_RECT.y);
                    minimap_pos.x = std::clamp(minimap_pos.x, 0, MINIMAP_RECT.w);
                    minimap_pos.y = std::clamp(minimap_pos.y, 0, MINIMAP_RECT.h);
                    xy map_pos = xy((editor.map.width * EDITOR_TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.w, (editor.map.height * EDITOR_TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.h);
                    editor.camera_offset = editor_camera_centered_on_cell(map_pos / EDITOR_TILE_SIZE); 
                }
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_RIGHT && editor.mode == EDITOR_MODE_DEFAULT && sdl_rect_has_point(CANVAS_RECT, editor.mouse_pos)) {
                    editor.mode = EDITOR_MODE_PAN;
                } else if (event.button.button == SDL_BUTTON_LEFT && editor.mode == EDITOR_MODE_DEFAULT && sdl_rect_has_point(MINIMAP_RECT, editor.mouse_pos)) {
                    editor.mode = EDITOR_MODE_MINIMAP_DRAG;
                }
            } else if (event.type == SDL_MOUSEBUTTONUP) {
                if ((event.button.button == SDL_BUTTON_RIGHT && editor.mode == EDITOR_MODE_PAN) ||
                    (event.button.button == SDL_BUTTON_LEFT && editor.mode == EDITOR_MODE_MINIMAP_DRAG)) {
                    editor.mode = EDITOR_MODE_DEFAULT;
                }
            }
        }

        if (SDL_GetWindowFlags(editor.window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            continue;
        }

        // UPDATE / RENDER

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::BeginMainMenuBar();
        if (ImGui::BeginMenu("File")) {
            ImGui::MenuItem("New");
            ImGui::MenuItem("Open");
            ImGui::MenuItem("Save");
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();

        ImGui::SetNextWindowPos(ImVec2(0, EDITOR_WINDOW_HEIGHT - EDITOR_STATUSBAR_HEIGHT), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(EDITOR_WINDOW_WIDTH, EDITOR_STATUSBAR_HEIGHT), ImGuiCond_FirstUseEver);
        ImGui::Begin("Status Bar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::Text("Hello friends.");
        ImGui::End();

        /*
        ImGui::SetNextWindowPos(ImVec2(0, menubar_size.y), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(MINIMAP_CANVAS_SIZE + 16.0f, EDITOR_WINDOW_HEIGHT - menubar_size.y - EDITOR_STATUSBAR_HEIGHT), ImGuiCond_FirstUseEver);
        ImGui::Begin("Sidebar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImVec2 sidebar_size = ImGui::GetWindowSize();
        SDL_SetRenderTarget(editor.renderer, editor.minimap_texture);
        SDL_SetRenderDrawColor(editor.renderer, 255, 0, 0, 255);
        SDL_RenderClear(editor.renderer);
        SDL_SetRenderTarget(editor.renderer, NULL);
        ImGui::SetCursorPos(ImVec2(8.0f, 8.0f));
        ImGui::Image(editor.minimap_texture, ImVec2(MINIMAP_CANVAS_SIZE, MINIMAP_CANVAS_SIZE));
        ImGui::End();
        */

        if (editor.show_demo_window) {
            ImGui::ShowDemoWindow();
        }

        ImGui::Render();

        SDL_SetRenderDrawColor(editor.renderer, 0, 0, 0, 255);
        SDL_RenderClear(editor.renderer);

        // Render map
        SDL_Rect tile_src_rect = (SDL_Rect) { .x = 0, .y = 0, .w = TILE_SIZE, .h = TILE_SIZE };
        SDL_Rect tile_dst_rect = (SDL_Rect) { .x = 0, .y = 0, .w = EDITOR_TILE_SIZE, .h = EDITOR_TILE_SIZE };
        xy base_pos = xy(-(editor.camera_offset.x % EDITOR_TILE_SIZE), -(editor.camera_offset.y % EDITOR_TILE_SIZE));
        xy base_coords = xy(editor.camera_offset.x / EDITOR_TILE_SIZE, editor.camera_offset.y / EDITOR_TILE_SIZE);
        xy max_visible_tiles = xy(CANVAS_RECT.w / EDITOR_TILE_SIZE, CANVAS_RECT.h / EDITOR_TILE_SIZE);
        if (base_pos.x != 0) {
            max_visible_tiles.x++;
        }
        if (CANVAS_RECT.w % EDITOR_TILE_SIZE != 0) {
            max_visible_tiles.x++;
        }
        if (base_pos.y != 0) {
            max_visible_tiles.y++;
        }
        if (CANVAS_RECT.h % EDITOR_TILE_SIZE != 0) {
            max_visible_tiles.y++;
        }

        for (int y = 0; y < max_visible_tiles.y; y++) {
            for (int x = 0; x < max_visible_tiles.x; x++) {
                int map_index = (base_coords.x + x) + ((base_coords.y + y) * editor.map.width);
                tile_src_rect.x = ((int)editor.map.tiles[map_index].base % editor.sprite_tileset.hframes) * TILE_SIZE;
                tile_src_rect.y = ((int)editor.map.tiles[map_index].base / editor.sprite_tileset.hframes) * TILE_SIZE;

                tile_dst_rect.x = CANVAS_RECT.x + base_pos.x + (x * EDITOR_TILE_SIZE);
                tile_dst_rect.y = CANVAS_RECT.y + base_pos.y + (y * EDITOR_TILE_SIZE);

                SDL_RenderCopy(editor.renderer, editor.sprite_tileset.texture, &tile_src_rect, &tile_dst_rect);
            }
        }

        // Render minimap
        SDL_SetRenderTarget(editor.renderer, editor.minimap_texture);

        SDL_SetRenderDrawColor(editor.renderer, COLOR_SAND_DARK.r, COLOR_SAND_DARK.g, COLOR_SAND_DARK.b, COLOR_SAND_DARK.a);
        SDL_RenderClear(editor.renderer);

        SDL_SetRenderDrawColor(editor.renderer, COLOR_GOLD.r, COLOR_GOLD.g, COLOR_GOLD.b, COLOR_GOLD.a);
        for (const mine_t& mine : editor.map.mines) {
            SDL_Rect mine_rect = (SDL_Rect) {
                .x = mine.cell.x,
                .y = mine.cell.y,
                .w = 4,
                .h = 4
            };
            SDL_RenderFillRect(editor.renderer, &mine_rect);
        }

        SDL_Rect camera_rect = (SDL_Rect) { 
            .x = editor.camera_offset.x / EDITOR_TILE_SIZE,
            .y = editor.camera_offset.y / EDITOR_TILE_SIZE,
            .w = 1 + (CANVAS_RECT.w / EDITOR_TILE_SIZE),
            .h = 1 + (CANVAS_RECT.h / EDITOR_TILE_SIZE) 
        };
        SDL_SetRenderDrawColor(editor.renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(editor.renderer, &camera_rect);

        SDL_SetRenderTarget(editor.renderer, NULL);

        // Render minimap frame and texture
        SDL_SetRenderDrawColor(editor.renderer, COLOR_WINDOWBG.r, COLOR_WINDOWBG.g, COLOR_WINDOWBG.b, COLOR_WINDOWBG.a);
        SDL_RenderFillRect(editor.renderer, &MINIMAP_FRAME_RECT);
        SDL_Rect minimap_src_rect = (SDL_Rect) { .x = 0, .y = 0, .w = (int)editor.map.width, .h = (int)editor.map.height };
        SDL_RenderCopy(editor.renderer, editor.minimap_texture, &minimap_src_rect, &MINIMAP_RECT);

        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), editor.renderer);
        SDL_RenderPresent(editor.renderer);
    } // End while running

    // DEINIT

    SDL_DestroyTexture(editor.minimap_texture);
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(editor.renderer);
    SDL_DestroyWindow(editor.window);
    IMG_Quit();
    SDL_Quit();

    return 0;
}

sprite_t load_sprite(Sprite sprite_name) {
    sprite_t sprite;

    sprite_params_t params = SPRITE_PARAMS.at(sprite_name);
    std::string path = std::string(RESOURCE_BASE_PATH) + std::string(params.path);
    SDL_Surface* surface = IMG_Load(path.c_str());
    if (surface == NULL) {
        log_error("Error loading image: %s", IMG_GetError());
    }

    sprite.texture = SDL_CreateTextureFromSurface(editor.renderer, surface);
    if (sprite.texture == NULL) {
        log_error("Error creating texture: %s", SDL_GetError());
    }

    if (params.hframes == -1) {
        sprite.frame_size = xy(TILE_SIZE, TILE_SIZE);
        sprite.hframes = surface->w / sprite.frame_size.x;
        sprite.vframes = surface->h / sprite.frame_size.y;
    } else {
        sprite.hframes = params.hframes;
        sprite.vframes = params.vframes;
        sprite.frame_size = xy(surface->w / sprite.hframes, surface->h / sprite.vframes);
    }

    SDL_FreeSurface(surface);
    return sprite;
}

void imgui_style_init() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameBorderSize = 1.0f;
    style.FramePadding = ImVec2(4.0f, 4.0f);
    style.WindowMenuButtonPosition = ImGuiDir_Right;
    style.ScrollbarSize = 16.0f;
    style.Colors[ImGuiCol_Text]                   = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled]           = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_WindowBg]               = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
    style.Colors[ImGuiCol_ChildBg]                = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
    style.Colors[ImGuiCol_PopupBg]                = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
    style.Colors[ImGuiCol_Border]                 = ImVec4(0.00f, 0.00f, 0.00f, 0.30f);
    style.Colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg]                = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered]         = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_FrameBgActive]          = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_TitleBg]                = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive]          = ImVec4(0.22f, 0.47f, 0.94f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);
    style.Colors[ImGuiCol_MenuBarBg]              = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.98f, 0.98f, 0.98f, 0.53f);
    style.Colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.69f, 0.69f, 0.69f, 0.80f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.49f, 0.49f, 0.49f, 0.80f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
    style.Colors[ImGuiCol_CheckMark]              = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab]             = ImVec4(0.26f, 0.59f, 0.98f, 0.78f);
    style.Colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.46f, 0.54f, 0.80f, 0.60f);
    style.Colors[ImGuiCol_Button]                 = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    style.Colors[ImGuiCol_ButtonHovered]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive]           = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_Header]                 = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
    style.Colors[ImGuiCol_HeaderHovered]          = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    style.Colors[ImGuiCol_HeaderActive]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_Separator]              = ImVec4(0.39f, 0.39f, 0.39f, 0.62f);
    style.Colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.14f, 0.44f, 0.80f, 0.78f);
    style.Colors[ImGuiCol_SeparatorActive]        = ImVec4(0.14f, 0.44f, 0.80f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip]             = ImVec4(0.80f, 0.80f, 0.80f, 0.56f);
    style.Colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    style.Colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    style.Colors[ImGuiCol_Tab]                    = ImVec4(0.76f, 0.80f, 0.84f, 0.95f);
    style.Colors[ImGuiCol_TabHovered]             = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    style.Colors[ImGuiCol_TabActive]              = ImVec4(0.60f, 0.73f, 0.88f, 0.95f);
    style.Colors[ImGuiCol_TabUnfocused]           = ImVec4(0.92f, 0.92f, 0.94f, 0.95f);
    style.Colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.74f, 0.82f, 0.91f, 1.00f);
    style.Colors[ImGuiCol_PlotLines]              = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.45f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    style.Colors[ImGuiCol_DragDropTarget]         = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    style.Colors[ImGuiCol_NavHighlight]           = style.Colors[ImGuiCol_HeaderHovered];
    style.Colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(0.70f, 0.70f, 0.70f, 0.70f);
    style.Colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.20f, 0.20f, 0.20f, 0.20f);
    style.Colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
}

void map_new(uint32_t width, uint32_t height) {
    map_data_t map;
    map.width = width;
    map.height = height;
    map.tiles = std::vector<tile_t>(width * height, (tile_t) {
        .base = 0,
        .decoration = 0
    });

    editor.map = map;
    editor.camera_offset = xy(0, 0);
    if (editor.minimap_texture != NULL) {
        SDL_DestroyTexture(editor.minimap_texture);
    }
    editor.minimap_texture = SDL_CreateTexture(editor.renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, editor.map.width, editor.map.height);
}

#else

int editor_run() {}

#endif