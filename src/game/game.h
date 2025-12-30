#pragma once

#include "defines.h"
#include "menu/menu.h"
#include "shell/shell.h"
#include "network/network.h"

#define FILENAME_MAX_LENGTH 255

enum GameMode {
    GAME_MODE_NONE,
    GAME_MODE_MENU,
    GAME_MODE_MATCH,
    GAME_MODE_REPLAY,
    #ifdef GOLD_DEBUG
        GAME_MODE_MAP_EDIT
    #endif
};

#ifdef GOLD_STEAM
    struct GameSetModeMenuParams {
        uint64_t steam_invite_id;
    };
#endif

struct GameSetModeMatchParams {
    int lcg_seed;
    Noise* noise;
};

struct GameSetModeReplayParams {
    char filename[FILENAME_MAX_LENGTH + 1];
};

struct GameSetModeParams {
    GameMode mode;
    union {
        #ifdef GOLD_STEAM
            GameSetModeMenuParams menu;
        #endif
        GameSetModeMatchParams match;
        GameSetModeReplayParams replay;
    };
};

#ifdef GOLD_DEBUG
    enum LaunchMode {
        LAUNCH_MODE_GAME,
        LAUNCH_MODE_TEST_HOST,
        LAUNCH_MODE_TEST_JOIN,
        LAUNCH_MODE_MAP_EDIT
    };
#endif

struct GameState {
    GameMode mode = GAME_MODE_NONE;
    MenuState* menu_state = nullptr;
    MatchShellState* match_shell_state = nullptr;

    #ifdef GOLD_DEBUG
        LaunchMode launch_mode;
        Bot test_bot;
    #endif
};

GameState game_init();
void game_quit(GameState& state);
bool game_is_running(const GameState& state);
void game_set_mode(GameState& state, GameSetModeParams params);

void game_handle_network_event(GameState& state, const NetworkEvent& event);
void game_update(GameState& state);
void game_render(const GameState& state);

#ifdef GOLD_DEBUG
    GameState game_debug_init(LaunchMode launch_mode);
    void game_test_set_mode(GameState& state, GameMode mode);
    void game_test_update(GameState& state);
#endif