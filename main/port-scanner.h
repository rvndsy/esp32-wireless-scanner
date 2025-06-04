#ifndef PORT_SCANNER_H
#define PORT_SCANNER_H

#include "esp_netif_ip_addr.h"

void scan_ports(esp_ip4_addr_t target_ip);

#endif
