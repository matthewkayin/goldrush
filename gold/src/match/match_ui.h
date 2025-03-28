#pragma once

#include "defines.h"
#include "math/gmath.h"
#include "core/network.h"
#include "noise.h"
#include "match_state.h"
#include "match_input.h"

#define MATCH_UI_HEIGHT 88

enum RenderLayer {
    RENDER_LAYER_TILE,
    RENDER_LAYER_MINE_SELECT_RING,
    RENDER_LAYER_MINE,
    RENDER_LAYER_SELECT_RING,
    RENDER_LAYER_ENTITY,
    RENDER_LAYER_COUNT
};
const uint16_t ELEVATION_COUNT = 3;
const uint32_t RENDER_TOTAL_LAYER_COUNT = (RENDER_LAYER_COUNT * ELEVATION_COUNT);

struct RenderSpriteParams {
    SpriteName sprite;
    ivec2 frame;
    ivec2 position;
    uint32_t options;
    int recolor_id;
};

enum MatchUiMode {
    MATCH_UI_MODE_NOT_STARTED,
    MATCH_UI_MODE_NONE
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
    std::vector<EntityId> selection;

    MatchState match;
};

MatchUiState match_ui_init(int32_t lcg_seed, Noise& noise);
void match_ui_handle_network_event(MatchUiState& state, NetworkEvent event);
void match_ui_update(MatchUiState& state);

void match_ui_clamp_camera(MatchUiState& state);
void match_ui_center_camera_on_cell(MatchUiState& state, ivec2 cell);
bool match_ui_is_mouse_in_ui();
bool match_ui_is_selecting(const MatchUiState& state);
std::vector<EntityId> match_ui_create_selection(const MatchUiState& state, Rect rect);
void match_ui_set_selection(MatchUiState& state, std::vector<EntityId>& selection);

void match_ui_render(const MatchUiState& state);

uint16_t match_ui_get_render_layer(uint16_t elevation, RenderLayer layer);
int match_ui_ysort_render_params_partition(std::vector<RenderSpriteParams>& params, int low, int high);
void match_ui_ysort_render_params(std::vector<RenderSpriteParams>& params, int low, int high);
