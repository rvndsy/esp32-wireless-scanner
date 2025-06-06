#ifndef PORT_SCANNER_H
#define PORT_SCANNER_H

#include "esp_netif_ip_addr.h"

extern uint8_t * scan_ports(const esp_ip4_addr_t target_ip, uint8_t * port_map);

#endif // PORT_SCANNER_H
