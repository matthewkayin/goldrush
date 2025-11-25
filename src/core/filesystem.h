#pragma once

#include <string>

#define FILESYSTEM_REPLAY_FOLDER_NAME "replays/"
#define FILESYSTEM_LOG_FOLDER_NAME "logs/"

std::string filesystem_get_timestamp_str();
std::string filesystem_get_data_path();
std::string filesystem_get_resource_path();
void filesystem_create_required_folders();