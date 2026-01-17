#pragma once

#include "defines.h"
#include "render/font.h"
#include "network/types.h"

#define SHELL_CHAT_PREFIX_BUFFER_SIZE (MAX_USERNAME_LENGTH + 4)
#define SHELL_CHAT_MESSAGE_BUFFER_SIZE NETWORK_CHAT_BUFFER_SIZE

struct ChatMessage {
    FontName prefix_font;
    uint32_t timer;
    char prefix[SHELL_CHAT_PREFIX_BUFFER_SIZE];
    char message[SHELL_CHAT_MESSAGE_BUFFER_SIZE];
};