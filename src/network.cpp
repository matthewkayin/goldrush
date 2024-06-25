#include "network.h"

#include "logger.h"
#include <enet/enet.h>
#include <cstdint>

static const uint16_t PORT = 9378;
static const size_t MAX_PLAYERS = 8;

struct state_t {
    ENetHost* host;
    ENetPeer* peer;
    bool is_server;
};
static state_t state;

bool network_init() {
    if (enet_initialize() != 0) {
        log_error("Unable to initialize enet.");
        return false;
    }

    state.is_server = false;

    return true;
}

void network_quit() {
    enet_deinitialize();
}

void network_disconnect() {
    state.is_server = false;
    enet_host_destroy(state.host);
}

bool network_is_server() {
    return state.is_server;
}

// SERVER

bool network_server_create() {
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = PORT;

    state.host = enet_host_create(&address, MAX_PLAYERS - 1, 2, 0, 0);
    if (state.host == nullptr) {
        log_error("Could not create enet host.");
        return false;
    }

    log_info("Created server.");
    state.is_server = true;
    return true;
}

void network_server_get_ip(char* ip_buffer) {
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = PORT;
    enet_address_get_host_ip(&address, ip_buffer, 17);
}

// CLIENT

bool network_client_create(const char* server_ip) {
    state.host = enet_host_create(NULL, 1, 2, 0, 0);
    if (state.host == nullptr) {
        log_error("Couldnot create enet host.");
        return false;
    }

    ENetAddress address;
    enet_address_set_host(&address, server_ip);
    address.port = PORT;

    state.peer = enet_host_connect(state.host, &address, 2, 0);
    if (state.peer == nullptr) {
        log_error("No peers available for initiating an enet connection.");
        return false;
    }

    return true;
}