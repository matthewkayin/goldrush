#include "network.h"

#include "defines.h"
#include "logger.h"
#include "asserts.h"
#include "lcg.h"
#include <enet/enet.h>
#include <cstdint>
#include <string>
#include <ctime>

static const uint16_t PORT = 9378;
static const uint32_t EVENT_QUEUE_SIZE = 16;

// STATE

struct state_t {
    ENetHost* host;
    ENetPeer* peer;

    NetworkStatus status;
    char client_username[MAX_USERNAME_LENGTH + 1];

    uint8_t player_id;
    player_t players[MAX_PLAYERS];

    network_event_t event_queue[EVENT_QUEUE_SIZE];
    uint32_t event_queue_count = 0;
    uint32_t event_queue_head = 0;
};
static state_t state;

// MESSAGE

enum MessageType {
    MESSAGE_GREET,
    MESSAGE_GREET_RESPONSE,
    MESSAGE_PLAYERLIST,
    MESSAGE_READY,
    MESSAGE_MATCH_LOAD,
    MESSAGE_MATCH_START,
    MESSAGE_INPUT
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
    uint8_t padding[3];
    player_t players[MAX_PLAYERS];
};

struct message_ready_t {
    const uint8_t type = MESSAGE_READY;
};

struct message_match_load_t {
    const uint8_t type = MESSAGE_MATCH_LOAD;
    uint8_t padding[3];
    uint32_t random_seed;
};

struct message_match_start_t {
    const uint8_t type = MESSAGE_MATCH_START;
};

void network_event_enqueue(const network_event_t& event);
void network_event_input_message_enqueue(uint8_t* data, size_t length);
void server_handle_message(uint8_t* data, size_t length, uint8_t player_id);
void client_handle_message(uint8_t* data, size_t length);

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

uint8_t network_get_player_id() {
    return state.player_id;
}

const player_t& network_get_player(uint8_t player_id) {
    return state.players[player_id];
}

bool network_are_all_players_ready() {
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (state.players[player_id].status == PLAYER_STATUS_NOT_READY) {
            return false;
        }
    }

    return true;
}

// POLL EVENTS

