#ifndef SCANNER_H_
#define SCANNER_H_

#include <stdint.h>

// device information, stored in a linked list because count of online devices is unknown and 
// memory is scarce
typedef struct ipv4_info {
    uint32_t ip;
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
extern ipv4_list * create_ipv4_list(uint32_t first_ip, uint32_t subnet);
extern ipv4_info * get_from_ipv4_list_at(const ipv4_list * ipv4_list, uint32_t index);
extern void add_to_ipv4_list(ipv4_list * ipv4_list, uint32_t ip, uint8_t mac[6]);
extern void delete_ipv4_list(ipv4_list * ipv4_list);

#endif
