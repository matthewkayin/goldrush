#pragma once

#include "defines.h"
#include "math/gmath.h"
#include "core/network.h"
#include "noise.h"
#include "match_state.h"

#define MATCH_UI_HEIGHT 88

enum MatchUiMode {
    MATCH_UI_MODE_NOT_STARTED,
    MATCH_UI_MODE_NONE
};

struct MatchUiState {
    MatchUiMode mode;
    ivec2 camera_offset;

    MatchState match;
};

MatchUiState match_ui_init(int32_t lcg_seed, Noise& noise);
void match_ui_handle_network_event(MatchUiState& state, NetworkEvent event);
void match_ui_update(MatchUiState& state);
void match_ui_render(const MatchUiState& state);