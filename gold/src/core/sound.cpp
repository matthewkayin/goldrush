#include "sound.h"

#include "defines.h"
#include "core/logger.h"
#include <SDL2/SDL_mixer.h>
#include <vector>
#include <unordered_map>
#include <cstdlib>

struct SoundParams {
    const char* path;
    int variants;
};

static const std::unordered_map<SoundName, SoundParams> SOUND_PARAMS = {
    { SOUND_UI_CLICK, (SoundParams) {
        .path = "ui_select",
        .variants = 1
    }}
};

struct SoundState {
    std::vector<Mix_Chunk*> sounds;
    int sound_index[SOUND_COUNT];
};
static SoundState state;

bool sound_init() {
    // Init Mixer
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) < 0) {
        log_error("SDL_mixer failed to initialize: %s", Mix_GetError());
        return false;
    }

    for (int sound = 0; sound < SOUND_COUNT; sound++) {
        const SoundParams& params = SOUND_PARAMS.at((SoundName)sound);
        state.sound_index[SOUND_UI_CLICK] = state.sounds.size();

        for (int variant = 0; variant < params.variants; variant++) {
            char sound_path[256];
            if (params.variants == 1) {
                sprintf(sound_path, "%ssfx/%s.wav", RESOURCE_PATH, params.path);
            } else {
                sprintf(sound_path, "%ssfx/%s%i.wav", RESOURCE_PATH, params.path, (variant + 1));
            }

            Mix_Chunk* sound_variant = Mix_LoadWAV(sound_path);
            if (sound_variant == NULL) {
                log_error("Unable to load sound at path %s: %s", sound_path, Mix_GetError());
                return false;
            }

            state.sounds.push_back(sound_variant);
        }
    }

    log_info("Initialized sound.");

    return true;
}

void sound_quit() {
    for (Mix_Chunk* chunk : state.sounds) {
        Mix_FreeChunk(chunk);
    }

    Mix_Quit();
    log_info("Quit sound.");
}

void sound_play(SoundName sound) {
    int variant = SOUND_PARAMS.at(sound).variants == 1 ? 0 : rand() % SOUND_PARAMS.at(sound).variants;
    Mix_PlayChannel(-1, state.sounds[state.sound_index[sound] + variant], 0);
}