void network_service() {
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
                    ENetPacket* packet = enet_packet_create(&greet, sizeof(message_greet_t), ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(state.peer, 0, packet);
                    enet_host_flush(state.host);
                }
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT: {
                if (state.status == NETWORK_STATUS_SERVER) {
                    // On client disconnected
                    log_info("player %u disconnected.", event.peer->incomingPeerID + 1);
                    state.players[event.peer->incomingPeerID + 1].status = PLAYER_STATUS_NONE;
                    network_server_broadcast_playerlist();

                    network_event_t network_event;
                    network_event.type = NETWORK_EVENT_CLIENT_DISCONNECTED;
                    network_event_enqueue(network_event);
                } else if (state.status == NETWORK_STATUS_DISCONNECTING) {
                    enet_host_destroy(state.host);
                    state.host = NULL;
                    state.peer = NULL;
                    state.status = NETWORK_STATUS_OFFLINE;
                } else {
                    // On server disconnected
                    log_info("server disconnected.");

                    network_event_t event;
                    event.type = state.status == NETWORK_STATUS_CONNECTING ? NETWORK_EVENT_CONNECTION_FAILED : NETWORK_EVENT_SERVER_DISCONNECTED;
                    network_event_enqueue(event);

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
                log_info("received message %b", event.packet->data, event.packet->dataLength);

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

// EVENTS

bool network_poll_events(network_event_t* event) {
    if (state.event_queue_count == 0) {
        return false;
    }

    *event = state.event_queue[state.event_queue_head];
    state.event_queue_head = (state.event_queue_head + 1) % EVENT_QUEUE_SIZE;
    state.event_queue_count--;

    return true;
}

void network_event_enqueue(const network_event_t& event) {
    GOLD_ASSERT(state.event_queue_count != EVENT_QUEUE_SIZE);
    uint32_t index = (state.event_queue_head + state.event_queue_count) % EVENT_QUEUE_SIZE;
    state.event_queue[index] = event;
    state.event_queue_count++;
}

void network_event_input_message_enqueue(uint8_t* data, size_t length) {
    network_event_t network_event;
    network_event.type = NETWORK_EVENT_INPUT;
    network_event.input.data_length = length;
    memcpy(network_event.input.data, data, network_event.input.data_length);
    network_event_enqueue(network_event);
}

// SERVER

bool network_server_create(const char* username) {
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = PORT;

    state.host = enet_host_create(&address, MAX_PLAYERS - 1, 2, 0, 0);
    if (state.host == NULL) {
        log_error("Could not create enet host.");
        return false;
    }

    log_info("Created server.");
    state.status = NETWORK_STATUS_SERVER;

    memset(state.players, 0, sizeof(state.players));
    state.player_id = 0;
    strncpy(state.players[state.player_id].name, username, MAX_USERNAME_LENGTH + 1);
    state.players[state.player_id].status = PLAYER_STATUS_HOST;

    return true;
}

void server_handle_message(uint8_t* data, size_t length, uint8_t player_id) {
    uint8_t message_type = data[0];

    switch (message_type) {
        case MESSAGE_GREET: {
            message_greet_t greet;
            memcpy(&greet, data, sizeof(message_greet_t));

            message_greet_response_t response;
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
            enet_peer_send(&state.host->peers[player_id - 1], 0, packet);
            if (response.status == GREET_RESPONSE_ACCEPTED) {
                network_server_broadcast_playerlist();
            }
            enet_host_flush(state.host);
            break;
        }
        case MESSAGE_READY: {
            state.players[player_id].status = state.players[player_id].status == PLAYER_STATUS_NOT_READY ? PLAYER_STATUS_READY : PLAYER_STATUS_NOT_READY;
            network_event_t network_event;
            network_event.type = NETWORK_EVENT_CLIENT_READY;
            network_event_enqueue(network_event);
        }
        case MESSAGE_INPUT: {
            // Send the message to the game
            network_event_input_message_enqueue(data, length);

            // Relay the message to other clients, skipping player_id 0 because that is the server
            for (uint8_t relay_player_id = 1; relay_player_id < MAX_PLAYERS; relay_player_id++) {
                // Don't relay the message back to the sender
                if (relay_player_id == player_id) {
                    continue;
                } 

                ENetPacket* relay_packet = enet_packet_create(data, length, ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(&state.host->peers[relay_player_id - 1], 0, relay_packet);
            }
            enet_host_flush(state.host);

            break;
        }
        default:
            break;
    }
}

void network_server_broadcast_playerlist() {
    message_playerlist_t playerlist;
    memcpy(&playerlist.players, state.players, sizeof(state.players));
    ENetPacket* packet = enet_packet_create(&playerlist, sizeof(message_playerlist_t), ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(state.host, 0, packet);
    enet_host_flush(state.host);
}

void network_server_start_loading() {
    // Check that all the players are ready
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (state.players[player_id].status == PLAYER_STATUS_NOT_READY) {
            return;
        }
    }

    // Set them to not ready so that they can re-ready themselves once they enter the match state
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (state.players[player_id].status == PLAYER_STATUS_READY) {
            state.players[player_id].status = PLAYER_STATUS_NOT_READY;
        }
    }

    // Broadcast a match start message to all clients
    message_match_load_t match_load;
#ifdef GOLD_RAND_SEED
    match_load.random_seed = GOLD_RAND_SEED;
#else
    match_load.random_seed = (uint32_t)time(NULL);
#endif
    ENetPacket* packet = enet_packet_create(&match_load, sizeof(message_match_load_t), ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(state.host, 0, packet);
    enet_host_flush(state.host);

    // Send a match start even to the menu
    network_event_t event;
    event.type = NETWORK_EVENT_MATCH_LOAD;
    network_event_enqueue(event);

    lcg_srand(match_load.random_seed);
    log_info("Set random seed to %u", match_load.random_seed);
}

void network_server_start_match() {
    message_match_start_t match_start;
    ENetPacket* packet = enet_packet_create(&match_start, sizeof(message_match_start_t), ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(state.host, 0, packet);
    enet_host_flush(state.host);
}

void network_server_send_input(uint8_t* data, size_t data_length) {
    data[0] = MESSAGE_INPUT;
    ENetPacket* packet = enet_packet_create(data, data_length * sizeof(uint8_t), ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(state.host, 0, packet);
    enet_host_flush(state.host);
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

void client_handle_message(uint8_t* data, size_t length) {
    uint8_t message_type = data[0];

    switch (message_type) {
        case MESSAGE_GREET_RESPONSE: {
            message_greet_response_t greet_response;
            memcpy(&greet_response, data, sizeof(message_greet_response_t));

            network_event_t event;
            state.status = NETWORK_STATUS_CONNECTED;
            log_info("setting status to connected.")
            if (greet_response.status == GREET_RESPONSE_ACCEPTED) {
                log_info("server accepted client");
                state.player_id = greet_response.player_id;
                event.type = NETWORK_EVENT_JOINED_LOBBY;
            } else if (greet_response.status == GREET_RESPONSE_LOBBY_FULL) {
                log_info("server lobby is full");
                event.type = NETWORK_EVENT_LOBBY_FULL;
                network_disconnect();
            } else if (greet_response.status == GREET_RESPONSE_INVALID_VERSION) {
                log_info("client version invalid");
                event.type = NETWORK_EVENT_INVALID_VERSION;
                network_disconnect();
            }
            network_event_enqueue(event);
            break;
        }
        case MESSAGE_PLAYERLIST: {
            memcpy(state.players, data + 4, sizeof(state.players));
            break;
        }
        case MESSAGE_MATCH_LOAD: {
            uint32_t random_seed;
            memcpy(&random_seed, data + 4, sizeof(uint32_t)); 
            lcg_srand(random_seed);
            log_info("Set random seed to %u", random_seed);

            network_event_t event;
            event.type = NETWORK_EVENT_MATCH_LOAD;
            network_event_enqueue(event);
            break;
        }
        case MESSAGE_MATCH_START: {
            network_event_t event;
            event.type = NETWORK_EVENT_MATCH_START;
            network_event_enqueue(event);
            break;
        }
        case MESSAGE_INPUT: {
            network_event_input_message_enqueue(data, length);
            break;
        }
        default:
            break;
    }
}

void network_client_toggle_ready() {
    message_ready_t ready;
    ENetPacket* packet = enet_packet_create(&ready, sizeof(message_ready_t), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(state.peer, 0, packet);
    enet_host_flush(state.host);
}

void network_client_send_input(uint8_t* data, size_t data_length) {
    data[0] = MESSAGE_INPUT;
    ENetPacket* packet = enet_packet_create(data, data_length * sizeof(uint8_t), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(state.peer, 0, packet);
    enet_host_flush(state.host);
}