#pragma once

#include "network/types.h"
#include <vector>

class INetworkScanner {
public:
    virtual ~INetworkScanner() = default;
    virtual bool is_initialized_successfully() const = 0;
    virtual void search_lobbies(const char* query) = 0;
    virtual void service() = 0;

    const NetworkLobby& get_lobby(size_t index) const;
    size_t get_lobby_count() const;
protected:
    char scanner_lobby_name_query[NETWORK_LOBBY_NAME_BUFFER_SIZE];
    std::vector<NetworkLobby> scanner_lobbies;
};