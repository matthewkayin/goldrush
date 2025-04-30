#pragma once

#include "render/sprite.h"
#include "core/input.h"
#include "entity.h"

enum HotkeyButtonType {
    HOTKEY_BUTTON_ACTION,
    HOTKEY_BUTTON_TOGGLED_ACTION,
    HOTKEY_BUTTON_TRAIN,
    HOTKEY_BUTTON_BUILD,
    HOTKEY_BUTTON_RESEARCH
};

struct HotkeyButtonActionInfo {
    const char* name;
    SpriteName sprite;
};

struct HotkeyButtonToggledActionInfo {
    const char* activate_name;
    SpriteName activate_sprite;
    const char* deactivate_name;
    SpriteName deactivate_sprite;
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
        HotkeyButtonToggledActionInfo toggled_action;
        EntityType entity_type;
        uint32_t upgrade;
    };
    HotkeyButtonRequirements requirements;
};

HotkeyButtonInfo hotkey_get_button_info(InputAction hotkey);
SpriteName hotkey_get_sprite(InputAction hotkey, bool show_toggled = false);
const char* hotkey_get_desc(InputAction hotkey);
int hotkey_get_name(char* str, InputAction hotkey, bool show_toggled = false);