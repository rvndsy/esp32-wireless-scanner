#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_net_stack.h"
#include "lwip/def.h"
#include "lwip/ip4_addr.h"
#include "lwip/etharp.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "conf.h"

#include "net-scanner.h"

// Global definitions
#define TAG "ARP_Scanner"

ipv4_info * arp_scan_single(const esp_ip4_addr_t target_ip);
ipv4_list * arp_scan_full(void);

void next_ipv4(uint32_t * ipv4);

ipv4_list * create_ipv4_list(uint32_t first_ip, uint32_t subnet);
ipv4_info * create_ipv4_info_node(uint32_t ip, uint8_t * mac);
ipv4_info * get_from_ipv4_list_at(const ipv4_list * ipv4_list, uint32_t index);
void add_to_ipv4_list(ipv4_list * ipv4_list, uint32_t ip, uint8_t mac[6]);
void delete_ipv4_list(ipv4_list * ipv4_list);



// Global variables
uint32_t deviceCount = 0; // store the exact online device count after the loop
uint32_t max_subnet_ip_count = 0; // store the maxium device that subnet can hold

ipv4_list * online_ipv4_list; // store last ip, MAC, and online information to device


void print_info(const ip4_addr_t *ip, const struct eth_addr * mac) {
    if (ip == NULL || mac == NULL) {
        return;
    }
    char mac_str[20], ip_str[IP4ADDR_STRLEN_MAX]; // For printing
    sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X",mac->addr[0],mac->addr[1],mac->addr[2],mac->addr[3],mac->addr[4],mac->addr[5]);
    esp_ip4addr_ntoa(ip, ip_str, IP4ADDR_STRLEN_MAX); // ignore clang warnings for now...
    ESP_LOGI(TAG, "%s's mac_ret address is %s", ip_str, mac_str);
}

ipv4_info * arp_scan_single(const esp_ip4_addr_t target_ip) {
    // Acquiring netif
    esp_netif_t* esp_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    struct netif *netif = (struct netif *)esp_netif_get_netif_impl(esp_netif);

    // Wipe the ARP table
    etharp_cleanup_netif(netif);
    vTaskDelay(25);

    // Send ARP request and wait...
    etharp_request(netif, (ip4_addr_t*)&target_ip);
    vTaskDelay(INDIVIDUAL_ARP_TIMEOUT);

    // Check if in ARP table
    ip4_addr_t *ip_ret = NULL;                 // To retrieve IP address from ARP table
    struct eth_addr *mac_ret = NULL;               // To retrieve MAC address from ARP table
    if (etharp_find_addr(NULL, (ip4_addr_t*)&target_ip, &mac_ret, &ip_ret) != -1) {
        // print MAC result for ip
        print_info(ip_ret, mac_ret);

        return create_ipv4_info_node(ntohl(ip_ret->addr), mac_ret->addr);
    } else {
        return NULL;
    }
}

