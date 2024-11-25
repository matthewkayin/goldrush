#include "network.h"

#include "logger.h"
#include <enet/enet.h>

static const uint16_t PORT = 6530;

struct player_t {
    PlayerStatus status;
    char name[24];
    char ip[24];
};

bool network_init();

struct network_state_t {
    ENetHost* host;
    player_t players[MAX_PLAYERS];
    NetworkStatus status;
    uint8_t player_id;
    char client_username[24];
};
static network_state_t state;

// MESSAGE

enum MessageType {
    MESSAGE_GREET,
    MESSAGE_GREET_RESPONSE_INVALID_VERSION,
    MESSAGE_GREET_RESPONSE_ACCEPTED,
    MESSAGE_NEW_PLAYER,
    MESSAGE_READY
};

struct message_greet_t {
    const uint8_t type = MESSAGE_GREET;
    char username[24];
    char app_version[sizeof(APP_VERSION)];
};

struct message_greet_response_accepted_t {
    const uint8_t type = MESSAGE_GREET_RESPONSE_ACCEPTED;
    uint8_t player_id;
    uint8_t padding[3];
    char server_username[24];
};

struct message_new_player_t {
    const uint8_t type = MESSAGE_NEW_PLAYER;
    uint8_t player_id;
    uint8_t status;
    uint8_t padding[1];
    char name[24];
    char ip[24];
};

bool network_init() {
    if (enet_initialize() != 0) {
        log_error("Unable to initialize enet.");
        return false;
    }

    state.status = NETWORK_STATUS_OFFLINE;

    return true;
}

void network_quit() {
    enet_deinitialize();
}

void network_disconnect() {

}

bool network_is_server() {
    return state.status == NETWORK_STATUS_SERVER;
}

NetworkStatus network_get_status() {
    return state.status;
}

PlayerStatus network_get_player_status(uint8_t player_id) {
    return state.players[player_id].status;
}

const char* network_get_player_name(uint8_t player_id) {
    return state.players[player_id].name;
}

// POLL EVENTS

void network_service() {
    ENetEvent event;
    while (state.status != NETWORK_STATUS_OFFLINE && enet_host_service(state.host, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                if (state.status == NETWORK_STATUS_CONNECTING) {
                    // On client connected to server
                    log_info("Connected to server. Sending greeting...");
                    message_greet_t greet;
                    strncpy(greet.username, state.client_username, MAX_USERNAME_LENGTH + 1);
                    strncpy(greet.app_version, APP_VERSION, sizeof(APP_VERSION));
                    ENetPacket* packet = enet_packet_create(&greet, sizeof(message_greet_t), ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(&state.host->peers[0], 0, packet);
                    enet_host_flush(state.host);
                }
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT: {
                break;
            }
            case ENET_EVENT_TYPE_RECEIVE: {
                log_info("Received message %b", event.packet->data, event.packet->dataLength);
                network_handle_message(event.packet->data, event.packet->dataLength, event.peer->incomingPeerID);
                enet_packet_destroy(event.packet);
                break;
            }
        }
    }
}

void network_handle_message(uint8_t* data, size_t length, uint16_t peer_id) {
    uint8_t message_type = data[0];

    switch (message_type) {
        case MESSAGE_GREET: {
            message_greet_t greet;
            memcpy(&greet, data, sizeof(message_greet_t));

            // I think that ENET will handle the lobby-full case
            message_greet_response_t response;
            uint8_t player_id;
            for (player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                if (state.players[player_id].status == PLAYER_STATUS_NONE) {
                    break;
                }
            }
            response.player_id = player_id;
            if (strcmp(greet.app_version, APP_VERSION) != 0) {
                log_info("Client app version mismatch. Rejecting client...");
                response.status = GREET_RESPONSE_INVALID_VERSION;
            } else {
                strncpy(state.players[player_id].name, greet.username, MAX_USERNAME_LENGTH + 1);
                state.players[player_id].status = PLAYER_STATUS_NOT_READY;
                log_info("Client is now player %u", player_id);

                response.status = GREET_RESPONSE_ACCEPTED;
            }

            ENetPacket* packet = enet_packet_create(&response, sizeof(message_greet_response_t), ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(&state.host->peers[peer_id], 0, packet);
            state.host->peers[peer_id].address;
            if (response.status == GREET_RESPONSE_ACCEPTED) {

            }
            enet_host_flush(state.host);
            break;
        }
    }
}

// SERVER

bool network_server_create(const char* username) {
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = PORT;

    state.host = enet_host_create(&address, MAX_PLAYERS - 1, 1, 0, 0);
    if (state.host == NULL) {
        log_error("Could not create enet host.");
        return false;
    }

    log_info("Created server.");
    state.status = NETWORK_STATUS_SERVER;

    state.player_id = 0;
    memset(state.players, 0, sizeof(state.players));
    strncpy(state.players[0].name, username, MAX_USERNAME_LENGTH + 1);
    state.players[0].status = PLAYER_STATUS_HOST;

    return true;
}

// CLIENT

bool network_client_create(const char* username, const char* server_ip) {
    state.host = enet_host_create(NULL, MAX_PLAYERS - 1, 1, 0, 0);
    if (state.host == NULL) {
        log_error("Could not create enet host.");
        return false;
    }

    ENetAddress address;
    enet_address_set_host(&address, server_ip);
    address.port = PORT;

    ENetPeer* server_peer = enet_host_connect(state.host, &address, 1, 0);
    if (server_peer == NULL) {
        enet_host_destroy(state.host);
        log_error("No peers available for initiating an enet connection.");
        return false;
    }

    memset(state.players, 0, sizeof(state.players));
    strncpy(state.client_username, username, MAX_USERNAME_LENGTH + 1);
    state.status = NETWORK_STATUS_CONNECTING;
    
    return true;
}