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
    char username[MAX_USERNAME_LENGTH + 1];
    char app_version[sizeof(APP_VERSION)];
};

enum GreetResponseStatus {
    GREET_RESPONSE_ACCEPTED,
    GREET_RESPONSE_LOBBY_FULL,
    GREET_RESPONSE_INVALID_VERSION
};

struct message_greet_response_t {
    uint8_t player_id;
    uint8_t status;
};

struct message_playerlist_t {
    player_t players[NETWORK_MAX_PLAYERS];
};

struct message_t {
    uint8_t type;
    union {
        message_greet_t greet;
        message_greet_response_t greet_response;
        message_playerlist_t playerlist;
    };
};

struct message_data_t {
    uint8_t* data;
    size_t length;
};

char* message_data_to_string(const message_data_t& data) {
    char* str = (char*)malloc(((3 * data.length) + 1) * sizeof(char));
    char* str_ptr = str;
    for (size_t i = 0; i < data.length; i++) {
        sprintf(str_ptr, " %02x", data.data[i]);
        str_ptr += 3;
    }
    str_ptr[0] = '\0';
    return str;
}

message_data_t message_serialize(const message_t& message) {
    message_data_t result;
    result.length = 1;

    switch (message.type) {
        case MESSAGE_GREET: {
            result.length += sizeof(message_greet_t);
            result.data = (uint8_t*)malloc(result.length);
            result.data[0] = message.type;
            memcpy(result.data + 1, &message.greet, sizeof(message_greet_t));
            break;
        }
        case MESSAGE_GREET_RESPONSE: {
            result.length += sizeof(message_greet_response_t);
            result.data = (uint8_t*)malloc(result.length);
            result.data[0] = message.type;
            memcpy(result.data + 1, &message.greet_response, sizeof(message_greet_response_t));
            break;
        }
        case MESSAGE_PLAYERLIST: {
            result.length += sizeof(message_playerlist_t);
            result.data = (uint8_t*)malloc(result.length);
            result.data[0] = message.type;
            memcpy(result.data + 1, &message.playerlist, sizeof(message_playerlist_t));
            break;
        }
        default:
            log_error("Tried to serialize message with unhandled type %u", message.type);
            GOLD_ASSERT(false);
            break;
    }

    return result;
}

message_t message_deserialize(const message_data_t& data) {
    message_t message;
    message.type = data.data[0];

    switch (message.type) {
        case MESSAGE_GREET: 
            memcpy(&message.greet, data.data + 1, sizeof(message_greet_t));
            break;
        case MESSAGE_GREET_RESPONSE:
            memcpy(&message.greet_response, data.data + 1, sizeof(message_greet_response_t));
            break;
        case MESSAGE_PLAYERLIST:
            memcpy(&message.playerlist, data.data + 1, sizeof(message_playerlist_t));
            break;
        default:
            log_error("Tried to deserialize message with unhandled type %u", message.type);
            GOLD_ASSERT(false);
            break;
    }

    return message;
}

void message_send(ENetPeer* peer, const message_t& message) {
    message_data_t data = message_serialize(message);
    char* data_str = message_data_to_string(data);
    log_info("Sending message: %s/%u", data_str, data.length);
    free(data_str);
    ENetPacket* packet = enet_packet_create(data.data, data.length, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, packet);
}

void message_broadcast(const message_t& message) {
    message_data_t data = message_serialize(message);
    ENetPacket* packet = enet_packet_create(data.data, data.length, ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(state.host, 0, packet);
}

void server_broadcast_playerlist() {
    message_t message;
    message.type = MESSAGE_PLAYERLIST;
    memcpy(message.playerlist.players, state.players, sizeof(state.players));
    message_broadcast(message);
}

void client_handle_message(const message_t& message) {
    log_info("Received message with type %u from server", message.type);

    switch (message.type) {
        case MESSAGE_GREET_RESPONSE: {
            state.status = NETWORK_STATUS_CONNECTED;
            log_info("setting status to connected.")
            if (message.greet_response.status == GREET_RESPONSE_ACCEPTED) {
                log_info("server accepted client");
                state.player_id = message.greet_response.player_id;
            } else {
                log_info("server rejected client");
                network_disconnect();
            }
            break;
        }
        case MESSAGE_PLAYERLIST: {
            memcpy(state.players, message.playerlist.players, sizeof(state.players));
            break;
        }
        default:
            break;
    }
}

void server_handle_message(const message_t& message, uint8_t player_id) {
    log_info("Received message with type %u from player %u", message.type, player_id);

    if (message.type == MESSAGE_GREET) {
        message_t response;
        response.type = MESSAGE_GREET_RESPONSE;
        response.greet_response.player_id = player_id;
        if (strcmp(message.greet.app_version, APP_VERSION) != 0) {
            log_info("Client app version mismatch. Rejecting client...");
            response.greet_response.status = GREET_RESPONSE_INVALID_VERSION;
            return;
        } else {
            strncpy(state.players[player_id].name, message.greet.username, MAX_USERNAME_LENGTH + 1);
            state.players[player_id].status = PLAYER_STATUS_NOT_READY;
            log_info("Client is now player %u", player_id);
            for (uint8_t i = 0; i < state.host->peerCount; i++) {
                log_info("player_id %u. status: %u connected: %i", i + 1, (uint8_t)state.players[i + 1].status, (int)(state.host->peers[i].state == ENET_PEER_STATE_CONNECTED));
            }

            response.greet_response.status = GREET_RESPONSE_ACCEPTED;
        }

        message_send(&state.host->peers[player_id - 1], response);
        if (response.greet_response.status == GREET_RESPONSE_ACCEPTED) {
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
                    message_t message;
                    message.type = MESSAGE_GREET;
                    strncpy(message.greet.username, state.client_username, MAX_USERNAME_LENGTH + 1);
                    strncpy(message.greet.app_version, APP_VERSION, sizeof(APP_VERSION));
                    message_send(state.peer, message);
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
                // Implicit: don't handle / throw away any events if status CONNECTING or DISCONNECTING
                message_data_t data = (message_data_t) {
                    .data = event.packet->data,
                    .length = event.packet->dataLength
                };
                char* data_str = message_data_to_string(data);
                log_info("received message %s/%u", data_str, data.length);
                free(data_str);
                message_t message = message_deserialize(data);
                if (state.status == NETWORK_STATUS_SERVER) {
                    server_handle_message(message, event.peer->incomingPeerID + 1);
                } else if (state.status == NETWORK_STATUS_CONNECTED || state.status == NETWORK_STATUS_CONNECTING) {
                    client_handle_message(message);
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