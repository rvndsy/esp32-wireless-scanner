#ifndef CONF_H_
#define CONF_H_

#define TAG "WiFi_Scanner"

/* ARP scanning */
#define ARP_TIMEOUT 7000   //patience is important...
#define INDIVIDUAL_ARP_TIMEOUT ARP_TIMEOUT
#define ARP_BATCH_SIZE 64

/* Port scanning */
// == DONT CHANGE ==
#define MAX_POSSIBLE_PORT 65535
// =================

//#define ESP_PORT 8080       // not really required for now
// Scannable port range
#define MIN_PORT 0            // greater than 0
#define MAX_PORT 100
#define MAX_ONGOING_CONNECTIONS 16 //must be between 0 and max open port count defined in sdkconfig
#define TCP_CONN_TIMEOUT_MS 5000

// == DONT CHANGE ==
#define OPEN_PORT_MAP_SIZE MAX_PORT / 8 + 8
// =================
/******************/

/* WiFi/AP */
#define MAX_SCAN_RESULTS 20
#define DEFAULT_WIFI_INPUT_PASSWORD ""
/***********/

/* SNTP/Time syncing */
#define SNTP_SERVER "162.159.200.123" //cloudflare ip
#define SNTP_SYNC_ATTEMPTS 200
#define SNTP_SYNC_ATTEMPT_DELAY 1000
/*********************/

/* Server/AP mode config */
#define AP_PASSWORD "password123"
#define AP_MAX_CONNECTIONS 1
#define AP_AUTH_MODE WIFI_AUTH_WPA2_PSK //wifi_auth_mode_t in esp-wifi-types.h
#define AP_SSID CONFIG_LWIP_LOCAL_HOSTNAME
// Webpage
#define BASE_PATH "/littlefs"
#define PAGE_NAME "ESP32"
#define FAVICON_PATH "favicon.ico"
/**********************/


#endif
