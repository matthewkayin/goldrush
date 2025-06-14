#pragma once

void feedback_init();
void feedback_quit();

bool feedback_is_open();
void feedback_set_replay_path(const char* path);
void feedback_update();
void feedback_render();