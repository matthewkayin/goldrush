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

    return true;
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

void network_disconnect() {
    if (state.backend == NETWORK_BACKEND_LAN) {
        network_lan_disconnect();
    } else if (state.backend == NETWORK_BACKEND_STEAM) {
        network_steam_disconnect();
    }
}

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

const NetworkPlayer& network_get_player(uint8_t player_id) {
    GOLD_ASSERT(state.backend != NETWORK_BACKEND_NONE);
    if (state.backend == NETWORK_BACKEND_LAN) {
        return network_lan_get_player(player_id);
    } else {
        return network_steam_get_player(player_id);
    }
}

uint8_t network_get_player_id() {
    GOLD_ASSERT(state.backend != NETWORK_BACKEND_NONE);
    if (state.backend == NETWORK_BACKEND_LAN) {
        return network_lan_get_player_id();
    } else {
        return network_steam_get_player_id();
    }
}

const char* network_get_lobby_name() {
    GOLD_ASSERT(state.backend != NETWORK_BACKEND_NONE);
    if (state.backend == NETWORK_BACKEND_LAN) {
        return network_lan_get_lobby_name();
    } else {
        return network_steam_get_lobby_name();
    }
}

void network_send_chat(const char* message) {
    if (state.backend == NETWORK_BACKEND_LAN) {
        network_lan_send_chat(message);
    } else if (state.backend == NETWORK_BACKEND_STEAM) {
        network_steam_send_chat(message);
    }
}

void network_set_player_ready(bool ready) {
    if (state.backend == NETWORK_BACKEND_LAN) {
        network_lan_set_player_ready(ready);
    } else if (state.backend == NETWORK_BACKEND_STEAM) {
        network_steam_set_player_ready(ready);
    }
}

void network_set_player_color(uint8_t color) {
    if (state.backend == NETWORK_BACKEND_LAN) {
        network_lan_set_player_color(color);
    } else if (state.backend == NETWORK_BACKEND_STEAM) {
        network_steam_set_player_color(color);
    }
}

void network_set_match_setting(uint8_t setting, uint8_t value) {
    if (state.backend == NETWORK_BACKEND_LAN) {
        network_lan_set_match_setting(setting, value);
    } else if (state.backend == NETWORK_BACKEND_STEAM) {
        network_steam_set_match_setting(setting, value);
    }
}

uint8_t network_get_match_setting(uint8_t setting) {
    GOLD_ASSERT(state.backend != NETWORK_BACKEND_NONE);
    if (state.backend == NETWORK_BACKEND_LAN) {
        return network_lan_get_match_setting(setting);
    } else {
        return network_steam_get_match_setting(setting);
    }
}

void network_set_player_team(uint8_t team) {
    if (state.backend == NETWORK_BACKEND_LAN) {
        network_lan_set_player_team(team);
    } else if (state.backend == NETWORK_BACKEND_STEAM) {
        network_steam_set_player_team(team);
    }
}

void network_begin_loading_match(int32_t lcg_seed, const Noise& noise) {
    if (state.backend == NETWORK_BACKEND_LAN) {
        network_lan_begin_loading_match(lcg_seed, noise);
    } else {
        network_steam_begin_loading_match(lcg_seed, noise);
    }
}

void network_send_input(uint8_t* out_buffer, size_t out_buffer_length) {
    if (state.backend == NETWORK_BACKEND_LAN) {
        network_lan_send_input(out_buffer, out_buffer_length);
    } else {
        network_steam_send_input(out_buffer, out_buffer_length);
    }
}