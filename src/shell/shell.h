#pragma once

#include "defines.h"
#include "shell/replay.h"
#include "match/state.h"
#include "core/ui.h"
#include "menu/options_menu.h"
#include "render/render.h"
#include "network/types.h"

#define MATCH_SHELL_UI_HEIGHT 176
#define MATCH_SHELL_CONTROL_GROUP_COUNT 10
#define MATCH_SHELL_CONTROL_GROUP_NONE -1
#define MATCH_SHELL_CAMERA_HOTKEY_COUNT 6

struct RenderSpriteParams {
    SpriteName sprite;
    ivec2 frame;
    ivec2 position;
    uint32_t options;
    int recolor_id;
};

enum MatchShellSelectionType {
    MATCH_SHELL_SELECTION_NONE,
    MATCH_SHELL_SELECTION_UNITS,
    MATCH_SHELL_SELECTION_ENEMY_UNIT,
    MATCH_SHELL_SELECTION_BUILDINGS,
    MATCH_SHELL_SELECTION_ENEMY_BUILDING,
    MATCH_SHELL_SELECTION_GOLD
};

enum MatchShellMode {
    MATCH_SHELL_MODE_NOT_STARTED,
    MATCH_SHELL_MODE_NONE,
    MATCH_SHELL_MODE_BUILD,
    MATCH_SHELL_MODE_BUILD2,
    MATCH_SHELL_MODE_BUILDING_PLACE,
    MATCH_SHELL_MODE_TARGET_ATTACK,
    MATCH_SHELL_MODE_TARGET_UNLOAD,
    MATCH_SHELL_MODE_TARGET_REPAIR,
    MATCH_SHELL_MODE_TARGET_MOLOTOV,
    MATCH_SHELL_MODE_CHAT,
    MATCH_SHELL_MODE_MENU,
    MATCH_SHELL_MODE_MENU_SURRENDER,
    MATCH_SHELL_MODE_MENU_SURRENDER_TO_DESKTOP,
    MATCH_SHELL_MODE_MATCH_OVER_VICTORY,
    MATCH_SHELL_MODE_MATCH_OVER_DEFEAT,
    MATCH_SHELL_MODE_LEAVE_MATCH,
    MATCH_SHELL_MODE_EXIT_PROGRAM
};

struct Alert {
    MinimapPixel pixel;
    ivec2 cell;
    int cell_size;
    uint32_t timer;
};

struct ChatMessage {
    std::string message;
    uint32_t timer;
    uint8_t player_id;
};

enum FireCellRender {
    FIRE_CELL_DO_NOT_RENDER,
    FIRE_CELL_RENDER_BELOW,
    FIRE_CELL_RENDER_ABOVE
};

#ifdef GOLD_DEBUG
    enum DebugFog {
        DEBUG_FOG_ENABLED,
        DEBUG_FOG_BOT_VISION,
        DEBUG_FOG_DISABLED
    };
#endif

struct MatchShellState {
    MatchShellMode mode;
    MatchState match_state;
    UI ui;
    OptionsMenuState options_menu;

    // Simulation timers
    uint32_t match_timer;
    uint32_t disconnect_timer;
    bool is_paused;

    // Inputs
    std::queue<std::vector<MatchInput>> inputs[MAX_PLAYERS];
    std::vector<MatchInput> input_queue;

    // Camera
    ivec2 camera_offset;
    bool is_minimap_dragging;
    ivec2 camera_hotkeys[MATCH_SHELL_CAMERA_HOTKEY_COUNT];

    // Selection
    ivec2 select_origin;
    uint32_t double_click_timer;
    int control_group_selected;
    uint32_t control_group_double_tap_timer;
    std::vector<EntityId> control_groups[MATCH_SHELL_CONTROL_GROUP_COUNT];
    std::vector<EntityId> selection;

    // Status
    std::string status_message;
    uint32_t status_timer;

    // For building placement
    EntityType building_type;