ipv4_list * arp_scan_full(void) {
    ESP_LOGI(TAG, "Starting ARP scan");

    // Checking for AP connection
    wifi_ap_record_t ap_info_check;
    if (esp_wifi_sta_get_ap_info(&ap_info_check) != ESP_OK) {
        ESP_LOGI(TAG, "Failed to start ARP scan. No AP connection detected.");
        return NULL;
    }

    // acquire esp_netif and ip_info of this device
    esp_netif_t* esp_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    struct netif *netif = (struct netif *)esp_netif_get_netif_impl(esp_netif);
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif, &ip_info);

    // Wipe the ARP table
    etharp_cleanup_netif(netif);

    // get subnet range
    esp_ip4_addr_t first_ip; // esp_ip4_addr_t in network order
    first_ip.addr = ip_info.netmask.addr & ip_info.ip.addr; // get the first ip of subnet

    // calculate subnet max device count
    uint32_t subnet_mask = ntohl(ip_info.netmask.addr);
    max_subnet_ip_count = UINT32_MAX - subnet_mask - 1; // the total count of ips to scan

    // char buffer for printing ipv4 addresses
    char char_target_ip[IP4ADDR_STRLEN_MAX];
    // get first, last ipv4 addresses
    esp_ip4_addr_t current_target_ip, last_ip;
    current_target_ip.addr = first_ip.addr;
    last_ip.addr = first_ip.addr | (~ip_info.netmask.addr); // calculate last ip in subnet

    ESP_LOGI(TAG,"%" PRIu32 " ipv4 addresses to scan", max_subnet_ip_count);
    esp_ip4addr_ntoa(&last_ip, char_target_ip, IP4ADDR_STRLEN_MAX); // convert IP from network to host
    ESP_LOGI(TAG, "Last ip %s", char_target_ip);
    esp_ip4addr_ntoa(&first_ip, char_target_ip, IP4ADDR_STRLEN_MAX); // convert IP from network to host
    ESP_LOGI(TAG, "First ip %s", char_target_ip);

    // ipv4 online address storage
    ipv4_list * ipv4_list = create_ipv4_list(first_ip.addr, subnet_mask);
    if(ipv4_list == NULL){
        ESP_LOGE(TAG, "arp_scan_full(): Failed to allocate ipv4_list with create_ipv4_list()");
        return NULL;
    }

    first_ip.addr = ntohl(first_ip.addr); //flip it to use in calculations, won't be needed elsewhere

    // scan the entire subnet in batches
    while (current_target_ip.addr != last_ip.addr) {
        esp_ip4_addr_t current_addresses[ARP_BATCH_SIZE]; // save current loop ip
        uint32_t current_arp_count = 0; // for checking ARP table use

        // send ARP request in batches because ARP table has a limited size
        for (int i = 0; i < ARP_BATCH_SIZE; i++) {
            next_ipv4(&current_target_ip.addr); // next ip
            if (current_target_ip.addr != last_ip.addr) {
                // printing some logging things...
                esp_ip4addr_ntoa(&current_target_ip, char_target_ip, IP4ADDR_STRLEN_MAX); // convert IP from network to host
                current_addresses[i] = current_target_ip;
                ESP_LOGI(TAG, "Success sending ARP to %s", char_target_ip);

                // send ARP request
                etharp_request(netif, (ip4_addr_t*)&current_target_ip); // won't send if table contains record already
                current_arp_count++;
            } else {
                break; // ip is last ip in subnet then break
            }
        }

        // Wait for ARP responses
        vTaskDelay(pdMS_TO_TICKS(ARP_TIMEOUT));

        /* Find received ARP responses in ARP table */
        for(int i = 0; i < current_arp_count; i++) {
            ip4_addr_t *ip_ret = NULL;   // To retrieve IP address from ARP table
            struct eth_addr *mac_ret = NULL; // To retrieve MAC address from ARP table

            if (etharp_find_addr(NULL, (ip4_addr_t*)&current_addresses[i].addr, &mac_ret, &ip_ret) != -1) {
                // unsigned int current_arp_count = flip_ipv4_bits(&current_addresses[i].addr) - flip_ipv4_bits(&first_ip.addr) - 1; 
                // calculate current size of ARP table
                uint32_t current_ipv4_count = ntohl(current_addresses[i].addr);
                current_ipv4_count = current_ipv4_count - first_ip.addr - 1;

                // If IP is located in ARP table, retrieve its MAC
                // print MAC result for ip
                print_info(ip_ret, mac_ret);

                add_to_ipv4_list(ipv4_list, ntohl(current_addresses[i].addr), mac_ret->addr);
            }
        }
    }
    // Check the list
    for (int i = 0; i < ipv4_list->size; i++) {
        ipv4_info * info = get_from_ipv4_list_at(ipv4_list, i);
        uint32_t ip = htonl(info->ip);
        esp_ip4addr_ntoa((esp_ip4_addr_t*)&ip, char_target_ip, IP4ADDR_STRLEN_MAX); // convert IP from network to host
        printf("%s\n", char_target_ip);
    }
    ESP_LOGI(TAG, "Scan done. %u devices are on local network", ipv4_list->size);

    return ipv4_list;
}

// get the next ipv4 address
void next_ipv4(uint32_t * ipv4){
    if (ipv4 == NULL) {
        return;
    }
    // reconstruct it to normal order
    uint32_t normal_ipv4 = ntohl(*ipv4); // to host order for math

    // if value is all 1's then its the last ipv4 address in the ipv4 address space, shouldnt really happen
    // else get next ipv4 address
    if (normal_ipv4 == UINT32_MAX) {
        return;
    } else {
        // because of endianess
        normal_ipv4++;
        *ipv4 = htonl(normal_ipv4);     // back to network order
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
    if (ipv4_list == NULL) {
        return NULL;
    }
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

ipv4_info * create_ipv4_info_node(uint32_t ip, uint8_t * mac) {
    if (mac == NULL) {
        return NULL;
    }
    ipv4_info * info_node = calloc(1, sizeof(ipv4_info));
    if (info_node == NULL) {
        ESP_LOGE(TAG, "add_to_ipv4_list(): Failed to allocate next_info next ipv4_info list node");
        return NULL;
    }
    info_node->ip = ip;
    memcpy(info_node->mac, mac, sizeof(uint8_t) * 6);
    return info_node;
}

void add_to_ipv4_list(ipv4_list * ipv4_list, uint32_t ip, uint8_t * mac) {
    if (mac == NULL) {
        return;
    }
    if (ipv4_list == NULL) {
        ESP_LOGE(TAG, "add_to_ipv4_list(): ipv4_list is NULL");
        return;
    }

    if ((ipv4_list->size == 0 && ipv4_list->head != NULL) || (ipv4_list->size > 0 && ipv4_list->head == NULL)) {
        ESP_LOGE(TAG, "add_to_ipv4_list(): ipv4_list size and head pointer don't match. The list is supposed to be empty.");
        return;
    }

    ipv4_info * next_info = create_ipv4_info_node(ip, mac);

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
    if (ipv4_list == NULL) {
        return;
    }

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
