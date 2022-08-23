#pragma once

int wifi_cmd_ap(int argc, char **argv);
int wifi_cmd_scan(int argc, char **argv);
int wifi_cmd_query(int argc, char **argv);
wifi_event_ftm_report_t *wifi_cmd_ftm(const char *ssid);
esp_err_t ftm_set_offset(int16_t offset_cm);

bool wifi_cmd_ap_set(const char* ssid, const char* pass);

void ftm_app_main(void);