    // Chat
    std::string chat_message;
    std::vector<ChatMessage> chat;
    bool chat_cursor_visible;
    uint32_t chat_cursor_blink_timer;

    // Alerts
    std::vector<Alert> alerts;
    ivec2 latest_alert_cell;

    // Sound
    uint32_t sound_cooldown_timers[SOUND_COUNT];

    // Animations
    Animation rally_flag_animation;
    Animation move_animation;
    Animation building_fire_animation;
    ivec2 move_animation_position;
    EntityId move_animation_entity_id;

    // Gold amounts
    uint32_t displayed_gold_amounts[MAX_PLAYERS];

    // Replay file (write)
    FILE* replay_file;

    // Replay data (read)
    bool replay_mode;
    UI replay_ui;
    std::vector<MatchState> replay_checkpoints;
    std::vector<std::vector<MatchInput>> replay_inputs[MAX_PLAYERS];
    std::vector<ReplayChatMessage> replay_chatlog;

    // Replay fog
    uint32_t replay_fog_index;
    std::vector<std::string> replay_fog_texts;
    std::vector<uint8_t> replay_fog_player_ids;

    // Replay loading thread
    SDL_Thread* replay_loading_thread;
    MatchState replay_loading_match_state;
    uint32_t replay_loading_match_timer;

    SDL_Mutex* replay_loading_mutex;
    uint32_t replay_loaded_match_timer;

    SDL_Mutex* replay_loading_early_exit_mutex;
    bool replay_loading_early_exit;

    // Debug
    #ifdef GOLD_DEBUG
        DebugFog debug_fog;
    #endif
};

// Init
MatchShellState* match_shell_init(int lcg_seed, Noise& noise);
MatchShellState* replay_shell_init(const char* replay_path);

// Network event
void match_shell_handle_network_event(MatchShellState* state, NetworkEvent event);

// Update
void match_shell_update(MatchShellState* state);

// State queries
bool match_shell_is_mouse_in_ui();
bool match_shell_is_selecting(const MatchShellState* state);
bool match_shell_is_in_menu(const MatchShellState* state);
bool match_shell_is_in_hotkey_submenu(const MatchShellState* state);
bool match_shell_is_targeting(const MatchShellState* state);

// Camera
void match_shell_clamp_camera(MatchShellState* state);
void match_shell_center_camera_on_cell(MatchShellState* state, ivec2 cell);

// Selection
std::vector<EntityId> match_shell_create_selection(const MatchShellState* state, Rect select_rect);
void match_shell_set_selection(MatchShellState* state, std::vector<EntityId>& selection);
MatchShellSelectionType match_shell_get_selection_type(const MatchShellState* state, const std::vector<EntityId>& selection);

// Vision
bool match_shell_is_entity_visible(const MatchShellState* state, const Entity& entity);
bool match_shell_is_cell_rect_revealed(const MatchShellState* state, ivec2 cell, int cell_size);
int match_shell_get_fog(const MatchShellState* state, ivec2 cell);

// Chat
void match_shell_add_chat_message(MatchShellState* state, uint8_t player_id, const char* message);
void match_shell_handle_player_disconnect(MatchShellState* state, uint8_t player_id);

// Menu
const char* match_shell_get_menu_header_text(const MatchShellState* state);

// Fire
bool match_shell_is_fire_on_screen(const MatchShellState* state);

// Replay
void match_shell_replay_begin_turn(MatchShellState* state, MatchState& match_state, uint32_t match_timer);
void match_shell_replay_scrub(MatchShellState* state, uint32_t position);
size_t match_shell_replay_end_of_tape(const MatchShellState* state);

// Leave match
bool match_shell_is_at_least_one_opponent_in_match(const MatchShellState* state);
bool match_shell_is_surrender_required_to_leave(const MatchShellState* state);
void match_shell_leave_match(MatchShellState* state, bool exit_program);

// Render
void match_shell_render(const MatchShellState* state);
RenderSpriteParams match_shell_create_entity_render_params(const MatchShellState* state, const Entity& entity);