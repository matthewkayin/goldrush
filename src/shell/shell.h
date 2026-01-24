#pragma once

#include "defines.h"
#include "shell/chat.h"
#include "shell/replay.h"
#include "match/state.h"
#include "core/ui.h"
#include "menu/options_menu.h"
#include "render/render.h"
#include "network/types.h"
#include "bot/bot.h"
#include "render/ysort.h"
#include "shell/hotkey.h"
#include "scenario/scenario.h"
#include <luajit/lua.hpp>

#define MATCH_SHELL_CONTROL_GROUP_COUNT 10
#define MATCH_SHELL_CONTROL_GROUP_NONE -1
#define MATCH_SHELL_CAMERA_HOTKEY_COUNT 6

// Timing
const uint32_t TURN_OFFSET = 4;
const uint32_t TURN_DURATION = 4;

enum RenderHealthbarType {
    RENDER_HEALTHBAR,
    RENDER_GARRISON_BAR,
    RENDER_ENERGY_BAR
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
    MATCH_SHELL_MODE_EXIT_PROGRAM,
    MATCH_SHELL_MODE_DESYNC
};

enum CameraMode {
    CAMERA_MODE_FREE,
    CAMERA_MODE_MINIMAP_DRAG,
    CAMERA_MODE_PAN,
    CAMERA_MODE_PAN_HOLD,
    CAMERA_MODE_PAN_RETURN
};

struct Alert {
    MinimapPixel pixel;
    ivec2 cell;
    int cell_size;
    uint32_t timer;
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
    uint32_t match_over_timer;
    bool is_paused;

    // Inputs
    std::queue<std::vector<MatchInput>> inputs[MAX_PLAYERS];
    std::vector<MatchInput> input_queue;

    // Bots
    Bot bots[MAX_PLAYERS];

    // Camera
    CameraMode camera_mode;
    ivec2 camera_offset;
    ivec2 camera_pan_return_offset;
    ivec2 camera_pan_offset;
    uint32_t camera_pan_timer;
    uint32_t camera_pan_duration;
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
    InputAction hotkey_group[HOTKEY_GROUP_SIZE];
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
    Animation highlight_animation;
    EntityId highlight_entity_id;

    // Gold amounts
    uint32_t displayed_gold_amounts[MAX_PLAYERS];

    // Scenario 
    uint32_t scenario_allowed_upgrades;
    bool scenario_allowed_entities[ENTITY_TYPE_COUNT];
    bool scenario_show_enemy_gold;
    bool scenario_lose_on_buildings_destroyed;
    lua_State* scenario_lua_state;

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

    // Checksum
    std::vector<uint32_t> checksums[MAX_PLAYERS];

    // Debug
    #ifdef GOLD_DEBUG
        DebugFog debug_fog;
        bool debug_show_region_lines;
    #endif
};

// Init
MatchShellState* match_shell_init(int lcg_seed, Noise* noise);
MatchShellState* match_shell_init_from_scenario(const Scenario* scenario, const char* script_path);
MatchShellState* replay_shell_init(const char* replay_path);

// Network event
void match_shell_handle_network_event(MatchShellState* state, NetworkEvent event);

// Update
void match_shell_update(MatchShellState* state);
void match_shell_handle_input(MatchShellState* state);
void match_shell_order_move(MatchShellState* state);
std::vector<EntityId> match_shell_find_idle_miners(const MatchShellState* state);
bool match_shell_does_player_meet_hotkey_requirements(const MatchState& state, InputAction hotkey);
bool match_shell_is_hotkey_available(const MatchShellState* state, const HotkeyButtonInfo& info);

// Triggers
uint32_t match_shell_get_player_entity_count(const MatchShellState* state, uint8_t player_id, EntityType entity_type);

// State queries
bool match_shell_is_mouse_in_ui();
bool match_shell_is_selecting(const MatchShellState* state);
bool match_shell_is_in_menu(const MatchShellState* state);
bool match_shell_is_in_hotkey_submenu(const MatchShellState* state);
bool match_shell_is_targeting(const MatchShellState* state);

// Camera
void match_shell_clamp_camera(MatchShellState* state);
void match_shell_center_camera_on_cell(MatchShellState* state, ivec2 cell);
bool match_shell_is_camera_free(const MatchShellState* state);
bool match_shell_is_camera_panning(const MatchShellState* state);
void match_shell_end_camera_pan(MatchShellState* state);
void match_shell_begin_camera_pan(MatchShellState* state, ivec2 to, uint32_t pan_duration);
void match_shell_begin_camera_return(MatchShellState* state);

