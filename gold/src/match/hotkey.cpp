#include "hotkey.h"

#include <unordered_map>

static const std::unordered_map<InputAction, HotkeyButtonInfo> HOTKEY_BUTTON_INFO = {
    { INPUT_HOTKEY_ATTACK, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_ACTION,
        .action = (HotkeyButtonActionInfo) {
            .name = "Attack",
            .sprite = SPRITE_BUTTON_ICON_ATTACK
        }
    }},
    { INPUT_HOTKEY_STOP, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_ACTION,
        .action = (HotkeyButtonActionInfo) {
            .name = "Stop",
            .sprite = SPRITE_BUTTON_ICON_STOP
        }
    }},
    { INPUT_HOTKEY_DEFEND, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_ACTION,
        .action = (HotkeyButtonActionInfo) {
            .name = "Defend",
            .sprite = SPRITE_BUTTON_ICON_DEFEND
        }
    }},
    { INPUT_HOTKEY_REPAIR, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_ACTION,
        .action = (HotkeyButtonActionInfo) {
            .name = "Repair",
            .sprite = SPRITE_BUTTON_ICON_REPAIR
        }
    }},
    { INPUT_HOTKEY_BUILD, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_ACTION,
        .action = (HotkeyButtonActionInfo) {
            .name = "Build",
            .sprite = SPRITE_BUTTON_ICON_BUILD
        }
    }},
    { INPUT_HOTKEY_BUILD2, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_ACTION,
        .action = (HotkeyButtonActionInfo) {
            .name = "Advanced Build",
            .sprite = SPRITE_BUTTON_ICON_BUILD2
        }
    }},
    { INPUT_HOTKEY_CANCEL, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_ACTION,
        .action = (HotkeyButtonActionInfo) {
            .name = "Cancel",
            .sprite = SPRITE_BUTTON_ICON_CANCEL
        }
    }},
    { INPUT_HOTKEY_HALL, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_BUILD,
        .entity_type = ENTITY_HALL
    }},
    { INPUT_HOTKEY_HOUSE, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_BUILD,
        .entity_type = ENTITY_HOUSE
    }},
    { INPUT_HOTKEY_SALOON, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_BUILD,
        .entity_type = ENTITY_SALOON
    }},
    { INPUT_HOTKEY_SMITH, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_BUILD,
        .entity_type = ENTITY_SMITH
    }},
    { INPUT_HOTKEY_BUNKER, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_BUILD,
        .entity_type = ENTITY_BUNKER
    }}
};

HotkeyButtonInfo hotkey_get_button_info(InputAction hotkey) {
    return HOTKEY_BUTTON_INFO.at(hotkey);
}