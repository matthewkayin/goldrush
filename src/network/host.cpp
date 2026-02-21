#include "host.h"

bool INetworkHost::poll_events(NetworkHostEvent* event) {
    if (host_events.empty()) {
        return false;
    }

    *event = host_events.front();
    host_events.pop();

    return true;
}

uint8_t INetworkHost::get_player_count() const {
    uint8_t player_count = 0;
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (host_players[player_id].status != NETWORK_PLAYER_STATUS_NONE) {
            player_count++;
        }
    }

    return player_count;
}