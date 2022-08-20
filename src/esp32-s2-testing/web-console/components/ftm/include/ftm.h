#pragma once

int wifi_cmd_ap(int argc, char **argv);
int wifi_cmd_scan(int argc, char **argv);
int wifi_cmd_query(int argc, char **argv);
int wifi_cmd_ftm(int argc, char **argv);

bool wifi_cmd_ap_set(const char* ssid, const char* pass);