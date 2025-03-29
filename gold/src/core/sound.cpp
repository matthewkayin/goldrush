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
    }},
    { SOUND_DEATH, (SoundParams) {
        .path = "death",
        .variants = 9
    }},
    { SOUND_MUSKET, (SoundParams) {
        .path = "musket",
        .variants = 9
    }},
    { SOUND_GUN, (SoundParams) {
        .path = "gun",
        .variants = 5
    }},
    { SOUND_EXPLOSION, (SoundParams) {
        .path = "explosion",
        .variants = 4
    }},
    { SOUND_PICKAXE, (SoundParams) {
        .path = "impact",
        .variants = 3
    }},
    { SOUND_HAMMER, (SoundParams) {
        .path = "hammer",
        .variants = 4
    }},
    { SOUND_BUILDING_PLACE, (SoundParams) {
        .path = "building_place",
        .variants = 1
    }},
    { SOUND_SWORD, (SoundParams) {
        .path = "sword",
        .variants = 3
    }},
    { SOUND_PUNCH, (SoundParams) {
        .path = "punch",
        .variants = 3
    }},
    { SOUND_DEATH_CHICKEN, (SoundParams) {
        .path = "death_chicken",
        .variants = 4
    }},
    { SOUND_CANNON, (SoundParams) {
        .path = "cannon",
        .variants = 4
    }},
    { SOUND_BUILDING_DESTROY, (SoundParams) {
        .path = "building_destroy",
        .variants = 1
    }},
    { SOUND_BUNKER_DESTROY, (SoundParams) {
        .path = "bunker_destroy",
        .variants = 1
    }},
    { SOUND_MINE_DESTROY, (SoundParams) {
        .path = "mine_destroy",
        .variants = 1
    }},
    { SOUND_MINE_INSERT, (SoundParams) {
        .path = "mine_insert",
        .variants = 1
    }},
    { SOUND_MINE_ARM, (SoundParams) {
        .path = "mine_arm",
        .variants = 1
    }},
    { SOUND_MINE_PRIME, (SoundParams) {
        .path = "mine_prime",
        .variants = 1
    }},
    { SOUND_SMOKE, (SoundParams) {
        .path = "smoke",
        .variants = 1
    }},
    { SOUND_THROW, (SoundParams) {
        .path = "throw",
        .variants = 1
    }},
    { SOUND_FLAG_THUMP, (SoundParams) {
        .path = "flag_thump",
        .variants = 1
    }},
    { SOUND_GARRISON_IN, (SoundParams) {
        .path = "garrison_in",
        .variants = 1
    }},
    { SOUND_GARRISON_OUT, (SoundParams) {
        .path = "garrison_out",
        .variants = 1
    }},
    { SOUND_ALERT_BELL, (SoundParams) {
        .path = "alert_bell",
        .variants = 1
    }},
    { SOUND_ALERT_BUILDING, (SoundParams) {
        .path = "alert_building",
        .variants = 1
    }},
    { SOUND_ALERT_RESEARCH, (SoundParams) {
        .path = "alert_research",
        .variants = 1
    }},
    { SOUND_ALERT_UNIT, (SoundParams) {
        .path = "harmonica",
        .variants = 1
    }},
    { SOUND_GOLD_MINE_COLLAPSE, (SoundParams) {
        .path = "gold_mine_collapse",
        .variants = 1
    }},
    { SOUND_UNIT_HEY, (SoundParams) {
        .path = "voice_hey",
        .variants = 6
    }},
    { SOUND_UNIT_OK, (SoundParams) {
        .path = "voice_awk",
        .variants = 7
    }},
    { SOUND_UNIT_HAW, (SoundParams) {
        .path = "voice_haw",
        .variants = 5
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