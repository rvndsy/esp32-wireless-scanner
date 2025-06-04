#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_net_stack.h"
#include "lwip/ip4_addr.h"
#include "lwip/etharp.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "conf.h"

#include "scanner.h"

// Global definitions
#define TAG "Scanner"

ipv4_list * arp_scan_full(void);

void flip_ipv4_bits(uint32_t * ipv4);
void next_ipv4(uint32_t * ipv4);

ipv4_list * create_ipv4_list(uint32_t first_ip, uint32_t subnet);
ipv4_info * get_from_ipv4_list_at(const ipv4_list * ipv4_list, uint32_t index);
void add_to_ipv4_list(ipv4_list * ipv4_list, uint32_t ip, uint8_t mac[6]);
void delete_ipv4_list(ipv4_list * ipv4_list);

// Global variables
uint32_t deviceCount = 0; // store the exact online device count after the loop
uint32_t max_subnet_ip_count = 0; // store the maxium device that subnet can hold

ipv4_list * online_ipv4_list; // store last ip, MAC, and online information to device

ipv4_list * arp_scan_full(void) {
    ESP_LOGI(TAG, "Starting ARP scan");

    // Checking for AP connection
    wifi_ap_record_t ap_info_check;
    if (esp_wifi_sta_get_ap_info(&ap_info_check) != ESP_OK) {
        ESP_LOGI(TAG, "Failed to start ARP scan. No AP connection detected.");
        return;
    }

    // acquire esp_netif and ip_info of this device
    esp_netif_t* esp_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    struct netif *netif = (struct netif *)esp_netif_get_netif_impl(esp_netif);
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif, &ip_info);

    // get subnet range
    esp_ip4_addr_t first_ip;
    first_ip.addr = ip_info.netmask.addr & ip_info.ip.addr; // get the first ip of subnet

    // calculate subnet max device count
    uint32_t subnet_mask = ip_info.netmask.addr;
    flip_ipv4_bits(&subnet_mask);
    max_subnet_ip_count = UINT32_MAX - subnet_mask - 1; // the total count of ips to scan

    // char buffer for printing ipv4 addresses
    char char_target_ip[IP4ADDR_STRLEN_MAX];
    // get first, last ipv4 addresses
    esp_ip4_addr_t current_target_ip, last_ip;
    current_target_ip.addr = first_ip.addr;
    last_ip.addr = first_ip.addr | (~ip_info.netmask.addr); // calculate last ip in subnet

    uint32_t online_ipv4_count = 0;

    ESP_LOGI(TAG,"%" PRIu32 " ipv4 addresses to scan", max_subnet_ip_count);
    esp_ip4addr_ntoa(&last_ip, char_target_ip, IP4ADDR_STRLEN_MAX); // convert IP from network to host
    ESP_LOGI(TAG, "Last ip %s", char_target_ip);
    esp_ip4addr_ntoa(&first_ip, char_target_ip, IP4ADDR_STRLEN_MAX); // convert IP from network to host
    ESP_LOGI(TAG, "First ip %s", char_target_ip);

    // ipv4 online address storage
    ipv4_list * ipv4_list = create_ipv4_list(first_ip.addr, subnet_mask);
    if(ipv4_list == NULL){
        ESP_LOGE(TAG, "arp_scan_full(): Failed to allocate ipv4_list with create_ipv4_list()");
        return;
    }

    // scan the entire subnet in batches
    while (current_target_ip.addr != last_ip.addr) {
        esp_ip4_addr_t current_addresses[ARP_BATCH_SIZE]; // save current loop ip
        uint32_t current_arp_count = 0; // for checking ARP table use

        // send ARP request in batches because ARP table has a limited size
        for (int i = 0; i < ARP_BATCH_SIZE; i++) {
            next_ipv4(&current_target_ip.addr); // next ip
            if (current_target_ip.addr != last_ip.addr) {
                esp_ip4addr_ntoa(&current_target_ip, char_target_ip, IP4ADDR_STRLEN_MAX); // convert IP from network to host
                current_addresses[i] = current_target_ip;
                ESP_LOGI(TAG, "Success sending ARP to %s", char_target_ip);

                // send ARP request
                etharp_request(netif, (ip4_addr_t*)&current_target_ip);
                current_arp_count++;
            } else {
                break; // ip is last ip in subnet then break
            }
        }

        // wait for ARP responses
        vTaskDelay(pdMS_TO_TICKS(ARP_TIMEOUT));

        // find received ARP responses in ARP table
        flip_ipv4_bits(&first_ip.addr); //flip it to use in calculations, won't be needed elsewhere
        for(int i = 0; i < current_arp_count; i++) {
            ip4_addr_t *ipaddr_ret = NULL;   // To retrieve IP address from ARP table
            struct eth_addr *eth_ret = NULL; // To retrieve MAC address from ARP table
            char mac[20], char_currIP[IP4ADDR_STRLEN_MAX]; // For printing

            // unsigned int current_arp_count = flip_ipv4_bits(&current_addresses[i].addr) - flip_ipv4_bits(&first_ip.addr) - 1; 
            // calculate current size of ARP table
            unsigned int current_ipv4_count = current_addresses[i].addr; 
            flip_ipv4_bits(&current_ipv4_count);
            current_ipv4_count = first_ip.addr - 1;

            // If IP is located in ARP table, retrieve its MAC
            if (etharp_find_addr(NULL, (ip4_addr_t*)&current_addresses[i], &eth_ret, &ipaddr_ret) != -1) {
                // print MAC result for ip
                sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",eth_ret->addr[0],eth_ret->addr[1],eth_ret->addr[2],eth_ret->addr[3],eth_ret->addr[4],eth_ret->addr[5]);
                esp_ip4addr_ntoa(&current_addresses[i], char_currIP, IP4ADDR_STRLEN_MAX);
                ESP_LOGI(TAG, "%s's MAC address is %s", char_currIP, mac);

                add_to_ipv4_list(ipv4_list, current_addresses[i].addr, eth_ret->addr);

                // count total online device
                online_ipv4_count++;
            }
        }

    }
    // Check the list
    for (int i = 0; i < ipv4_list->size; i++) {
        ipv4_info * info = get_from_ipv4_list_at(ipv4_list, i);
        esp_ip4addr_ntoa((esp_ip4_addr_t*)&info->ip, char_target_ip, IP4ADDR_STRLEN_MAX); // convert IP from network to host
        printf("%s\n", char_target_ip);
    }
    // update deviceCount
    deviceCount = online_ipv4_count;
    ESP_LOGI(TAG, "Scan done. %" PRIu32 " devices are on local network", online_ipv4_count);

    return ipv4_list;
}

