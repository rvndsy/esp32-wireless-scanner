#include "conf.h"
#include "file-writing.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/ip4_addr.h"
#include "esp_netif_ip_addr.h"
#include "net-scanner.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define TAG "File_writing"

const char port_root_dir[] = "/littlefs/ports/";
const char ipv4_info_root_dir[] = "/littlefs/ipv4_info/";
const char ap_records_root_dir[] = "/littlefs/ap_records/";
const char extension[] = ".txt";
#define SIZEOF_TIME_STR 20

void ensure_directory_exists(const char * path) {
    if (path == NULL) {
        return;
    }
    struct stat status = {0};
    if (stat(path, &status) == -1) {
        ESP_LOGI(TAG, "Creating directory %s...", path);
        mkdir(path, 0777); // permissions are ignored in LittleFS
    }
}

void iterate_bitmap_bits(const uint8_t * port_map, uint32_t max_port) {
    if (port_map == NULL) {
        ESP_LOGE(TAG, "iterate_bitmap_bits(): port_map is NULL");
        return;
    }
    printf("%u\n", max_port);
    uint16_t num_bytes = (max_port + 7) / 8;
    for (size_t byte_index = 0; byte_index < num_bytes; byte_index++) {
        uint8_t byte = port_map[byte_index];
        for (int bit_index = 0; bit_index < 8; bit_index++) {  // LSB first
            size_t bit_position = byte_index * 8 + bit_index;
            if (bit_position >= max_port) {
                return;
            }
            if ((byte >> bit_index) & 1) {
                printf("TCP Port %zu is up\n", bit_position);
            } else {
                //printf("TCP Port %zu is down\n", bit_position);
            }
        }
    }
}

const char *auth_mode_to_string(wifi_auth_mode_t mode) {
    switch (mode) {
        case 0:   return "OPEN";
        case 1:   return "WEP";
        case 2:   return "WPA_PSK";
        case 3:   return "WPA2_PSK";
        case 4:   return "WPA_WPA2_PSK";
        // case 5 and 6 are WIFI_AUTH_WPA2_ENTERPRISE == WIFI_AUTH_ENTERPRISE respectively
        case 6:   return "WPA2_ENTERPRISE";
        case 7:   return "WPA3_PSK";
        case 8:   return "WPA2_WPA3_PSK";
        case 9:   return "WAPI_PSK";
        case 10:  return "WPA3_ENT_192";
        default:  return "UNKNOWN";
    }
}

void write_timestamp_string_to_file(const struct tm * time_info, FILE * fptr) {
    if (time_info->tm_year >= (2025 - 1900)) {
        char timestamp_str[23];
        strftime(timestamp_str, 23*sizeof(char), "%Y-%m-%d %H:%M UTC+0", time_info);
        fprintf(fptr, "%s\n", timestamp_str);
    } else {
        fprintf(fptr, "NO TIME DATA\n");
    }
}

void format_uint32t_ip_into_str(uint32_t ip, char * ip_str) {
    uint32_t ip_n = htonl(ip);
    esp_ip4addr_ntoa((esp_ip4_addr_t*)&ip_n, ip_str, IP4ADDR_STRLEN_MAX);
}


// Top 1000 port scan example from nmap
/*
2025-06-06 21:26 UTC+2
Nmap scan report for 192.168.1.66
Host is up
Not shown: 997 closed tcp ports
PORT     STATE SERVICE
80/tcp   open  http
554/tcp  open  rtsp
1935/tcp open  rtmp
MAC Address: 00:11:22:AA:BB:CC (Unknown)

Nmap done: 1 IP address (1 host up) scanned in 2.38 seconds
*/

// TODO: SERVICE column (a single switch statement) and MAC address lookup for vendor?

