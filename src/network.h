#pragma once

enum NetworkStatus {
    NETWORK_STATUS_OFFLINE,
    NETWORK_STATUS_CONNECTING,
    NETWORK_STATUS_CONNECTED
};

bool network_init();
void network_quit();
void network_disconnect();
bool network_is_server();
NetworkStatus network_get_status();
void network_poll_events();

bool network_server_create();
void network_server_get_ip(char* ip_buffer);

bool network_client_create(const char* server_ip);