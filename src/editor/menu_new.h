#pragma once

#include "defines.h"
#include "core/ui.h"
#include "editor/document.h"

enum EditorMenuNewMode {
    EDITOR_MENU_NEW_CLOSED,
    EDITOR_MENU_NEW_OPEN,
    EDITOR_MENU_NEW_CREATE
};

struct EditorMenuNew {
    EditorMenuNewMode mode;
    uint32_t map_type;
    uint32_t map_size;
    uint32_t use_noise_gen_params;
    uint32_t noise_gen_inverted;
    uint32_t noise_gen_water_threshold;
    uint32_t noise_gen_lowground_threshold;
    uint32_t noise_gen_forest_threshold;
};

EditorMenuNew editor_menu_new_open();
void editor_menu_new_update(EditorMenuNew& menu, UI& ui);
NoiseGenParams editor_menu_new_create_noise_gen_params(const EditorMenuNew& menu);