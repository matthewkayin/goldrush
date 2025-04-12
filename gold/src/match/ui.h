#pragma once

#include "defines.h"
#include "math/gmath.h"
#include "core/network.h"
#include "noise.h"
#include "match/state.h"
#include "match/input.h"
#include "render/render.h"
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
    RENDER_GARRISONBAR,
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
    MATCH_UI_MODE_TARGET_ATTACK,
    MATCH_UI_MODE_TARGET_UNLOAD,
    MATCH_UI_MODE_TARGET_REPAIR,
    MATCH_UI_MODE_TARGET_SMOKE,
    MATCH_UI_MODE_CHAT
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
    std::vector<std::vector<MatchInput>> inputs[MAX_PLAYERS];
    std::vector<MatchInput> input_queue;

    ivec2 camera_offset;
    bool is_minimap_dragging;

    ivec2 select_origin;
    uint32_t double_click_timer;
    int control_group_selected;
    uint32_t control_group_double_tap_timer;
    std::vector<EntityId> control_groups[MATCH_UI_CONTROL_GROUP_COUNT];
    std::vector<EntityId> selection;

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
    ivec2 move_animation_position;
    EntityId move_animation_entity_id;

    MatchState match;
};

MatchUiState match_ui_init(int32_t lcg_seed, Noise& noise);
void match_ui_handle_network_event(MatchUiState& state, NetworkEvent event);
void match_ui_handle_input(MatchUiState& state);
void match_ui_update(MatchUiState& state);

void match_ui_clamp_camera(MatchUiState& state);
void match_ui_center_camera_on_cell(MatchUiState& state, ivec2 cell);
bool match_ui_is_mouse_in_ui();
bool match_ui_is_targeting(const MatchUiState& state);
bool match_ui_is_selecting(const MatchUiState& state);
std::vector<EntityId> match_ui_create_selection(const MatchUiState& state, Rect rect);
void match_ui_set_selection(MatchUiState& state, std::vector<EntityId>& selection);
MatchUiSelectionType match_ui_get_selection_type(const MatchUiState& state, const std::vector<EntityId>& selection);
void match_ui_order_move(MatchUiState& state);
void match_ui_show_status(MatchUiState& state, const char* message);
ivec2 match_ui_get_building_cell(int building_size, ivec2 camera_offset);
bool match_ui_building_can_be_placed(const MatchUiState& state);
Rect match_ui_get_selection_list_item_rect(uint32_t selection_index);
void match_ui_add_chat_message(MatchUiState& state, uint8_t player_id, const char* message);

void match_ui_render(const MatchUiState& state);

SpriteName match_ui_get_entity_select_ring(EntityType type, bool attacking);
int match_ui_ysort_render_params_partition(std::vector<RenderSpriteParams>& params, int low, int high);
void match_ui_ysort_render_params(std::vector<RenderSpriteParams>& params, int low, int high);
void match_ui_render_healthbar(RenderHealthbarType type, ivec2 position, ivec2 size, int amount, int max);
void match_ui_render_target_build(const MatchUiState& state, const Target& target);
RenderSpriteParams match_ui_create_entity_render_params(const MatchUiState& state, const Entity& entity);