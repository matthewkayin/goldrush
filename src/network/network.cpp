#include "network.h"

#include "lan.h"
#include "steam.h"
#include "core/logger.h"
#include "core/asserts.h"

struct NetworkState {
    NetworkBackend backend;
};
static NetworkState state;

bool network_init() {
    network_common_init();
    if (!network_lan_init()) {
        return false;
    }
    network_steam_init();
    state.backend = NETWORK_BACKEND_NONE;
}

void network_quit() {
    network_lan_quit();
    log_info("Quit network.");
}

void network_set_backend(NetworkBackend backend) {
    if (network_get_status() != NETWORK_STATUS_OFFLINE) {
        log_error("Can't backends when we are not fully offline.", state.backend, backend);
        network_disconnect();
        return;
    }
    state.backend = backend;
}

void network_service() {
    if (state.backend == NETWORK_BACKEND_LAN) {
        network_lan_service();
    } else if (state.backend == NETWORK_BACKEND_STEAM) {
        network_steam_service();
    }
}
void network_disconnect();

void network_search_lobbies(const char* query) {
    if (state.backend == NETWORK_BACKEND_LAN) {
        return network_lan_search_lobbies(query);
    } else {
        return network_steam_search_lobbies(query);
    }
}

uint32_t network_get_lobby_count() {
    GOLD_ASSERT(state.backend != NETWORK_BACKEND_NONE);
    if (state.backend == NETWORK_BACKEND_LAN) {
        return network_lan_get_lobby_count();
    } else {
        return network_steam_get_lobby_count();
    }
}

const NetworkLobby& network_get_lobby(uint32_t index) {
    GOLD_ASSERT(state.backend != NETWORK_BACKEND_NONE);
    if (state.backend == NETWORK_BACKEND_LAN) {
        return network_lan_get_lobby(index);
    } else {
        return network_steam_get_lobby(index);
    }
}

void network_create_lobby(const char* lobby_name) {
    if (state.backend == NETWORK_BACKEND_LAN) {
        network_lan_create_lobby(lobby_name);
    } else if (state.backend == NETWORK_BACKEND_STEAM) {
        network_steam_create_lobby(lobby_name);
    }
}

void network_join_lobby(uint32_t index) {
    if (state.backend == NETWORK_BACKEND_LAN) {
        network_lan_join_lobby(index);
    } else if (state.backend == NETWORK_BACKEND_STEAM) {
        network_steam_join_lobby(index);
    }
}

NetworkStatus network_get_status() {
    if (state.backend == NETWORK_BACKEND_LAN) {
        return network_lan_get_status();
    } else if (state.backend == NETWORK_BACKEND_STEAM) {
        return network_steam_get_status();
    } else {
        return NETWORK_STATUS_OFFLINE;
    }
}

bool network_is_host() {
    return network_get_status() == NETWORK_STATUS_HOST;
}

const NetworkPlayer& network_get_player(uint8_t player_id);
uint8_t network_get_player_id();
const char* network_get_lobby_name();