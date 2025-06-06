#ifndef SCANNER_H_
#define SCANNER_H_

#include "esp_netif_ip_addr.h"
#include <stdint.h>

// device information, stored in a linked list because count of online devices is unknown and 
// memory is scarce
typedef struct ipv4_info {
    uint32_t ip;    // in host order
    uint8_t mac[6];
    struct ipv4_info * next;
    struct ipv4_info * prev;
} ipv4_info;

typedef struct ipv4_list {
    uint32_t first_ip;
    uint32_t subnet;
    uint32_t size;
    struct ipv4_info * head;
} ipv4_list;

// scan loop
extern ipv4_list * arp_scan_full(void);

//ipv4_list linked list operations
extern ipv4_list * create_ipv4_list(uint32_t first_ip, uint32_t subnet); // argument target_ip in network order
extern ipv4_info * get_from_ipv4_list_at(const ipv4_list * ipv4_list, uint32_t index);
extern void add_to_ipv4_list(ipv4_list * ipv4_list, uint32_t ip, uint8_t mac[6]);
extern void delete_ipv4_list(ipv4_list * ipv4_list);
extern ipv4_info * arp_scan_single(const esp_ip4_addr_t target_ip); // argument target_ip in network order

#endif
