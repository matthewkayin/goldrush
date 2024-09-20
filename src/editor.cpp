#include "editor.h"

#ifdef GOLD_DEBUG

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_sdl2.h>
#include <imgui/imgui_impl_sdlrenderer2.h>
#include <cstdint>
#include <string>
#include "defines.h"
#include "logger.h"

#define EDITOR_WINDOW_WIDTH 1280
#define EDITOR_WINDOW_HEIGHT 720

struct editor_t {
    SDL_Window* window;
    SDL_Renderer* renderer;

    bool is_running;
    bool show_demo_window;
};
static editor_t editor;

void imgui_style_init();

int editor_run() {
    log_info("Initializing GoldRush Map Editor...");

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

    log_info("Initialized editor.");

    editor.is_running = true;
    editor.show_demo_window = false;

    while (editor.is_running) {
        // INPUT
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            // Handle quit
            if (event.type == SDL_QUIT) {
                editor.is_running = false;
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_BACKQUOTE) {
                editor.show_demo_window = !editor.show_demo_window;
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

        ImGui::SetNextWindowPos(ImVec2(0, EDITOR_WINDOW_HEIGHT - 28));
        ImGui::SetNextWindowSize(ImVec2(EDITOR_WINDOW_WIDTH, 28));
        ImGui::Begin("Status Bar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav);
        ImGui::Text("Hello friends.");
        ImGui::End();

        if (editor.show_demo_window) {
            ImGui::ShowDemoWindow();
        }

        ImGui::Render();

        SDL_SetRenderDrawColor(editor.renderer, 0, 0, 0, 255);
        SDL_RenderClear(editor.renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), editor.renderer);
        SDL_RenderPresent(editor.renderer);
    } // End while running

    // DEINIT

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(editor.renderer);
    SDL_DestroyWindow(editor.window);
    IMG_Quit();
    SDL_Quit();

    return 0;
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

#else

int editor_run() {}

#endif