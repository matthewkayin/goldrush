#include "network.h"

#include "defines.h"
#include "logger.h"
#include "asserts.h"
#include <enet/enet.h>
#include <cstdint>
#include <string>

static const uint16_t PORT = 9378;

// STATE

struct state_t {
    ENetHost* host;
    ENetPeer* peer;

    NetworkStatus status;
    char client_username[MAX_USERNAME_LENGTH + 1];

    uint8_t player_id;
    player_t players[NETWORK_MAX_PLAYERS];
};
static state_t state;

// MESSAGE

enum MessageType {
    MESSAGE_GREET,
    MESSAGE_GREET_RESPONSE,
    MESSAGE_PLAYERLIST
};

struct message_greet_t {
    const uint8_t type = MESSAGE_GREET;
    char username[MAX_USERNAME_LENGTH + 1];
    char app_version[sizeof(APP_VERSION)];
};

enum GreetResponseStatus {
    GREET_RESPONSE_ACCEPTED,
    GREET_RESPONSE_LOBBY_FULL,
    GREET_RESPONSE_INVALID_VERSION
};

struct message_greet_response_t {
    const uint8_t type = MESSAGE_GREET_RESPONSE;
    uint8_t player_id;
    uint8_t status;
};

struct message_playerlist_t {
    const uint8_t type = MESSAGE_PLAYERLIST;
    player_t players[NETWORK_MAX_PLAYERS];
};

char* message_data_to_string(uint8_t* data, size_t length) {
    char* str = (char*)malloc(((3 * length) + 1) * sizeof(char));
    char* str_ptr = str;
    for (size_t i = 0; i < length; i++) {
        sprintf(str_ptr, " %02x", data[i]);
        str_ptr += 3;
    }
    str_ptr[0] = '\0';
    return str;
}

template <typename utype>
ENetPacket* message_serialize(utype data) {
    size_t message_length = sizeof(utype);
    uint8_t* message_data = (uint8_t*)malloc(message_length);
    memcpy(message_data, &data, sizeof(utype));
    ENetPacket* packet = enet_packet_create(message_data, message_length, ENET_PACKET_FLAG_RELIABLE);

    char* data_str = message_data_to_string(message_data, message_length);
    log_info("Sending message: %s/%u", data_str, message_length);
    free(data_str);

    free(message_data);
    return packet;
}

template <typename utype>
void message_send(ENetPeer* peer, utype data) {
    ENetPacket* packet = message_serialize<utype>(data);
    enet_peer_send(peer, 0, packet);
}

template <typename utype>
void message_broadcast(utype data) {
    ENetPacket* packet = message_serialize<utype>(data);
    enet_host_broadcast(state.host, 0, packet);
}

void server_broadcast_playerlist() {
    message_playerlist_t playerlist;
    memcpy(playerlist.players, state.players, sizeof(state.players));
    message_broadcast<message_playerlist_t>(playerlist);
}

void client_handle_message(uint8_t* data, size_t length) {
    uint8_t message_type = data[0];
    log_info("Received message with type %u from server", message_type);

    switch (message_type) {
        case MESSAGE_GREET_RESPONSE: {
            message_greet_response_t greet_response;
            memcpy(&greet_response, data, sizeof(message_greet_response_t));

            state.status = NETWORK_STATUS_CONNECTED;
            log_info("setting status to connected.")
            if (greet_response.status == GREET_RESPONSE_ACCEPTED) {
                log_info("server accepted client");
                state.player_id = greet_response.player_id;
            } else {
                log_info("server rejected client");
                network_disconnect();
            }
            break;
        }
        case MESSAGE_PLAYERLIST: {
            // Data is copied from data + 1 because we're copying straight into players so we need to offset by the type
            memcpy(state.players, data + 1, sizeof(state.players));
            break;
        }
        default:
            break;
    }
}

void server_handle_message(uint8_t* data, size_t lenght, uint8_t player_id) {
    uint8_t message_type = data[0];
    log_info("Received message with type %u from player %u", message_type, player_id);

    if (message_type == MESSAGE_GREET) {
        message_greet_t greet;
        memcpy(&greet, data, sizeof(message_greet_t));

        message_greet_response_t response;
        response.player_id = player_id;
        if (strcmp(greet.app_version, APP_VERSION) != 0) {
            log_info("Client app version mismatch. Rejecting client...");
            response.status = GREET_RESPONSE_INVALID_VERSION;
            return;
        } else {
            strncpy(state.players[player_id].name, greet.username, MAX_USERNAME_LENGTH + 1);
            state.players[player_id].status = PLAYER_STATUS_NOT_READY;
            log_info("Client is now player %u", player_id);
            for (uint8_t i = 0; i < state.host->peerCount; i++) {
                log_info("player_id %u. status: %u connected: %i", i + 1, (uint8_t)state.players[i + 1].status, (int)(state.host->peers[i].state == ENET_PEER_STATE_CONNECTED));
            }

            response.status = GREET_RESPONSE_ACCEPTED;
        }

        message_send<message_greet_response_t>(&state.host->peers[player_id - 1], response);
        if (response.status == GREET_RESPONSE_ACCEPTED) {
            server_broadcast_playerlist();
        }
        enet_host_flush(state.host);
    }
}

