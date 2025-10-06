#pragma once

enum ProfileKey {
    PROFILE_KEY_RENDER,
    PROFILE_KEY_UPDATE,
    PROFILE_KEY_BOT,
    PROFILE_KEY_BOT_STRATEGY,
    PROFILE_KEY_BOT_PRODUCTION,
    PROFILE_KEY_BOT_SQUAD,
    PROFILE_KEY_BOT_MISC,
    PROFILE_KEY_COUNT
};

void profile_begin_frame();
void profile_begin(ProfileKey key);
void profile_end(ProfileKey key);
void profile_end_frame();
const char* profile_key_str(ProfileKey key);
double profile_key_duration(ProfileKey key);