// Selection
std::vector<EntityId> match_shell_create_selection(const MatchShellState* state, Rect select_rect);
void match_shell_set_selection(MatchShellState* state, std::vector<EntityId>& selection);
MatchShellSelectionType match_shell_get_selection_type(const MatchShellState* state, const std::vector<EntityId>& selection);
bool match_shell_selection_has_enough_energy(const MatchShellState* state, const std::vector<EntityId>& selection, uint32_t cost);
bool match_shell_is_entity_player_controlled_and_in_goldmine(const MatchShellState* state, const Entity& entity);
bool match_shell_is_entity_selectable(const MatchShellState* state, const Entity& entity);
bool match_shell_can_keep_selecting_entity(const MatchShellState* state, const Entity& entity);

// Vision
bool match_shell_is_entity_visible(const MatchShellState* state, const Entity& entity);
bool match_shell_is_cell_rect_revealed(const MatchShellState* state, ivec2 cell, int cell_size);
int match_shell_get_fog(const MatchShellState* state, ivec2 cell);

// Chat
void match_shell_get_player_prefix(const MatchShellState* state, uint8_t player_id, char* prefix);
FontName match_shell_get_player_font(uint8_t player_id);
void match_shell_add_chat_message(MatchShellState* state, FontName prefix_font, const char* prefix, const char* message, uint32_t duration);
void match_shell_handle_player_disconnect(MatchShellState* state, uint8_t player_id);

// Status
void match_shell_show_status(MatchShellState* state, const char* message);

// Building placement
ivec2 match_shell_get_building_cell(int building_size, ivec2 camera_offset);
bool match_shell_building_can_be_placed(const MatchShellState* state);
bool match_shell_is_building_place_cell_valid(const MatchShellState* state, ivec2 miner_cell, ivec2 cell);

// Hotkey Menu
Rect match_shell_get_selection_list_item_rect(uint32_t selection_index);
Rect match_shell_get_idle_miner_button_rect();
bool match_shell_has_pressed_idle_miner_button();

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
bool match_shell_is_in_single_player_game();
bool match_shell_is_surrender_required_to_leave(const MatchShellState* state);
void match_shell_leave_match(MatchShellState* state, bool exit_program);

// Desync
bool match_shell_are_checksums_out_of_sync(MatchShellState* state, uint32_t frame);
void match_shell_compare_checksums(MatchShellState* state, uint32_t frame);
#ifdef GOLD_DEBUG_DESYNC
    void match_shell_handle_serialized_frame(uint8_t* incoming_state_buffer, size_t incoming_state_buffer_length);
#endif

// Render
void match_shell_render(const MatchShellState* state);
bool match_shell_use_yellow_rings(const MatchShellState* state);
SpriteName match_shell_get_entity_select_ring(EntityType type, bool attacking);
SpriteName match_shell_hotkey_get_sprite(const MatchShellState* state, InputAction hotkey, bool show_toggled);
void match_shell_render_healthbar(RenderHealthbarType type, ivec2 position, ivec2 size, int amount, int max);
void match_shell_render_target_build(const MatchShellState* state, const Target& target, uint8_t player_id);
RenderSpriteParams match_shell_create_entity_render_params(const MatchShellState* state, const Entity& entity);
void match_shell_render_entity_select_rings_and_healthbars(const MatchShellState* state, const Entity& entity);
void match_shell_render_entity_icon(const MatchShellState* state, const Entity& entity, Rect icon_rect);
void match_shell_render_entity_move_animation(const MatchShellState* state, const Entity& entity, Animation move_animation);
void match_shell_render_particle(const MatchShellState* state, const Particle& particle);
bool match_shell_should_render_hotkey_toggled(const MatchShellState* state, InputAction hotkey);
const char* match_shell_render_get_stat_tooltip(SpriteName sprite);
void match_shell_render_tooltip(const MatchShellState* state, InputAction hotkey);
void match_shell_should_render_player_resources(const MatchShellState* state, uint8_t player_id, bool* should_render_gold, bool* should_render_population, bool* should_render_name);
ivec2 match_shell_get_queued_target_position(const MatchShellState* state, const Target& target);
FireCellRender match_shell_get_fire_cell_render(const MatchShellState* state, const Fire& fire);
MinimapPixel match_shell_get_minimap_pixel_for_cell(const MatchShellState* state, ivec2 cell);
MinimapPixel match_shell_get_minimap_pixel_for_entity(const MatchShellState* state, const Entity& entity);