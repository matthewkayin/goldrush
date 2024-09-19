#define SDL_MAIN_HANDLED

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <cstdio>
#include <cstdint>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

struct editor_t {
    SDL_Window* window;
    SDL_Renderer* renderer;
    bool is_running;
};
static editor_t editor;

int main() {
    // INIT

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL failed to initialize: %s\n", SDL_GetError());
        return 1;
    }

#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    if (TTF_Init() == -1) {
        printf("SDL_ttf failed to initialize: %s\n", TTF_GetError());
        return 1;
    }

    int img_flags = IMG_INIT_PNG;
    if (!(IMG_Init(img_flags) & img_flags)) {
        printf("SDL_image failed to initialize: %s\n", IMG_GetError());
        return 1;
    }

    editor.window = SDL_CreateWindow("GoldRush Map Editor", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (editor.window == NULL) {
        printf("Error creating window: %s\n", SDL_GetError());
        return 1;
    }

    editor.renderer = SDL_CreateRenderer(editor.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (editor.renderer == NULL) {
        printf("Error creating renderer: %s\n", SDL_GetError());
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsClassic();

    ImGui_ImplSDL2_InitForSDLRenderer(editor.window, editor.renderer);
    ImGui_ImplSDLRenderer2_Init(editor.renderer);

    printf("Initialized editor.\n");

    // GAME LOOP
    editor.is_running = true;

    while (editor.is_running) {
        // INPUT
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            // Handle quit
            if (event.type == SDL_QUIT) {
                editor.is_running = false;
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
            if (ImGui::MenuItem("New")) { }
            if (ImGui::MenuItem("Open")) { }
            if (ImGui::MenuItem("Save")) { }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();

        ImGui::Render();

        SDL_SetRenderDrawColor(editor.renderer, 0, 0, 0, 255);
        SDL_RenderClear(editor.renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(editor.renderer);
    } // End while running

    // DEINIT

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(editor.renderer);
    SDL_DestroyWindow(editor.window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();

    return 0;
}