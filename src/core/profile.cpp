#include "profile.h"

#include "defines.h"

#ifdef GOLD_PROFILE_ENABLED

#include "logger.h"
#include <SDL3/SDL.h>
#include <cstdio>

struct ProfileState {
    uint64_t key_start_time[PROFILE_KEY_COUNT];
    double key_duration[PROFILE_KEY_COUNT];
    uint32_t key_occurances[PROFILE_KEY_COUNT];
};
static ProfileState state;

void profile_begin_frame() {
    memset(&state, 0, sizeof(state));
}

void profile_begin(ProfileKey key) {
    state.key_start_time[key] = SDL_GetTicksNS();
}

double profile_end(ProfileKey key) {
    if (state.key_start_time[key] == 0) {
        return 0.0;
    }
    double duration = (double)(SDL_GetTicksNS() - state.key_start_time[key]) / (double)SDL_NS_PER_SECOND;
    state.key_duration[key] += duration;
    state.key_start_time[key] = 0;
    state.key_occurances[key]++;

    return duration;
}

const char* profile_key_str(ProfileKey key) {
    switch (key) {
        case PROFILE_KEY_UPDATE: 
            return "UPDATE";
        case PROFILE_KEY_PATHFIND: 
            return "PATHFIND";
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
        case PROFILE_KEY_BOT_BUILD_CANCEL: 
            return "BOT BUILD CANCEL";
        case PROFILE_KEY_BOT_REPAIR: 
            return "BOT REPAIR";
        case PROFILE_KEY_BOT_REIN: 
            return "BOT REIN";
        case PROFILE_KEY_BOT_RALLY: 
            return "BOT RALLY";
        case PROFILE_KEY_BOT_UNLOAD: 
            return "BOT UNLOAD";
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

void profile_print_log() {
    char message[2048];
    char* message_ptr = message;
    message_ptr += sprintf(message_ptr, "PROFILE: ");
    for (int key = 0; key < PROFILE_KEY_COUNT; key++) {
        message_ptr += sprintf(message_ptr, "%s:%0.0f%% ", profile_key_str((ProfileKey)key), profile_key_percentage((ProfileKey)key));
    }
    log_trace("%s", message);
}

#else

void profile_begin_frame() {}
void profile_begin(ProfileKey key) {}
double profile_end(ProfileKey key) { return 0.0; } 
const char* profile_key_str(ProfileKey key) { return ""; }
double profile_key_duration(ProfileKey key) { return 0.0; }
double profile_key_percentage(ProfileKey key) { return 0.0; }
void profile_print_log() {}

#endif