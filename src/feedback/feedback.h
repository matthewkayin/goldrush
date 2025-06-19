#pragma once

void feedback_init(bool show_welcome);
void feedback_quit();

bool feedback_is_open();
void feedback_set_replay_path(const char* path);
void feedback_update();
void feedback_render();