#include "hotkey.h"

#include "upgrade.h"
#include <unordered_map>

static const std::unordered_map<InputAction, HotkeyButtonInfo> HOTKEY_BUTTON_INFO = {
    { INPUT_HOTKEY_ATTACK, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_ACTION,
        .action = (HotkeyButtonActionInfo) {
            .name = "Attack",
            .sprite = SPRITE_BUTTON_ICON_ATTACK
        },
        .requirements = (HotkeyButtonRequirements) { 
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_STOP, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_ACTION,
        .action = (HotkeyButtonActionInfo) {
            .name = "Stop",
            .sprite = SPRITE_BUTTON_ICON_STOP
        },
        .requirements = (HotkeyButtonRequirements) { 
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_DEFEND, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_ACTION,
        .action = (HotkeyButtonActionInfo) {
            .name = "Hold Position",
            .sprite = SPRITE_BUTTON_ICON_DEFEND
        },
        .requirements = (HotkeyButtonRequirements) { 
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_REPAIR, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_ACTION,
        .action = (HotkeyButtonActionInfo) {
            .name = "Repair",
            .sprite = SPRITE_BUTTON_ICON_REPAIR
        },
        .requirements = (HotkeyButtonRequirements) { 
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_BUILD, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_ACTION,
        .action = (HotkeyButtonActionInfo) {
            .name = "Build",
            .sprite = SPRITE_BUTTON_ICON_BUILD
        },
        .requirements = (HotkeyButtonRequirements) { 
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_BUILD2, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_ACTION,
        .action = (HotkeyButtonActionInfo) {
            .name = "Advanced Build",
            .sprite = SPRITE_BUTTON_ICON_BUILD2
        },
        .requirements = (HotkeyButtonRequirements) { 
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_UNLOAD, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_ACTION,
        .action = (HotkeyButtonActionInfo) {
            .name = "Unload",
            .sprite = SPRITE_BUTTON_ICON_UNLOAD
        },
        .requirements = (HotkeyButtonRequirements) { 
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_CANCEL, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_ACTION,
        .action = (HotkeyButtonActionInfo) {
            .name = "Cancel",
            .sprite = SPRITE_BUTTON_ICON_CANCEL
        },
        .requirements = (HotkeyButtonRequirements) { 
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_MOLOTOV, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_ACTION,
        .action = (HotkeyButtonActionInfo) {
            .name = "Throw Molotov",
            .sprite = SPRITE_BUTTON_ICON_MOLOTOV
        },
        .requirements = (HotkeyButtonRequirements) {
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_CAMO, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_TOGGLED_ACTION,
        .toggled_action = (HotkeyButtonToggledActionInfo) {
            .activate_name = "Camouflage",
            .activate_sprite = SPRITE_BUTTON_ICON_CAMO,
            .deactivate_name = "Remove Camouflage",
            .deactivate_sprite = SPRITE_BUTTON_ICON_DECAMO
        },
        .requirements = (HotkeyButtonRequirements) {
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_HALL, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_BUILD,
        .entity_type = ENTITY_HALL,
        .requirements = (HotkeyButtonRequirements) { 
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_HOUSE, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_BUILD,
        .entity_type = ENTITY_HOUSE,
        .requirements = (HotkeyButtonRequirements) { 
            .type = HOTKEY_REQUIRES_BUILDING,
            .building = ENTITY_HALL
        }
    }},
    { INPUT_HOTKEY_SALOON, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_BUILD,
        .entity_type = ENTITY_SALOON,
        .requirements = (HotkeyButtonRequirements) { 
            .type = HOTKEY_REQUIRES_BUILDING,
            .building = ENTITY_HALL
        }
    }},
    { INPUT_HOTKEY_WORKSHOP, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_BUILD,
        .entity_type = ENTITY_WORKSHOP,
        .requirements = (HotkeyButtonRequirements) { 
            .type = HOTKEY_REQUIRES_BUILDING,
            .building = ENTITY_SALOON
        }
    }},
    { INPUT_HOTKEY_BUNKER, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_BUILD,
        .entity_type = ENTITY_BUNKER,
        .requirements = (HotkeyButtonRequirements) { 
            .type = HOTKEY_REQUIRES_BUILDING,
            .building = ENTITY_SALOON
        }
    }},
    { INPUT_HOTKEY_SMITH, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_BUILD,
        .entity_type = ENTITY_SMITH,
        .requirements = (HotkeyButtonRequirements) { 
            .type = HOTKEY_REQUIRES_BUILDING,
            .building = ENTITY_SALOON
        }
    }},
    { INPUT_HOTKEY_COOP, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_BUILD,
        .entity_type = ENTITY_COOP,
        .requirements = (HotkeyButtonRequirements) { 
            .type = HOTKEY_REQUIRES_BUILDING,
            .building = ENTITY_SMITH
        }
    }},
    { INPUT_HOTKEY_BARRACKS, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_BUILD,
        .entity_type = ENTITY_BARRACKS,
        .requirements = (HotkeyButtonRequirements) { 
            .type = HOTKEY_REQUIRES_BUILDING,
            .building = ENTITY_SMITH
        }
    }},
    { INPUT_HOTKEY_SHERIFFS, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_BUILD,
        .entity_type = ENTITY_SHERIFFS,
        .requirements = (HotkeyButtonRequirements) { 
            .type = HOTKEY_REQUIRES_BUILDING,
            .building = ENTITY_SMITH
        }
    }},
    { INPUT_HOTKEY_MINER, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_TRAIN,
        .entity_type = ENTITY_MINER,
        .requirements = (HotkeyButtonRequirements) {
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_COWBOY, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_TRAIN,
        .entity_type = ENTITY_COWBOY,
        .requirements = (HotkeyButtonRequirements) {
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_BANDIT, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_TRAIN,
        .entity_type = ENTITY_BANDIT,
        .requirements = (HotkeyButtonRequirements) {
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_SAPPER, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_TRAIN,
        .entity_type = ENTITY_SAPPER,
        .requirements = (HotkeyButtonRequirements) {
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_PYRO, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_TRAIN,
        .entity_type = ENTITY_PYRO,
        .requirements = (HotkeyButtonRequirements) {
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_WAGON, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_TRAIN,
        .entity_type = ENTITY_WAGON,
        .requirements = (HotkeyButtonRequirements) {
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_JOCKEY, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_TRAIN,
        .entity_type = ENTITY_JOCKEY,
        .requirements = (HotkeyButtonRequirements) {
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_SOLDIER, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_TRAIN,
        .entity_type = ENTITY_SOLDIER,
        .requirements = (HotkeyButtonRequirements) {
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_CANNON, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_TRAIN,
        .entity_type = ENTITY_CANNON,
        .requirements = (HotkeyButtonRequirements) {
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_DETECTIVE, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_TRAIN,
        .entity_type = ENTITY_DETECTIVE,
        .requirements = (HotkeyButtonRequirements) {
            .type = HOTKEY_REQUIRES_BUILDING,
            .building = ENTITY_SHERIFFS
        }
    }},
    { INPUT_HOTKEY_BALLOON, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_TRAIN,
        .entity_type = ENTITY_BALLOON,
        .requirements = (HotkeyButtonRequirements) {
            .type = HOTKEY_REQUIRES_NONE,
        }
    }},
    { INPUT_HOTKEY_LANDMINE, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_BUILD,
        .entity_type = ENTITY_LANDMINE,
        .requirements = (HotkeyButtonRequirements) {
            .type = HOTKEY_REQUIRES_UPGRADE,
            .upgrade = UPGRADE_LANDMINES
        }
    }},
    { INPUT_HOTKEY_RESEARCH_LANDMINES, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_RESEARCH,
        .upgrade = UPGRADE_LANDMINES,
        .requirements = (HotkeyButtonRequirements) {
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_RESEARCH_WAGON_ARMOR, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_RESEARCH,
        .upgrade = UPGRADE_WAR_WAGON,
        .requirements = (HotkeyButtonRequirements) {
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_RESEARCH_BAYONETS, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_RESEARCH,
        .upgrade = UPGRADE_BAYONETS,
        .requirements = (HotkeyButtonRequirements) {
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_RESEARCH_PRIVATE_EYE, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_RESEARCH,
        .upgrade = UPGRADE_PRIVATE_EYE,
        .requirements = (HotkeyButtonRequirements) {
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_RESEARCH_STAKEOUT, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_RESEARCH,
        .upgrade = UPGRADE_STAKEOUT,
        .requirements = (HotkeyButtonRequirements) {
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_RESEARCH_SERRATED_KNIVES, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_RESEARCH,
        .upgrade = UPGRADE_SERRATED_KNIVES,
        .requirements = (HotkeyButtonRequirements) {
            .type = HOTKEY_REQUIRES_NONE
        }
    }},
    { INPUT_HOTKEY_RESEARCH_FAN_HAMMER, (HotkeyButtonInfo) {
        .type = HOTKEY_BUTTON_RESEARCH,
        .upgrade = UPGRADE_FAN_HAMMER,
        .requirements = (HotkeyButtonRequirements) {
            .type = HOTKEY_REQUIRES_NONE
        }
    }}
};

HotkeyButtonInfo hotkey_get_button_info(InputAction hotkey) {
    return HOTKEY_BUTTON_INFO.at(hotkey);
}

SpriteName hotkey_get_sprite(InputAction hotkey, bool show_toggled) {
    const HotkeyButtonInfo& info = HOTKEY_BUTTON_INFO.at(hotkey);
    switch (info.type) {
        case HOTKEY_BUTTON_ACTION:
            return info.action.sprite;
        case HOTKEY_BUTTON_TOGGLED_ACTION:
            return show_toggled ? info.toggled_action.deactivate_sprite : info.toggled_action.activate_sprite;
        case HOTKEY_BUTTON_BUILD:
        case HOTKEY_BUTTON_TRAIN:
            return entity_get_data(info.entity_type).icon;
        case HOTKEY_BUTTON_RESEARCH:
            return upgrade_get_data(info.upgrade).icon;
    }
}

const char* hotkey_get_desc(InputAction hotkey) {
    const HotkeyButtonInfo& info = HOTKEY_BUTTON_INFO.at(hotkey);
    if (info.type == HOTKEY_BUTTON_RESEARCH) {
        return upgrade_get_data(info.upgrade).desc;
    }
    switch (hotkey) {
        default:
            return "";
    }
}

int hotkey_get_name(char* str, InputAction hotkey, bool show_toggled) {
    int size = 0;
    const HotkeyButtonInfo& info = HOTKEY_BUTTON_INFO.at(hotkey);
    switch (info.type) {
        case HOTKEY_BUTTON_ACTION:
            size += sprintf(str, "%s", info.action.name);
            break;
        case HOTKEY_BUTTON_TOGGLED_ACTION:
            size += sprintf(str, "%s", show_toggled ? info.toggled_action.deactivate_name : info.toggled_action.activate_name);
            break;
        case HOTKEY_BUTTON_BUILD:
        case HOTKEY_BUTTON_TRAIN:
            size += sprintf(str, "%s %s", info.type == HOTKEY_BUTTON_BUILD ? "Build" : "Hire", entity_get_data(info.entity_type).name);
            break;
        case HOTKEY_BUTTON_RESEARCH:
            size += sprintf(str, "Research %s", upgrade_get_data(info.upgrade).name);
            break;
    }

    return size;
}