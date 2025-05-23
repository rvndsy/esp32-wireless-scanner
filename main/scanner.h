#ifndef SCANNER_H_
#define SCANNER_H_

#include <stdint.h>

// device information
typedef struct deviceInfo{
    int online;
    uint32_t ip;
    uint8_t mac[6];
} deviceInfo;

// scan loop
void arp_scan_full();

// gets
uint32_t get_max_device();
uint32_t get_device_count();
deviceInfo * get_device_info();

#endif
