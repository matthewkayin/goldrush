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

enum HotkeyButtonRequirementsType {
    HOTKEY_REQUIRES_NONE,
    HOTKEY_REQUIRES_BUILDING,
    HOTKEY_REQUIRES_UPGRADE
};

struct HotkeyButtonRequirements {
    HotkeyButtonRequirementsType type;
    union {
        EntityType building;
        uint32_t upgrade;
    };
};

struct HotkeyButtonInfo {
    HotkeyButtonType type;
    union {
        HotkeyButtonActionInfo action;
        EntityType entity_type;
        uint32_t upgrade;
    };
    HotkeyButtonRequirements requirements;
};

HotkeyButtonInfo hotkey_get_button_info(InputAction hotkey);
SpriteName hotkey_get_sprite(InputAction hotkey);