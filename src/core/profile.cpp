#include "profile.h"

#include "defines.h"

#ifdef GOLD_PROFILE_ENABLED

#include "logger.h"
#include <SDL3/SDL.h>

struct ProfileState {
    uint64_t frame_start_time;
    double frame_duration;
    uint64_t key_start_time[PROFILE_KEY_COUNT];
    double key_duration[PROFILE_KEY_COUNT];
    double last_key_duration[PROFILE_KEY_COUNT];
    double last_total_duration;
};
static ProfileState state;

void profile_begin_frame() {
    static bool first_run = true;
    memset(state.key_start_time, 0, sizeof(state.key_start_time));
    if (first_run) {
        memset(state.last_key_duration, 0, sizeof(state.last_key_duration));
        state.last_total_duration = 0.0;
        first_run = false;
    } else if (state.key_duration[PROFILE_KEY_BOT] != 0.0) {
        memcpy(state.last_key_duration, state.key_duration, sizeof(state.key_duration));
        state.last_total_duration = state.frame_duration;
    }
    memset(state.key_duration, 0, sizeof(state.key_duration));
    state.frame_start_time = SDL_GetTicksNS();
}

void profile_begin(ProfileKey key) {
    state.key_start_time[key] = SDL_GetTicksNS();
}

void profile_end(ProfileKey key) {
    if (state.key_start_time[key] == 0) {
        return;
    }
    state.key_duration[key] = (double)(SDL_GetTicksNS() - state.key_start_time[key]) / (double)SDL_NS_PER_SECOND;
    state.key_start_time[key] = 0;
}

void profile_end_frame() {
    state.frame_duration = (double)(SDL_GetTicksNS() - state.frame_start_time) / (double)SDL_NS_PER_SECOND;
}

const char* profile_key_str(ProfileKey key) {
    switch (key) {
        case PROFILE_KEY_RENDER: 
            return "RENDER";
        case PROFILE_KEY_UPDATE: 
            return "UPDATE";
        case PROFILE_KEY_BOT: 
            return "BOT";
        case PROFILE_KEY_BOT_STRATEGY: 
            return "BOT STRATEGY";
        case PROFILE_KEY_BOT_PRODUCTION: 
            return "BOT PRODUCTION";
        case PROFILE_KEY_BOT_SQUAD: 
            return "BOT SQUAD";
        case PROFILE_KEY_BOT_MISC: 
            return "BOT MISC";
        case PROFILE_KEY_COUNT: 
            return "COUNT";
    }
}

double profile_key_duration(ProfileKey key) {
    return (state.last_key_duration[key] / state.last_total_duration) * 100.0;
}

#else

void profile_begin_frame() {}
void profile_begin(ProfileKey key) {}
void profile_end(ProfileKey key) {}
void profile_end_frame() {}
const char* profile_key_str(ProfileKey key) { return ""; }
double profile_key_duration(ProfileKey key) { return 0.0; }

#endif