#pragma once

enum SoundName {
    SOUND_UI_CLICK,
    SOUND_COUNT
};

bool sound_init();
void sound_quit();
void sound_play(SoundName sound);