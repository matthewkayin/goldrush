#include "host.h"

#include "core/logger.h"

// BASE

bool NetworkHost::poll_events(NetworkHostEvent* event) {
    if (events.empty()) {
        return false;
    }

    *event = events.front();
    events.pop();

    return true;
}

// PACKET

void network_host_packet_destroy(NetworkHostPacket packet) {
    if (packet.backend == NETWORK_BACKEND_LAN) {
        enet_packet_destroy(packet.enet_packet);
    } else if (packet.backend == NETWORK_BACKEND_STEAM) {
        packet.steam_message->Release();
    }
}