err_t record_single_port_data_to_file(const time_t time, ipv4_info * ipv4_info, uint8_t * ports) {
    if (ipv4_info == NULL || ports == NULL) {
        return ESP_FAIL;
    }

    struct tm * time_info = localtime(&time);
    char file_path[strlen(port_root_dir)+SIZEOF_TIME_STR+strlen(extension)];

    ensure_directory_exists(port_root_dir);

    strcpy(file_path, port_root_dir);
    strftime(file_path+strlen(port_root_dir), sizeof(file_path), "%Y-%m-%d-%H-%M-%S", time_info);
    strcpy(file_path+strlen(port_root_dir)+SIZEOF_TIME_STR-1, extension);

    ESP_LOGI(TAG, "Writing port data to file %s", file_path);

    FILE * fptr = fopen(file_path, "w");
    if (fptr == NULL) {
        ESP_LOGI(TAG, "Failed to open file for port data writing %s", file_path);
        return ESP_FAIL;
    }

    // Write the date/time
    write_timestamp_string_to_file(time_info, fptr);

    // Header
    char ip_str[IP4ADDR_STRLEN_MAX];
    format_uint32t_ip_into_str(ipv4_info->ip, ip_str);
    fprintf(fptr, "Scan report for %s\n", ip_str);
    fprintf(fptr, "Host is up\n");

    // Count closed ports
    int closed_ports = 0;
    for (uint16_t port = 0; port < MAX_PORT; port++) {
        uint8_t byte = ports[port / 8];
        if (!((byte >> (port % 8)) & 1)) {
            closed_ports++;
        }
    }
    fprintf(fptr, "Not shown: %d closed tcp ports\n", closed_ports);

    // Write open ports
    //fprintf(fptr, "PORT     STATE SERVICE\n");
    if (closed_ports != MAX_PORT) {
        fprintf(fptr, "PORT     STATE\n");
        for (uint16_t port = 0; port < MAX_PORT; port++) {
            uint8_t byte = ports[port / 8];
            if ((byte >> (port % 8)) & 1) {
                //fprintf(fptr, "%-8d open  %d\n", port, resolve_service_name(port));
                fprintf(fptr, "%-8d open\n", port);
            }
        }
    } else {
        fprintf(fptr, "All %d scanned ports on %s are in ignored states.\n", closed_ports, ip_str);
    }

    // 5. Write MAC address
    fprintf(fptr, "MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", ipv4_info->mac[0], ipv4_info->mac[1], ipv4_info->mac[2], ipv4_info->mac[3], ipv4_info->mac[4], ipv4_info->mac[5]);

    // TODO: Maybe ARP check if host is up????? For now i dont want to inflate file size too much
    //fprintf(fptr, "Done: 1 IP address (1 host up)");

    fclose(fptr);

    fptr = fopen(file_path, "r");
    if (fptr) {
        char buf[64];
        while (fgets(buf, sizeof(buf), fptr)) {
            printf("%s", buf); // This sends to Serial monitor
        }
        fclose(fptr);
    }

    return ESP_OK;
}

/*
Starting Nmap 7.95 ( https://nmap.org ) at 2025-06-06 22:00 EEST
Nmap scan report for 192.168.58.84
Host is up (0.000077s latency).
Nmap scan report for 192.168.58.154
Host is up (0.053s latency).
Nmap done: 256 IP addresses (2 hosts up) scanned in 9.62 seconds
*/

