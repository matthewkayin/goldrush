#pragma once

#include "defines.h"

#define OBJECTIVE_TITLE_BUFFER_LENGTH 32

struct Objective {
    bool is_finished;
    char description[OBJECTIVE_TITLE_BUFFER_LENGTH];
};