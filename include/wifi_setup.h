#pragma once

bool wifi_connect_blocking(int timeout_sec);
bool wifi_is_connected();
bool wifi_ensure_connected(int timeout_sec);
void wifi_sync_time();
