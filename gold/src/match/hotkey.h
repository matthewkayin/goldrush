#pragma once

#include "render/sprite.h"
#include "core/input.h"
#include "entity.h"

enum HotkeyButtonType {
    HOTKEY_BUTTON_ACTION,
    HOTKEY_BUTTON_TRAIN,
    HOTKEY_BUTTON_BUILD,
    HOTKEY_BUTTON_RESEARCH
};

struct HotkeyButtonActionInfo {
    const char* name;
    SpriteName sprite;
};

struct HotkeyButtonInfo {
    HotkeyButtonType type;
    union {
        HotkeyButtonActionInfo action;
        EntityType entity_type;
    };
};

HotkeyButtonInfo hotkey_get_button_info(InputAction hotkey);