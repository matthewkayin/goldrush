#include "host.h"

bool INetworkHost::poll_events(NetworkHostEvent* event) {
    if (host_events.empty()) {
        return false;
    }

    *event = host_events.front();
    host_events.pop();

    return true;
}

const char* INetworkHost::get_lobby_name() const {
    return host_lobby_name;
}

void INetworkHost::set_lobby_name(const char* value) {
    strncpy(host_lobby_name, value, sizeof(host_lobby_name));
}