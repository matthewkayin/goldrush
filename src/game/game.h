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
    GAME_MODE_REPLAY
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
    enum TestMode {
        TEST_MODE_NONE,
        TEST_MODE_HOST,
        TEST_MODE_JOIN
    };
#endif

struct GameState {
    GameMode mode = GAME_MODE_NONE;
    MenuState* menu_state = nullptr;
    MatchShellState* match_shell_state = nullptr;

    #ifdef GOLD_DEBUG
        TestMode test_mode;
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
    GameState game_test_init(TestMode test_mode);
    void game_test_set_mode(GameState& state, GameMode mode);
    void game_test_update(GameState& state);
#endif