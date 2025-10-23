#include "sound.h"

#include "core/logger.h"
#include "core/filesystem.h"
#include <SDL3/SDL.h>
#include <vector>
#include <unordered_map>
#include <cstdlib>
#include <cstdio>

#define SFX_STREAM_COUNT 8

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
        .path = "harmonica",
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
        .path = "silenced",
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
    }}
};

struct SoundData {
    uint8_t* buffer;
    uint32_t length;
};

struct SoundState {
    SDL_AudioDeviceID audio_device;
    SDL_AudioStream* streams[SFX_STREAM_COUNT];
    SDL_AudioStream* fire_stream;

    std::vector<SoundData> sounds;
    int sound_index[SOUND_COUNT];

    bool is_fire_loop_playing;
};
static SoundState state;

bool sound_init() {
    // Open audio device
    state.audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (state.audio_device == 0) {
        log_error("Couldn't open audio device: %s", SDL_GetError());
        return false;
    }

    // Get device format
    SDL_AudioSpec device_audio_spec;
    if (!SDL_GetAudioDeviceFormat(state.audio_device, &device_audio_spec, NULL)) {
        log_error("Couldn't get audio device format: %s", SDL_GetError());
        return false;
    }

    // Create and bind SFX audio streams
    for (int stream = 0; stream < SFX_STREAM_COUNT; stream++) {
        state.streams[stream] = SDL_CreateAudioStream(&device_audio_spec, NULL);
        if (!state.streams[stream]) {
            log_error("Couldn't create audio stream: %s", SDL_GetError());
            return false;
        }
    }
    if (!SDL_BindAudioStreams(state.audio_device, state.streams, SFX_STREAM_COUNT)) {
        log_error("Couldn't bind audio streams: %s", SDL_GetError());
        return false;
    }

    // Create and bind fire audio stream
    state.fire_stream = SDL_CreateAudioStream(&device_audio_spec, NULL);
    if (!state.fire_stream) {
        log_error("Couldn't create fire audio stream: %s", SDL_GetError());
        return false;
    }
    if (!SDL_BindAudioStream(state.audio_device, state.fire_stream)) {
        log_error("Couldn't find fire audio stream: %s", SDL_GetError());
        return false;
    }

    // Load sound data
    for (int sound = 0; sound < SOUND_COUNT; sound++) {
        const SoundParams& params = SOUND_PARAMS.at((SoundName)sound);
        state.sound_index[sound] = state.sounds.size();

        for (int variant = 0; variant < params.variants; variant++) {
            // Determine sound subpath
            char sound_subpath[128];
            if (params.variants == 1) {
                sprintf(sound_subpath, "sfx/%s.wav", params.path);
            } else {
                sprintf(sound_subpath, "sfx/%s%i.wav", params.path, variant + 1);
            }

            // Determine sound full path
            char sound_path[256];
            filesystem_get_resource_path(sound_path, sound_subpath);

            // Load the sound variant
            SoundData sound_variant;
            SDL_AudioSpec sound_spec;
            if (!SDL_LoadWAV(sound_path, &sound_spec, &sound_variant.buffer, &sound_variant.length)) {
                log_error("Unable to load sound at path %s: %s", sound_path, SDL_GetError());
                return false;
            }

            // Convert sound as required
            if (sound_spec.format != device_audio_spec.format || sound_spec.channels != device_audio_spec.channels || sound_spec.freq != device_audio_spec.freq) {
                uint8_t* converted_data;
                int converted_length;
                if (!SDL_ConvertAudioSamples(&sound_spec, sound_variant.buffer, sound_variant.length, &device_audio_spec, &converted_data, &converted_length)) {
                    log_error("Failed to convert sound.");
                    return false;
                }
                SDL_free(sound_variant.buffer);
                sound_variant.buffer = converted_data;
                sound_variant.length = converted_length;
            }

            state.sounds.push_back(sound_variant);
        } // End for each variant
    } // End for each sound

    // Begin audio playback
    SDL_ResumeAudioDevice(state.audio_device);

    log_info("Audio device %s format: %i freq: %i channels: %i", SDL_GetAudioDeviceName(state.audio_device), device_audio_spec.format, device_audio_spec.freq, device_audio_spec.channels);

    return true;
}

void sound_quit() {
    SDL_PauseAudioDevice(state.audio_device);

    // Free all audio streams
    for (int stream = 0; stream < SFX_STREAM_COUNT; stream++) {
        SDL_DestroyAudioStream(state.streams[stream]);
    }
    SDL_DestroyAudioStream(state.fire_stream);

    // Free all sound data
    for (SoundData sound_data : state.sounds) {
        SDL_free(sound_data.buffer);
    }

    SDL_CloseAudioDevice(state.audio_device);

    log_info("Quit sound.");
}

void sound_set_sfx_volume(int volume) {
    for (int stream = 0; stream < SFX_STREAM_COUNT; stream++) {
        SDL_SetAudioStreamGain(state.streams[stream], (float)volume / 100.0f);
    }
    SDL_SetAudioStreamGain(state.fire_stream, (float)volume / 100.0f);
}

void sound_set_mus_volume(int volume) {}

void sound_update() {
    if (state.is_fire_loop_playing) {
        SoundData& sound_fire = state.sounds[state.sound_index[SOUND_FIRE_BURN]];
        if (SDL_GetAudioStreamQueued(state.fire_stream) < (int)sound_fire.length) {
            SDL_PutAudioStreamData(state.fire_stream, sound_fire.buffer, sound_fire.length);
        }
    }
}

void sound_play(SoundName sound) { 
    // Determine the index of the audio stream to play the sound on
    // We will choose the stream with the least amount of data queued
    // in order to avoid cutting off a sound mid-playback
    int available_stream = 0;
    int min_data_available = SDL_GetAudioStreamQueued(state.streams[0]);
    for (int stream = 1; stream < SFX_STREAM_COUNT; stream++) {
        int stream_data_available = SDL_GetAudioStreamQueued(state.streams[stream]);
        if (stream_data_available < min_data_available) {
            available_stream = stream;
            min_data_available = stream_data_available;
            break;
        }
    }

    int variant = SOUND_PARAMS.at(sound).variants == 1 ? 0 : rand() % SOUND_PARAMS.at(sound).variants;
    SoundData& sound_data = state.sounds[state.sound_index[sound] + variant];
    SDL_ClearAudioStream(state.streams[available_stream]);
    SDL_PutAudioStreamData(state.streams[available_stream], sound_data.buffer, sound_data.length);
}

bool sound_is_fire_loop_playing() {
    return state.is_fire_loop_playing;
}

void sound_begin_fire_loop() {
    SoundData& sound_fire = state.sounds[state.sound_index[SOUND_FIRE_BURN]];
    SDL_PutAudioStreamData(state.fire_stream, sound_fire.buffer, sound_fire.length);
    state.is_fire_loop_playing = true;
}

void sound_end_fire_loop() {
    SDL_ClearAudioStream(state.fire_stream);
    state.is_fire_loop_playing = false;
}