err_t record_ipv4_list_data_to_file(const time_t time, ipv4_list * ipv4_list) {
    if (ipv4_list == NULL) {
        return ESP_FAIL;
    }

    struct tm * time_info = localtime(&time);
    char file_path[strlen(ipv4_info_root_dir)+SIZEOF_TIME_STR+strlen(extension)];

    ensure_directory_exists(ipv4_info_root_dir);

    strcpy(file_path, ipv4_info_root_dir);
    strftime(file_path+strlen(ipv4_info_root_dir), sizeof(file_path), "%Y-%m-%d-%H-%M-%S", time_info);
    strcpy(file_path+strlen(ipv4_info_root_dir)+SIZEOF_TIME_STR-1, extension);

    ESP_LOGI(TAG, "Writing ipv4_list data to file %s", file_path);

    FILE * fptr = fopen(file_path, "w");
    if (fptr == NULL) {
        ESP_LOGI(TAG, "Failed to open file for ipv4_list data writing %s", file_path);
        return ESP_FAIL;
    }

    // Write the date/time
    write_timestamp_string_to_file(time_info, fptr);

    char ip_str[IP4ADDR_STRLEN_MAX];
    ipv4_info * ipv4_info;
    for (uint32_t i = 0; i < ipv4_list->size; i++) {
        ipv4_info = get_from_ipv4_list_at(ipv4_list, i);
        format_uint32t_ip_into_str(ipv4_info->ip, ip_str);
        fprintf(fptr, "Report for %s\n", ip_str);
        fprintf(fptr, "MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", ipv4_info->mac[0], ipv4_info->mac[1], ipv4_info->mac[2], ipv4_info->mac[3], ipv4_info->mac[4], ipv4_info->mac[5]);
        fprintf(fptr, "Host is up\n");
    }

    uint32_t ip_count = UINT32_MAX - ipv4_list->subnet - 1; // the total count of ips to scan
    fprintf(fptr, "Done: %u IP addresses (%u hosts up)\n", ip_count, ipv4_list->size);

    fclose(fptr);

    fptr = fopen(file_path, "r");
    if (fptr) {
        char buf[64];
        while (fgets(buf, sizeof(buf), fptr)) {
            printf("%s", buf); // This sends to Serial monitor
        }
        fclose(fptr);
    }

    return ESP_OK;
}

err_t record_ap_records_data_to_file(const time_t time, wifi_ap_record_t * ap_records, size_t ap_record_count) {
    if (ap_records == NULL) {
        return ESP_FAIL;
    }

    struct tm * time_info = localtime(&time);
    char file_path[strlen(ap_records_root_dir)+SIZEOF_TIME_STR+strlen(extension)];

    ensure_directory_exists(ap_records_root_dir);

    strcpy(file_path, ap_records_root_dir);
    strftime(file_path+strlen(ap_records_root_dir), sizeof(file_path), "%Y-%m-%d-%H-%M-%S", time_info);
    strcpy(file_path+strlen(ap_records_root_dir)+SIZEOF_TIME_STR-1, extension);

    ESP_LOGI(TAG, "Writing ap_records data to file %s", file_path);

    FILE * fptr = fopen(file_path, "w");
    if (fptr == NULL) {
        ESP_LOGI(TAG, "Failed to open file for ap_records data writing %s", file_path);
        return ESP_FAIL;
    }

    // Write the date/time
    write_timestamp_string_to_file(time_info, fptr);

    // Writing these values for now...
    // uint8_t ssid[33];                     /**< SSID of AP */
    // uint8_t primary;                      /**< channel of AP */
    // int8_t  rssi;                         /**< signal strength of AP. Note that in some rare cases where signal strength is very strong, rssi values can be slightly positive */
    // wifi_auth_mode_t authmode;            /**< authmode of AP */
    // uint8_t bssid[6];                     /**< MAC address of AP */

    // Maybe?
    // wifi_cipher_type_t pairwise_cipher;   /**< pairwise cipher of AP */
    // wifi_cipher_type_t group_cipher;      /**< group cipher of AP */

    fprintf(fptr, "SSID                            CH  RSSI AUTH            BSSID(MAC)         \n");
    for (size_t i = 0; i < ap_record_count; i++) {
        fprintf(fptr, "%-32s", ap_records[i].ssid);
        fprintf(fptr, "%-4i", ap_records[i].primary);
        fprintf(fptr, "%-5i", ap_records[i].rssi);
        fprintf(fptr, "%-16s", auth_mode_to_string(ap_records[i].authmode));
        fprintf(fptr, "%02X:%02X:%02X:%02X:%02X:%02X\n", ap_records[i].bssid[0], ap_records[i].bssid[1], ap_records[i].bssid[2], ap_records[i].bssid[3], ap_records[i].bssid[4], ap_records[i].bssid[5]);

    }
    fprintf(fptr, "Done: %zu Access Points up\n", ap_record_count);

    fclose(fptr);

    fptr = fopen(file_path, "r");
    if (fptr) {
        char buf[64];
        while (fgets(buf, sizeof(buf), fptr)) {
            printf("%s", buf); // This sends to Serial monitor
        }
        fclose(fptr);
    }

    return ESP_OK;
}

