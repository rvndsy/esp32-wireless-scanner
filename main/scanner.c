#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_net_stack.h"
#include "lwip/ip4_addr.h"
#include "lwip/etharp.h"

#include <inttypes.h>
#include <string.h>

#include "scanner.h"

// Global definitions
#define TAG "Scanner"
#define ARP_TIMEOUT 1000
#define ARP_BATCH_SIZE 32 

uint32_t switch_ip_orientation (uint32_t *);
void nextIP(esp_ip4_addr_t *);

// Global variables
uint32_t deviceCount = 0; // store the exact online device count after the loop
uint32_t maxSubnetDevice = 0; // store the maxium device that subnet can hold
deviceInfo *device_info_storage; // store all ip, MAC, and online information to device

void arp_scan_full() {
    ESP_LOGI(TAG, "Starting ARP scan");

    // acquire esp_netif and ip_info of this device
    esp_netif_t* esp_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    struct netif *netif = (struct netif *)esp_netif_get_netif_impl(esp_netif);
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif, &ip_info);

    // get subnet range
    esp_ip4_addr_t first_ip;
    first_ip.addr = ip_info.netmask.addr & ip_info.ip.addr; // get the first ip of subnet

    // calculate subnet max device count
    uint32_t subnet_mask = switch_ip_orientation(&ip_info.netmask.addr);
    maxSubnetDevice = UINT32_MAX - subnet_mask - 1; // the total count of ips to scan

    // initialize device information storing database
    device_info_storage = calloc(maxSubnetDevice, sizeof(deviceInfo));
    if(device_info_storage == NULL){
        ESP_LOGI(TAG, "Not enough space for storing information. Subnet size: %u", maxSubnetDevice);
        return;
    }

    // scanning all ips from local network
    uint32_t onlineDevicesCount = 0;
    ESP_LOGI(TAG,"%" PRIu32 " ips to scan", maxSubnetDevice);

    // ips
    char char_target_ip[IP4ADDR_STRLEN_MAX]; // char ip for printing
    esp_ip4_addr_t current_ip, last_ip;
    current_ip.addr = first_ip.addr;
    last_ip.addr = first_ip.addr | (~ip_info.netmask.addr); // calculate last ip in subnet

    // scan the entire subnet in batches
    while (current_ip.addr != last_ip.addr) {
        esp_ip4_addr_t current_addresses[ARP_BATCH_SIZE]; // save current loop ip
        uint32_t current_arp_count = 0; // for checking Arp table use

        // send ARP request in batches because ARP table has limit size
        for (int i = 0; i < ARP_BATCH_SIZE; i++) {
            nextIP(&current_ip); // next ip
            if (current_ip.addr != last_ip.addr) {
                esp_ip4addr_ntoa(&current_ip, char_target_ip, IP4ADDR_STRLEN_MAX); // convert IP from network to host
                current_addresses[i] = current_ip;
                ESP_LOGI(TAG, "Success sending ARP to %s", char_target_ip);

                // send ARP request
                etharp_request(netif, (ip4_addr_t*)&current_ip);
                current_arp_count++;
            } else {
                break; // ip is last ip in subnet then break
            }
        }
        // wait for resopnd
        vTaskDelay(pdMS_TO_TICKS(ARP_TIMEOUT));

        // find received ARP responses in ARP table
        for(int i=0; i<current_arp_count; i++) {
            ip4_addr_t *ipaddr_ret = NULL;   // IP address
            struct eth_addr *eth_ret = NULL; // MAC address
            char mac[20], char_currIP[IP4ADDR_STRLEN_MAX];

            unsigned int currentIpCount = switch_ip_orientation(&current_addresses[i].addr) - switch_ip_orientation(&first_ip.addr) - 1; // calculate current size of ARP table
            if(etharp_find_addr(NULL, (ip4_addr_t*)&current_addresses[i], &eth_ret, &ipaddr_ret) != -1){ // find in ARP table
                // print MAC result for ip
                sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",eth_ret->addr[0],eth_ret->addr[1],eth_ret->addr[2],eth_ret->addr[3],eth_ret->addr[4],eth_ret->addr[5]);
                esp_ip4addr_ntoa(&current_addresses[i], char_currIP, IP4ADDR_STRLEN_MAX);
                ESP_LOGI(TAG, "%s's MAC address is %s", char_currIP, mac);

                // stroing information to database
                device_info_storage[currentIpCount] = (deviceInfo){1, current_addresses[i].addr}; // storing online status and ip address specified by ip No.
                memcpy(device_info_storage[currentIpCount].mac, eth_ret->addr, 6); // storing mac address into database specified by ip No.

                // count total online device
                onlineDevicesCount++;
            } else { // not fount in arp table
                if (device_info_storage[currentIpCount].online == 1 || device_info_storage[currentIpCount].online == 2) { // previously online
                    device_info_storage[currentIpCount].online = 2; // prvonline
                } else {
                    device_info_storage[currentIpCount].online = 0; // offline

                    device_info_storage[currentIpCount].ip = current_addresses[i].addr;
                }
            }
        }

        // update deviceCount
        deviceCount = onlineDevicesCount;
        ESP_LOGI(TAG, "Scan done. %" PRIu32 " devices are on local network", onlineDevicesCount);
    }
}

// switch between the lwip and the normal way of representation of ipv4
uint32_t switch_ip_orientation (uint32_t *ipv4){
    uint32_t ip =   ((*ipv4 & 0xff000000) >> 24)|\
                    ((*ipv4 & 0xff0000)   >> 8) |\
                    ((*ipv4 & 0xff00)     << 8) |\
                    ((*ipv4 & 0xff)       << 24);
    return ip;
}

// get the next ip in numerical order
void nextIP(esp_ip4_addr_t *ip){
    // reconstruct it to normal order
    esp_ip4_addr_t normal_ip;
    normal_ip.addr = switch_ip_orientation(&ip->addr); // switch to the normal way

    // check if ip is the last ip in subnet
    if(normal_ip.addr == UINT32_MAX) return;

    // add one to obtain the next ip address location
    normal_ip.addr += (uint32_t)1;
    ip->addr = switch_ip_orientation(&normal_ip.addr); // switch back the lwip way

    return;
}
