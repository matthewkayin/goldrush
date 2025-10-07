#include "profile.h"

#include "defines.h"

#ifdef GOLD_PROFILE_ENABLED

#include "logger.h"
#include <SDL3/SDL.h>

struct ProfileState {
    uint64_t key_start_time[PROFILE_KEY_COUNT];
    double key_duration[PROFILE_KEY_COUNT];
};
static ProfileState state;

void profile_begin_frame() {
    static bool initialized = false;
    memset(state.key_start_time, 0, sizeof(state.key_start_time));
    if (!initialized) {
        memset(state.key_duration, 0, sizeof(state.key_duration));
        initialized = true;
    }
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

const char* profile_key_str(ProfileKey key) {
    switch (key) {
        case PROFILE_KEY_UPDATE: 
            return "UPDATE";
        case PROFILE_KEY_BOT: 
            return "BOT";
        case PROFILE_KEY_BOT_STRATEGY: 
            return "BOT STRATEGY";
        case PROFILE_KEY_BOT_BANDIT_RUSH:
            return "BOT BANDIT RUSH";
        case PROFILE_KEY_BOT_BUNKER:
            return "BOT BUNKER";
        case PROFILE_KEY_BOT_EXPAND:
            return "BOT EXPAND";
        case PROFILE_KEY_BOT_PRODUCTION: 
            return "BOT PRODUCTION";
        case PROFILE_KEY_BOT_SQUAD: 
            return "BOT SQUAD";
        case PROFILE_KEY_BOT_SCOUT: 
            return "BOT SCOUT";
        case PROFILE_KEY_BOT_MISC: 
            return "BOT MISC";
        case PROFILE_KEY_COUNT: 
            return "COUNT";
    }
}

double profile_key_duration(ProfileKey key) {
    return state.key_duration[key];
}

double profile_key_percentage(ProfileKey key) {
    static const double UPDATE_DURATION = 1.0 / UPDATES_PER_SECOND;
    return (state.key_duration[key] / UPDATE_DURATION) * 100.0;
}

#else

void profile_begin_frame() {}
void profile_begin(ProfileKey key) {}
void profile_end(ProfileKey key) {}
const char* profile_key_str(ProfileKey key) { return ""; }
double profile_key_duration(ProfileKey key) { return 0.0; }
double profile_key_percentage(ProfileKey key) { return 0.0; }

#endif