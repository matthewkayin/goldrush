#pragma once

#include "defines.h"

enum SoundName {
    SOUND_UI_CLICK,
    SOUND_DEATH,
    SOUND_MUSKET,
    SOUND_GUN,
    SOUND_EXPLOSION,
    SOUND_PICKAXE,
    SOUND_HAMMER,
    SOUND_BUILDING_PLACE,
    SOUND_SWORD,
    SOUND_DEATH_CHICKEN,
    SOUND_CANNON,
    SOUND_BUILDING_DESTROY,
    SOUND_BUNKER_DESTROY,
    SOUND_MINE_DESTROY,
    SOUND_MINE_INSERT,
    SOUND_MINE_PRIME,
    SOUND_THROW,
    SOUND_FLAG_THUMP,
    SOUND_GARRISON_IN,
    SOUND_GARRISON_OUT,
    SOUND_ALERT_BELL,
    SOUND_ALERT_BUILDING,
    SOUND_ALERT_RESEARCH,
    SOUND_ALERT_UNIT,
    SOUND_GOLD_MINE_COLLAPSE,
    SOUND_MOLOTOV_IMPACT,
    SOUND_FIRE_BURN,
    SOUND_PISTOL_SILENCED,
    SOUND_CAMO_ON,
    SOUND_CAMO_OFF,
    SOUND_BALLOON_DEATH,
    SOUND_RICOCHET,
    SOUND_OBJECTIVE_COMPLETE,
    SOUND_COUNT
};

bool sound_init();
void sound_quit();
const char* sound_get_name(SoundName sound);
void sound_set_sfx_volume(uint32_t volume);
void sound_set_mus_volume(uint32_t volume);

void sound_play(SoundName sound);
void sound_update();
bool sound_is_fire_loop_playing();
void sound_begin_fire_loop();
void sound_end_fire_loop();