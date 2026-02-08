#pragma once

#include "defines.h"

// Using Tracy 0.12.2

#ifdef TRACY_ENABLE

// Beetlejuice, beetlejuice, beetlejuice!
#include <tracy/tracy/Tracy.hpp>
#define GOLD_PROFILE_FRAME_MARK FrameMark
#define GOLD_PROFILE_SCOPE ZoneScoped
#define GOLD_PROFILE_SCOPE_NAME ZoneScopedN

#else

#define GOLD_PROFILE_FRAME_MARK
#define GOLD_PROFILE_SCOPE
#define GOLD_PROFILE_SCOPE_NAME 

#endif