// GENERAL

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
    switch (state.status) {
        case NETWORK_STATUS_SERVER: {
            for (uint8_t peer_id = 0; peer_id < state.host->peerCount; peer_id++) {
                if (state.host->peers[peer_id].state == ENET_PEER_STATE_CONNECTED) {
                    enet_peer_disconnect(&state.host->peers[peer_id], 0);
                }
                enet_host_flush(state.host);
            }
            enet_host_destroy(state.host);
            state.host = NULL;
            state.status = NETWORK_STATUS_OFFLINE;
            break;
        }
        case NETWORK_STATUS_CONNECTED: {
            log_info("Client attempting gentle disconnect...");
            enet_peer_disconnect(state.peer, 0);
            state.status = NETWORK_STATUS_DISCONNECTING;
            break;
        }
        case NETWORK_STATUS_CONNECTING:
        case NETWORK_STATUS_DISCONNECTING: {
            enet_peer_reset(state.peer);
            enet_host_destroy(state.host);
            state.host = NULL;
            state.peer = NULL;
            log_info("Client hard disconnected.");
            state.status = NETWORK_STATUS_OFFLINE;
            break;
        }
        default:
            break;
    }
}

// ACCESSOR

bool network_is_server() {
    return state.status == NETWORK_STATUS_SERVER;
}

NetworkStatus network_get_status() {
    return state.status;
}

const player_t& network_get_player(uint8_t player_id) {
    return state.players[player_id];
}

// POLL EVENTS

void network_poll_events() {
    ENetEvent event; 
    while (state.status != NETWORK_STATUS_OFFLINE && enet_host_service(state.host, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                if (state.status == NETWORK_STATUS_CONNECTING) {
                    // On server connected to client
                    log_info("connected to server. Sending greeting...");
                    message_greet_t greet;
                    strncpy(greet.username, state.client_username, MAX_USERNAME_LENGTH + 1);
                    strncpy(greet.app_version, APP_VERSION, sizeof(APP_VERSION));
                    message_send<message_greet_t>(state.peer, greet);
                    enet_host_flush(state.host);
                }
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT: {
                if (state.status == NETWORK_STATUS_SERVER) {
                    // On client disconnected
                    log_info("player %u disconnected.", event.peer->incomingPeerID + 1);
                    state.players[event.peer->incomingPeerID + 1].status = PLAYER_STATUS_NONE;
                    for (uint8_t i = 0; i < state.host->peerCount; i++) {
                        log_info("player_id %u. status: %u connected: %i", i + 1, (uint8_t)state.players[i + 1].status, (int)(state.host->peers[i].state == ENET_PEER_STATE_CONNECTED));
                    }
                    server_broadcast_playerlist();
                } else if (state.status == NETWORK_STATUS_DISCONNECTING) {
                    enet_host_destroy(state.host);
                    state.host = NULL;
                    state.peer = NULL;
                    state.status = NETWORK_STATUS_OFFLINE;
                } else {
                    // On server disconnected
                    log_info("server disconnected.");
                    enet_peer_reset(state.peer);
                    enet_host_destroy(state.host);
                    state.host = NULL;
                    state.peer = NULL;
                    state.status = NETWORK_STATUS_OFFLINE;
                    return;
                }
                break;
            }
            case ENET_EVENT_TYPE_RECEIVE: {
                char* data_str = message_data_to_string(event.packet->data, event.packet->dataLength);
                log_info("received message %s/%u", data_str, event.packet->dataLength);
                free(data_str);

                // Implicit: don't handle / throw away any events if status CONNECTING or DISCONNECTING
                if (state.status == NETWORK_STATUS_SERVER) {
                    server_handle_message(event.packet->data, event.packet->dataLength, event.peer->incomingPeerID + 1);
                } else if (state.status == NETWORK_STATUS_CONNECTED || state.status == NETWORK_STATUS_CONNECTING) {
                    client_handle_message(event.packet->data, event.packet->dataLength);
                }
                enet_packet_destroy(event.packet);
            }
            default: 
                break;
        }
    }
}

// SERVER

bool network_server_create(const char* username) {
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = PORT;

    char ip_test[17];
    enet_address_get_host_ip(&address, ip_test, 17);
    log_info("You IP is %s", ip_test);

    state.host = enet_host_create(&address, NETWORK_MAX_PLAYERS - 1, 2, 0, 0);
    if (state.host == NULL) {
        log_error("Could not create enet host.");
        return false;
    }

    log_info("Created server.");
    state.status = NETWORK_STATUS_SERVER;

    memset(state.players, 0, sizeof(state.players));
    state.player_id = 0;
    strncpy_s(state.players[state.player_id].name, username, 17);
    state.players[state.player_id].status = PLAYER_STATUS_HOST;

    for (uint8_t i = 0; i < state.host->peerCount; i++) {
        log_info("player_id %u. connected? %i", i + 1, (int)(state.host->peers[i].state == ENET_PEER_STATE_CONNECTED));
    }

    return true;
}

// CLIENT

bool network_client_create(const char* username, const char* server_ip) {
    state.host = enet_host_create(NULL, 1, 2, 0, 0);
    if (state.host == NULL) {
        log_error("Couldnot create enet host.");
        return false;
    }

    ENetAddress address;
    enet_address_set_host(&address, server_ip);
    address.port = PORT;

    state.peer = enet_host_connect(state.host, &address, 2, 0);
    if (state.peer == NULL) {
        enet_host_destroy(state.host);
        log_error("No peers available for initiating an enet connection.");
        return false;
    }

    memset(state.players, 0, sizeof(state.players));
    strncpy(state.client_username, username, MAX_USERNAME_LENGTH + 1);
    state.status = NETWORK_STATUS_CONNECTING;

    return true;
}