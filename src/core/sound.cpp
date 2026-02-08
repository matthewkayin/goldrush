#include "sound.h"

#include "core/logger.h"
#include "core/asserts.h"
#include "core/filesystem.h"
#include "core/options.h"
#include <SDL3/SDL.h>
#include <vector>
#include <unordered_map>
#include <cstdlib>
#include <cstdio>

#define SOUND_AUDIO_CHANNEL_COUNT 2
#define SOUND_VOICE_COUNT 32
#define SOUND_MIX_BUFFER_SIZE 4096

struct SoundParams {
    const char* path;
    int variants;
};

static const std::unordered_map<SoundName, SoundParams> SOUND_PARAMS = {
    { SOUND_UI_CLICK, (SoundParams) {
        .path = "ui_click",
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
        .path = "pickaxe",
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
    { SOUND_MINE_PRIME, (SoundParams) {
        .path = "mine_prime",
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
        .path = "alert_unit",
        .variants = 1
    }},
    { SOUND_GOLD_MINE_COLLAPSE, (SoundParams) {
        .path = "gold_mine_collapse",
        .variants = 1
    }},
    { SOUND_MOLOTOV_IMPACT, (SoundParams) {
        .path = "molotov_impact",
        .variants = 1
    }},
    { SOUND_FIRE_BURN, (SoundParams) {
        .path = "fire_burn",
        .variants = 1
    }},
    { SOUND_PISTOL_SILENCED, (SoundParams) {
        .path = "pistol_silenced",
        .variants = 3
    }},
    { SOUND_CAMO_ON, (SoundParams) {
        .path = "camo_on",
        .variants = 1
    }},
    { SOUND_CAMO_OFF, (SoundParams) {
        .path = "camo_off",
        .variants = 1
    }},
    { SOUND_BALLOON_DEATH, (SoundParams) {
        .path = "balloon_death",
        .variants = 1
    }},
    { SOUND_RICOCHET, (SoundParams) {
        .path = "ricochet",
        .variants = 5
    }},
    { SOUND_OBJECTIVE_COMPLETE, (SoundParams) {
        .path = "objective_complete",
        .variants = 1
    }},
    { SOUND_MATCH_START, (SoundParams) {
        .path = "match_start",
        .variants = 1
    }}
};

struct SoundData {
    float* samples;
    int frame_count;
};

enum SoundVoiceMode {
    SOUND_VOICE_OFF,
    SOUND_VOICE_PLAYING,
    SOUND_VOICE_LOOPING
};

struct SoundVoice {
    SoundVoiceMode mode = SOUND_VOICE_OFF;
    int sound_index;
    int frame;
};

struct SoundState {
    SDL_AudioStream* audio_stream;

    std::vector<SoundData> sounds;
    int sound_index[SOUND_COUNT];

    SoundVoice voices[SOUND_VOICE_COUNT];

    bool is_fire_loop_playing;
};

static SoundState state;

static void sound_sdl_audio_callback(void* /*user_data*/, SDL_AudioStream* stream, int additional_amount, int /*total_amount*/) {
    const int BYTES_PER_FRAME = SOUND_AUDIO_CHANNEL_COUNT * sizeof(float);

    int requested_frames = additional_amount / BYTES_PER_FRAME;
    float mix_buffer[SOUND_MIX_BUFFER_SIZE * SOUND_AUDIO_CHANNEL_COUNT];
    memset(mix_buffer, 0, sizeof(mix_buffer));
    GOLD_ASSERT(requested_frames * SOUND_AUDIO_CHANNEL_COUNT < SOUND_MIX_BUFFER_SIZE);

    for (int voice_index = 0; voice_index < SOUND_VOICE_COUNT; voice_index++) {
        SoundVoice* voice = &state.voices[voice_index];
        if (voice->mode == SOUND_VOICE_OFF) {
            continue;
        }

        const SoundData* sound_data = &state.sounds[voice->sound_index];
        for (int frame = 0; frame < requested_frames; frame++) {
            int mix_buffer_index = frame * 2; 
            int sound_data_index = voice->frame * 2;

            // Once for each channel
            mix_buffer[mix_buffer_index + 0] += sound_data->samples[sound_data_index + 0];
            mix_buffer[mix_buffer_index + 1] += sound_data->samples[sound_data_index + 1];

            voice->frame++;
            if (voice->frame == sound_data->frame_count) {
                if (voice->mode == SOUND_VOICE_LOOPING) {
                    voice->frame = 0;
                } else {
                    voice->mode = SOUND_VOICE_OFF;
                    break;
                }
            }
        }
    }

    SDL_PutAudioStreamData(stream, mix_buffer, requested_frames * BYTES_PER_FRAME);
}

bool sound_init() {
    const SDL_AudioSpec AUDIO_SPEC = {
        .format = SDL_AUDIO_F32,
        .channels = SOUND_AUDIO_CHANNEL_COUNT,
        .freq = 48000
    };

    state.audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &AUDIO_SPEC, sound_sdl_audio_callback, NULL);
    if (state.audio_stream == NULL) {
        log_error("Couldn't open audio device stream: %s", SDL_GetError());
        return false;
    }

    // Load sound data
    for (int sound = 0; sound < SOUND_COUNT; sound++) {
        const SoundParams& params = SOUND_PARAMS.at((SoundName)sound);
        state.sound_index[sound] = (int)state.sounds.size();

        for (int variant = 0; variant < params.variants; variant++) {
            // Determine sound full path
            std::string sound_path = filesystem_get_resource_path() + "sfx/" + params.path + (params.variants == 1 ? "" : std::to_string(variant + 1)) + ".wav";

            // Load the sound variant
            uint8_t* src_data;
            uint32_t src_length;
            SDL_AudioSpec sound_spec;
            if (!SDL_LoadWAV(sound_path.c_str(), &sound_spec, &src_data, &src_length)) {
                log_error("Unable to load sound at path %s: %s", sound_path.c_str(), SDL_GetError());
                return false;
            }

            // Convert sound as required
            uint8_t* converted_data;
            int converted_length;
            if (!SDL_ConvertAudioSamples(&sound_spec, src_data, (int)src_length, &AUDIO_SPEC, &converted_data, &converted_length)) {
                log_error("Failed to convert sound.");
                return false;
            }
            SDL_free(src_data);

            SoundData sound_variant;
            sound_variant.samples = (float*)converted_data;
            sound_variant.frame_count = converted_length / (SOUND_AUDIO_CHANNEL_COUNT * (sizeof(float)));
            state.sounds.push_back(sound_variant);
        } // End for each variant
    } // End for each sound

    // Begin audio playback
    SDL_AudioDeviceID audio_device = SDL_GetAudioStreamDevice(state.audio_stream);
    SDL_ResumeAudioDevice(audio_device);

    option_apply(OPTION_SFX_VOLUME);
    option_apply(OPTION_MUSIC_VOLUME);
    log_info("Initialized sound system. Device: %s", SDL_GetAudioDeviceName(audio_device));

    return true;
}

void sound_quit() {
    SDL_PauseAudioStreamDevice(state.audio_stream);

    // Also closes the associated device
    SDL_DestroyAudioStream(state.audio_stream);

    // Free all sound data
    for (SoundData sound_data : state.sounds) {
        SDL_free(sound_data.samples);
    }

    log_info("Quit sound.");
}

const char* sound_get_name(SoundName sound) {
    return SOUND_PARAMS.at(sound).path;
}

void sound_set_sfx_volume(uint32_t volume) {
    SDL_SetAudioStreamGain(state.audio_stream, (float)volume / 100.0f);
}

void sound_set_music_volume(uint32_t volume) {}

int sound_voice_frames_remaining(const SoundVoice* voice) {
    // This should only be called on a playing voice
    GOLD_ASSERT(voice->mode == SOUND_VOICE_PLAYING);
    return state.sounds[voice->sound_index].frame_count - voice->frame;
}

uint32_t sound_play(SoundName sound, bool looping) { 
    uint32_t available_voice_index = SOUND_VOICE_COUNT;
    for (uint32_t voice_index = 0; voice_index < SOUND_VOICE_COUNT; voice_index++) {
        // Never interrupt a looping voice
        if (state.voices[voice_index].mode == SOUND_VOICE_LOOPING) {
            continue;
        }

        // If there is an off voice, just use that one
        if (state.voices[voice_index].mode == SOUND_VOICE_OFF) {
            available_voice_index = voice_index;
            break;
        }

        // If the voice is active but not looping, use it if it is closer to finishing than all the others
        if (state.voices[voice_index].mode == SOUND_VOICE_PLAYING &&
                (available_voice_index == SOUND_VOICE_COUNT || 
                    sound_voice_frames_remaining(&state.voices[voice_index]) < 
                    sound_voice_frames_remaining(&state.voices[available_voice_index]))) {
            available_voice_index = voice_index;
        }
    }
    GOLD_ASSERT(available_voice_index != SOUND_VOICE_COUNT);

    state.voices[available_voice_index].mode = looping
        ? SOUND_VOICE_LOOPING
        : SOUND_VOICE_PLAYING;
    int variant = SOUND_PARAMS.at(sound).variants == 1 
        ? 0 
        : rand() % SOUND_PARAMS.at(sound).variants;
    state.voices[available_voice_index].sound_index = state.sound_index[sound] + variant;
    state.voices[available_voice_index].frame = 0;

    return available_voice_index;
}

void sound_stop(uint32_t voice_index) {
    state.voices[voice_index].mode = SOUND_VOICE_OFF;
}

void sound_stop_all() {
    for (uint32_t voice_index = 0; voice_index < SOUND_VOICE_COUNT; voice_index++) {
        state.voices[voice_index].mode = SOUND_VOICE_OFF;
    }
}