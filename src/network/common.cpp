#include "common.h"

#include <steam/steam_api.h>

struct NetworkCommonState {
    char username[NETWORK_PLAYER_NAME_BUFFER_SIZE];
};
static NetworkCommonState state;

void network_common_init() {
    strncpy(state.username, SteamFriends()->GetPersonaName(), NETWORK_PLAYER_NAME_BUFFER_SIZE);
}

const char* network_get_username() {
    return state.username;
}