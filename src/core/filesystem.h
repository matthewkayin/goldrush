#pragma once

void filesystem_get_resource_path(char* buffer, const char* subpath);
void filesystem_get_data_path(char* buffer, const char* subpath);
void filesystem_get_replay_path(char* buffer, const char* subpath);
void filesystem_create_data_folder(const char* name);