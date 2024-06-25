#pragma once

bool network_init();
void network_quit();
void network_disconnect();
bool network_is_server();

bool network_server_create();
void network_server_get_ip(char* ip_buffer);

bool network_client_create(const char* server_ip);