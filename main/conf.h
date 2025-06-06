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
#define MAX_ONGOING_CONNECTIONS 32 //must be between 0 and max open port count defined in sdkconfig
#define TCP_CONN_TIMEOUT_MS 10000

// == DONT CHANGE ==
#define OPEN_PORT_MAP_SIZE MAX_PORT / 8 + 8
// =================

/* WiFi/AP */
#define MAX_SCAN_RESULTS 20

/* SNTP/Time syncing */
#define SNTP_SERVER "162.159.200.123"
#define SNTP_SYNC_ATTEMPTS 200
#define SNTP_SYNC_ATTEMPT_DELAY 1000

#endif
