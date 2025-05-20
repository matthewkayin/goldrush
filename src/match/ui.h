#pragma once

#include "defines.h"
#include "math/gmath.h"
#include "core/network.h"
#include "noise.h"
#include "match/state.h"
#include "match/input.h"
#include "render/render.h"
#include "menu/options.h"
#include <string>

#define MATCH_UI_HEIGHT 88
#define MATCH_UI_CONTROL_GROUP_COUNT 10
#define MATCH_UI_CONTROL_GROUP_NONE -1

const uint16_t ELEVATION_COUNT = 3;

struct RenderSpriteParams {
    SpriteName sprite;
    ivec2 frame;
    ivec2 position;
    uint32_t options;
    int recolor_id;
};

enum RenderHealthbarType {
    RENDER_HEALTHBAR,
    RENDER_GARRISON_BAR,
    RENDER_ENERGY_BAR
};

enum MatchUiSelectionType {
    MATCH_UI_SELECTION_NONE,
    MATCH_UI_SELECTION_UNITS,
    MATCH_UI_SELECTION_ENEMY_UNIT,
    MATCH_UI_SELECTION_BUILDINGS,
    MATCH_UI_SELECTION_ENEMY_BUILDING,
    MATCH_UI_SELECTION_GOLD
};

enum MatchUiMode {
    MATCH_UI_MODE_NOT_STARTED,
    MATCH_UI_MODE_NONE,
    MATCH_UI_MODE_BUILD,
    MATCH_UI_MODE_BUILD2,
    MATCH_UI_MODE_BUILDING_PLACE,
    MATCH_UI_MODE_UPGRADE_ARMOR,
    MATCH_UI_MODE_UPGRADE_GUNS,
    MATCH_UI_MODE_TARGET_ATTACK,
    MATCH_UI_MODE_TARGET_UNLOAD,
    MATCH_UI_MODE_TARGET_REPAIR,
    MATCH_UI_MODE_TARGET_MOLOTOV,
    MATCH_UI_MODE_CHAT,
    MATCH_UI_MODE_MENU,
    MATCH_UI_MODE_MENU_SURRENDER,
    MATCH_UI_MODE_MATCH_OVER_VICTORY,
    MATCH_UI_MODE_MATCH_OVER_DEFEAT,
    MATCH_UI_MODE_LEAVE_MATCH
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

struct MatchUiState {
    MatchUiMode mode;
    uint32_t turn_timer;
    uint32_t turn_counter;
    uint32_t disconnect_timer;
    std::queue<std::vector<MatchInput>> inputs[MAX_PLAYERS];
    std::vector<MatchInput> input_queue;

    ivec2 camera_offset;
    bool is_minimap_dragging;

    ivec2 select_origin;
    uint32_t double_click_timer;
    int control_group_selected;
    uint32_t control_group_double_tap_timer;
    std::vector<EntityId> control_groups[MATCH_UI_CONTROL_GROUP_COUNT];
    std::vector<EntityId> selection;
    InputAction hotkey_group[HOTKEY_GROUP_SIZE];

    std::string status_message;
    uint32_t status_timer;

    EntityType building_type;

    std::string chat_message;
    std::vector<ChatMessage> chat;
    std::vector<Alert> alerts;
    uint32_t sound_cooldown_timers[SOUND_COUNT];
    bool chat_cursor_visible;
    uint32_t chat_cursor_blink_timer;

    Animation rally_flag_animation;
    Animation move_animation;
    Animation building_fire_animation;
    ivec2 move_animation_position;
    EntityId move_animation_entity_id;
    
    uint32_t displayed_gold_amounts[MAX_PLAYERS];

    MatchState match;
    OptionsMenuState options_menu;

    bool replay_mode;
    FILE* replay_file;
    uint32_t replay_fog_index;
    std::vector<std::string> replay_fog_texts;
    std::vector<uint8_t> replay_fog_player_ids;
    bool replay_paused;
    std::vector<MatchState> replay_checkpoints;
    std::vector<std::vector<MatchInput>> replay_inputs[MAX_PLAYERS];
};

MatchUiState match_ui_init(int32_t lcg_seed, Noise& noise);
MatchUiState match_ui_init_from_replay(const char* replay_path);
void match_ui_handle_network_event(MatchUiState& state, NetworkEvent event);
void match_ui_handle_input(MatchUiState& state);
void match_ui_update(MatchUiState& state);

void match_ui_clamp_camera(MatchUiState& state);
void match_ui_center_camera_on_cell(MatchUiState& state, ivec2 cell);
bool match_ui_is_mouse_in_ui();
bool match_ui_is_targeting(const MatchUiState& state);
bool match_ui_is_in_submenu(const MatchUiState& state);
bool match_ui_is_selecting(const MatchUiState& state);
std::vector<EntityId> match_ui_create_selection(const MatchUiState& state, Rect rect);
void match_ui_set_selection(MatchUiState& state, std::vector<EntityId>& selection);
MatchUiSelectionType match_ui_get_selection_type(const MatchUiState& state, const std::vector<EntityId>& selection);
bool match_ui_selection_has_enough_energy(const MatchUiState& state, const std::vector<EntityId>& selection, uint32_t cost);
void match_ui_order_move(MatchUiState& state);
void match_ui_show_status(MatchUiState& state, const char* message);
ivec2 match_ui_get_building_cell(int building_size, ivec2 camera_offset);
bool match_ui_building_can_be_placed(const MatchUiState& state);
Rect match_ui_get_selection_list_item_rect(uint32_t selection_index);
void match_ui_add_chat_message(MatchUiState& state, uint8_t player_id, const char* message);
bool match_ui_is_opponent_in_match(const MatchUiState& state);
bool match_ui_is_in_menu(MatchUiMode mode);
const char* match_ui_get_menu_header_text(MatchUiMode mode);
void match_ui_leave_match(MatchUiState& state);
bool match_ui_is_entity_visible(const MatchUiState& state, const Entity& entity);
bool match_ui_is_cell_rect_revealed(const MatchUiState& state, ivec2 cell, int cell_size);
int match_ui_get_fog(const MatchUiState& state, ivec2 cell);

void match_ui_replay_begin_turn(MatchUiState& state);
void match_ui_replay_scrub(MatchUiState& state, uint32_t position);
size_t match_ui_replay_end_of_tape(const MatchUiState& state);

void match_ui_render(const MatchUiState& state, bool render_debug_info);

SpriteName match_ui_get_entity_select_ring(EntityType type, bool attacking);
int match_ui_ysort_render_params_partition(std::vector<RenderSpriteParams>& params, int low, int high);
void match_ui_ysort_render_params(std::vector<RenderSpriteParams>& params, int low, int high);
void match_ui_render_healthbar(RenderHealthbarType type, ivec2 position, ivec2 size, int amount, int max);
void match_ui_render_target_build(const MatchUiState& state, const Target& target);
RenderSpriteParams match_ui_create_entity_render_params(const MatchUiState& state, const Entity& entity);
void match_ui_render_entity_select_rings_and_healthbars(const MatchUiState& state, const Entity& entity);
void match_ui_render_entity_icon(const MatchUiState& state, const Entity& entity, Rect icon_rect);
void match_ui_render_entity_move_animation(const MatchUiState& state, const Entity& entity);
void match_ui_render_particle(const MatchUiState& state, const Particle& particle);
bool match_ui_should_render_hotkey_toggled(const MatchUiState& state, InputAction hotkey);
const char* match_ui_render_get_stat_tooltip(SpriteName sprite);