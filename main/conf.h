#ifndef CONF_H_
#define CONF_H_

#define TAG "WiFi_Scanner"

/* ARP scanning */
// ARP scanning uses the ARP table records to determine if an IP/device is up (ipv4 only for now).
// ARP packets are sent out in batches. One ARP packet to one IP per scan (for now... sending multiple packets might be better on unstable networks).
// First batch of ARP_BATCH_SIZE amount of ARP packets is sent, then wait for ARP_TIMEOUT before sending the second batch of same size.
// Final batch size is truncated. There are issues with ARP scan reliability on weak connection/unstable networks.
#define ARP_TIMEOUT 7000                    // how long to wait between sending arp batches (patience is important...)
#define INDIVIDUAL_ARP_TIMEOUT ARP_TIMEOUT  // how long to wait for pinging a single ip with arp
#define ARP_BATCH_SIZE 64                   // how many arp packets to send out in a single batch

/* Port (TCP) scanning */
// Port scanning tries to establish a TCP connection with a port. If connection is successful, port is written into port bitmap that its open.
// Port scanning is done on a given range of ports starting with MIN_PORT ending with MAX_PORT.
// TCP connections are opened in a queue-like manner - when one TCP connection is timed-out or connected/failed a slot is opened for a TCP connection for the next port in line.
// MAX_ONGOING_CONNECTIONS connections max can be open at any time during a scan. Too many MAX_ONGOING_CONNECTIONS will absolutely crash the ESP32 - don't go above 20 to be safe.
// == DONT CHANGE ==
#define MAX_POSSIBLE_PORT 65535             // end of port range (2^16-1)
// =================

//#define ESP_PORT 8080                     // used for opening a socket on esp32 (not required for now)
#define MIN_PORT 1                          // must be greater than 0 (inclusive)
#define MAX_PORT 100                        // must be less than or equal to MAX_POSSIBLE_PORT (inclusive)
#define MAX_ONGOING_CONNECTIONS 16          // must be between 0 and max open port count defined in sdkconfig (don't recommend going above ~20)
#define TCP_CONN_TIMEOUT_MS 5000            // how long to wait for a single tcp connection to time out (lwip doesn't have timeout?)

// == DONT CHANGE ==
#define OPEN_PORT_MAP_SIZE MAX_PORT / 8 + 1 // size of port bitmap storage in bytes (uint8_t's) while scanning ports with TCP
// =================
/******************/

/* WiFi/AP scanning/searching and connecting */
#define MAX_SCAN_RESULTS 20                 // how many WiFi/AP's we store from last scan (in code search means 'scanning' for AP's)
#define DEFAULT_WIFI_INPUT_PASSWORD ""      // in popup to enter WiFi/AP password, autofill this in on startup (touch is broken - keyboard is hard to use)
/***********/

/* SNTP/Time syncing */
// Time is synced from SNTP after connecting with a WiFi/AP the first time. When power supply is unplugged time information is completely lost.
#define SNTP_SERVER "162.159.200.123"       // (162.159.200.123 - cloudflare ip)
#define SNTP_SYNC_ATTEMPTS 200              // how many times to check if retrieved time from SNTP
#define SNTP_SYNC_ATTEMPT_DELAY 1000        // how long to wait before checking SNTP time sync again
/*********************/

/* Server/AP mode config */
// The ESP32 can be turned into an AP and start a HTTP server to access all previous scan results on a HTTP webpage.
#define AP_PASSWORD "password123"           // password of ESP32 AP
#define AP_MAX_CONNECTIONS 1                // how many devices can connect to ESP32 AP
#define AP_AUTH_MODE WIFI_AUTH_WPA2_PSK     // security used by ESP32 AP - enum found in wifi_auth_mode_t in esp-wifi-types.h (WPA3 doesn't work)
#define AP_SSID CONFIG_LWIP_LOCAL_HOSTNAME  // SSID of ESP32 AP (CONFIG_LWIP_LOCAL_HOSTNAME found in sdkconfig)
// Webpage
#define BASE_PATH "/littlefs"               // root of LittleFS filesystem
#define PAGE_NAME "ESP32"                   // displayed title of webpage in browser
#define FAVICON_PATH "favicon.ico"          // path of favicon (file in ../server-image)
/**********************/

#endif
