#ifndef CONF_H_
#define CONF_H_

#define TAG "WiFi_Scanner"

/* ARP scanning */
#define ARP_TIMEOUT 500
#define ARP_BATCH_SIZE 5

/* Port scanning */
//#define ESP_PORT 8080 // not really required for now

#define MAX_MAX_PORT 65535

// Scannable port range
#define MIN_PORT 0 // greater than 0
#define MAX_PORT MAX_MAX_PORT

#define OPEN_PORT_MAP_SIZE MAX_PORT / 8 + 8
#define MAX_ONGOING_CONNECTIONS 128 //must be between 0 and max open port count defined in sdkconfig
#define TCP_CONN_TIMEOUT_MS 12000

#endif
