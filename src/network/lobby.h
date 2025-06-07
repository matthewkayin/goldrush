#pragma once

#include "types.h"
#include <enet/enet.h>
#include <steam/steam_api.h>

enum NetworkLobbyStatus {
    NETWORK_LOBBY_CLOSED,
    NETWORK_LOBBY_OPENING,
    NETWORK_LOBBY_OPEN,
    NETWORK_LOBBY_ERROR
};

class NetworkLobby {
public:
    NetworkLobby() {
        status = NETWORK_LOBBY_CLOSED;
    }
    NetworkLobbyStatus get_status() const {
        return status;
    }
    virtual ~NetworkLobby() = default;
    virtual void open(const char* name, NetworkConnectionInfo connection_info) = 0;
    virtual void close() = 0;
    virtual void service() = 0;
    virtual void set_player_count(uint8_t player_count) = 0;
protected:
    NetworkLobbyStatus status;
};

class NetworkLanLobby : public NetworkLobby {
public:
    void open(const char* name, NetworkConnectionInfo connection_info) override;
    void close() override;
    void service() override;
    void set_player_count(uint8_t player_count) override;
private:
    ENetSocket scanner;
    NetworkLanLobbyInfo lobby_info;
};

class NetworkSteamLobby : public NetworkLobby {
public:
    void open(const char* name, NetworkConnectionInfo connection_info) override;
    void close() override;
    void service() override;
    void set_player_count(uint8_t player_count) override;
private:
    char lobby_name[NETWORK_LOBBY_NAME_BUFFER_SIZE];
    uint8_t lobby_player_count;
    CSteamID lobby_id;
    NetworkConnectionInfo connection_info;
    CCallResult<NetworkSteamLobby, LobbyCreated_t> call_result_lobby_created;
    void on_lobby_created(LobbyCreated_t* lobby_created, bool io_failure);
};