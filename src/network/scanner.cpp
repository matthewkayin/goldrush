#include "scanner.h"

const NetworkLobby& INetworkScanner::get_lobby(size_t index) const {
    return scanner_lobbies[index];
}

const size_t INetworkScanner::get_lobby_count() const {
    return scanner_lobbies.size();
}