// flip ip address
void flip_ipv4_bits(uint32_t *ipv4){
    *ipv4 = ((*ipv4 & 0xff000000) >> 24) |
            ((*ipv4 & 0xff0000)   >> 8)  |
            ((*ipv4 & 0xff00)     << 8)  |
            ((*ipv4 & 0xff)       << 24);
    return;
}

// get the next ipv4 address
void next_ipv4(uint32_t * ipv4){
    // reconstruct it to normal order
    uint32_t normal_ipv4 = *ipv4;
    flip_ipv4_bits(&normal_ipv4); // switch to the normal way

    // if value is all 1's then its the last ipv4 address in the ipv4 address space
    // else get next ipv4 address
    if (normal_ipv4 == UINT32_MAX) {
        return;
    } else {
        // because of endianess
        normal_ipv4++;
        flip_ipv4_bits(&normal_ipv4);
        *ipv4 = normal_ipv4;
    }
}

ipv4_list * create_ipv4_list(uint32_t first_ip, uint32_t subnet) {
    ipv4_list * list = calloc(1, sizeof(ipv4_list));
    if (list == NULL) {
        ESP_LOGE(TAG, "Failed to initialize ipv4_list * list in create_ipv4_list(). Out of memory?");
        return NULL;
    }

    list->first_ip = first_ip;
    list->subnet = subnet;
    list->head = NULL;
    list->size = 0;

    return list;
}

ipv4_info * get_from_ipv4_list_at(const ipv4_list * ipv4_list, uint32_t index_at) {
    if (index_at >= ipv4_list->size) {
        return NULL;
    }
    if (index_at == 0) {
        return ipv4_list->head;
    }
    if (index_at == 1) {
        return ipv4_list->head->next;
    }
    ipv4_info * tmp_info_ptr = ipv4_list->head->next->next;

    for (uint32_t i = 2; i != index_at && tmp_info_ptr->next != NULL; i++, tmp_info_ptr = tmp_info_ptr->next) {
        if (tmp_info_ptr == ipv4_list->head) {
            return NULL;
        }
    }

    return tmp_info_ptr;
}

void add_to_ipv4_list(ipv4_list * ipv4_list, uint32_t ip, uint8_t * mac) {
    if (ipv4_list == NULL) {
        ESP_LOGE(TAG, "add_to_ipv4_list(): ipv4_list is NULL");
        return;
    }

    if ((ipv4_list->size == 0 && ipv4_list->head != NULL) || (ipv4_list->size > 0 && ipv4_list->head == NULL)) {
        ESP_LOGE(TAG, "add_to_ipv4_list(): ipv4_list size and head pointer don't match. The list is supposed to be empty.");
        return;
    }

    ipv4_info * next_info = calloc(1, sizeof(ipv4_info));
    if (next_info == NULL) {
        ESP_LOGE(TAG, "add_to_ipv4_list(): Failed to allocate next_info next ipv4_info list node");
        return;
    }
    next_info->ip = ip;
    memcpy(next_info->mac, mac, sizeof(uint8_t) * 6);

    if (ipv4_list->size == 0 && ipv4_list->head == NULL) {
        ipv4_list->head = next_info;
        next_info->next = ipv4_list->head;
        next_info->prev = ipv4_list->head;
    } else if (ipv4_list->head->next == ipv4_list->head) {
        ipv4_list->head->next = next_info;
        ipv4_list->head->prev = next_info;
        next_info->next = ipv4_list->head;
        next_info->prev = ipv4_list->head;
    } else {
        ipv4_list->head->prev->next = next_info;
        next_info->next = ipv4_list->head;
        next_info->prev = ipv4_list->head->prev;
        ipv4_list->head->prev = next_info;
    }

    (ipv4_list->size)++;

    return;
}

void delete_ipv4_list(ipv4_list * ipv4_list) {
    ipv4_info * tmp_info_ptr = ipv4_list->head->next;

    ipv4_list->head->prev->next = NULL;

    while (tmp_info_ptr->next != NULL) {
        free(ipv4_list->head);
        ipv4_list->head = tmp_info_ptr;
        tmp_info_ptr = ipv4_list->head->next;
    }
    free(tmp_info_ptr);
    ipv4_list = NULL;

    return;
}
