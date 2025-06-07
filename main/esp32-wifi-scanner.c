#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "driver/uart.h"
#include "esp_err.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "esp_netif_types.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "esp_wifi_default.h"
#include "esp_wifi_types.h"
#include "freertos/FreeRTOS.h"
#include "http-server.h"
#include "lv_core/lv_obj.h"
#include "lv_widgets/lv_label.h"
#include "lv_widgets/lv_textarea.h"
#include "lv_widgets/lv_list.h"
#include "lwip/def.h"
#include "lwip/ip4_addr.h"
#include "lwip/timeouts.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "freertos/portmacro.h"
#include "lv_conf_internal.h"
#include "nvs_flash.h"

#include "conf.h"
#include "scanner.h"
#include "port-scanner.h"
#include "file-writing.h"

/*   WiFi   */
#include "esp_wifi.h"

#define WIFI_AUTHMODE WIFI_AUTH_WPA2_PSK
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static size_t found_ap_count = 0;
wifi_ap_record_t current_ap_record;
wifi_ap_record_t ap_records[MAX_SCAN_RESULTS];
#define SIZEOF_AP_LABEL 64
static char ap_label_list[MAX_SCAN_RESULTS][SIZEOF_AP_LABEL];

char ap_password_str[64];
static const int MAX_AP_RETRY_ATTEMPT = 10;
wifi_ap_record_t ap_info;

esp_netif_t * netif = NULL;
esp_netif_dns_info_t dns;
/********/

/* HTTP Server */
static httpd_handle_t server = NULL;
/***************/


/*   LVGL   */
#include "lv_hal/lv_hal_disp.h"
#include "lv_misc/lv_task.h"
#include "lv_font/lv_symbol_def.h"
#include "lvgl_helpers.h"

char line_buf[64*2];

#define LV_TICK_PERIOD_MS 1
#define LV_TASK_HANDLER_MS 15
#define STATUSBAR_PERIOD 1000

#include "gui.h"
#define IPV4_LIST_SIZE 254
static lv_obj_t * wifi_btn_list[MAX_SCAN_RESULTS];
static lv_obj_t * ipv4_scan_btn_list[IPV4_LIST_SIZE];
/************/


/* FreeRTOS */
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

// Wifi
static EventGroupHandle_t wifi_event_group = NULL;
static TaskHandle_t wifi_task_handle = NULL;
static TaskHandle_t time_sync_task_handle = NULL;

// LVGL
static SemaphoreHandle_t xGuiSemaphore;
/************/


/*  Time   */
#include "time.h"
time_t now = 0;
struct tm timeinfo;
char time_str[64];
/***********/

/* lwip */
#define LWIP_TICK_PERIOD_MS 250
/**************/

/* LittleFS */
esp_vfs_littlefs_conf_t lfs_conf = {
    .base_path = "/littlefs",
    .partition_label = "littlefs",      //see partitions.csv name column
    .format_if_mount_failed = true,
    .dont_mount = false,
};
/************/


typedef enum {
    NONE,
    NETWORK_SEARCHING,
    NETWORK_CONNECTING,
    SYNCING_TIME,
    NETWORK_SCANNING,
    NETWORK_CONNECTED,
    NETWORK_CONNECT_FAILED,
    START_SERVE,
    STOP_SERVE,
    SERVING,
} network_status_t;
network_status_t network_status = NONE;

typedef enum {
    NO_SCAN,
    ARP_FULL,
    PORT_FULL,
    PORT_SINGLE,
} scan_type_t;
scan_type_t scan_status = NO_SCAN;
uint32_t current_port_scan_target_index = 0;

ipv4_list * last_ipv4_list = NULL;
uint8_t last_ipv4_port_map[OPEN_PORT_MAP_SIZE];

bool is_sntp_setup = false;

void setup_dns() {
    // set DNS manually for now
    IP4_ADDR(&dns.ip.u_addr.ip4, 1, 1, 1, 1);
    dns.ip.type = IPADDR_TYPE_V4;
    esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns);
}

void setup_sntp() {
    if (is_sntp_setup == true) {
        return;
    }
    is_sntp_setup = true;
    ESP_LOGI("SNTP service", "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_setservername(0, SNTP_SERVER); // default sntp server
    sntp_init();
}

void sync_time() {
    if (network_status != NETWORK_CONNECTED) {
        return;
    }
    network_status = SYNCING_TIME;
    const int max_attempts = 200; //this may not be quick?
    int attempts = 0;
    while (now < 1749221337 && attempts < SNTP_SYNC_ATTEMPTS) {
        ESP_LOGI(TAG, "Attempting to sync time by SNTP %d/%d...", attempts, max_attempts);
        time(&now);
        localtime_r(&now, &timeinfo);
        vTaskDelay(pdMS_TO_TICKS(SNTP_SYNC_ATTEMPT_DELAY));
        attempts++;
    }

    if (now < 1749221337) {
        ESP_LOGW(TAG, "Time sync failed");
    } else {
        ESP_LOGI(TAG, "Time synced to %s", asctime(&timeinfo));
    }
    network_status = NETWORK_CONNECTED;
}

bool update_time_check_if_valid() {
    time(&now);
    if (now < 1749221337) {
        timeinfo = *localtime(&now);
        return false;
    } else {
        timeinfo = *localtime(&now);
        return true;
    }
}

esp_err_t wifi_init(void) {
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize TCP/IP network stack");
        return ret;
    }

    ret = esp_wifi_set_default_wifi_sta_handlers();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set default handlers");
        return ret;
    }

    netif = esp_netif_create_default_wifi_sta();
    if (netif == NULL) {
        ESP_LOGE(TAG, "Failed to create default WiFi STA interface");
        return ESP_FAIL;
    }

    netif = esp_netif_create_default_wifi_ap();
    if (netif == NULL) {
        ESP_LOGE(TAG, "Failed to create default WiFi AP interface");
        return ESP_FAIL;
    }

    // Wi-Fi stack configuration parameters
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ret;
}

void wifi_start_ap(void) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .password = AP_PASSWORD,
            .max_connection = AP_MAX_CONNECTIONS,
            .authmode = AP_AUTH_MODE,
        },
    };

    if (strlen(AP_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi started as AP SSID: %s Password: %s", AP_SSID, AP_PASSWORD);
}

esp_ip4_addr_t get_ap_ip() {
    esp_netif_ip_info_t ip_info;
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (netif == NULL) {
        ESP_LOGE(TAG, "Failed to get netif handle for AP");
    }
    esp_netif_get_ip_info(netif, &ip_info);
    return ip_info.gw;
}

void time_sync_task(void * arg) {
    (void)arg;
    setup_dns();
    setup_sntp();
    sync_time();
    vTaskDelete(NULL);
}

int wifi_connect(const char * wifi_password) {
    int status = WIFI_FAIL_BIT;

    wifi_config_t wifi_cfg = {
        .sta = {
            // this sets the weakest authmode accepted in fast scan mode (default)
            .threshold.authmode = WIFI_AUTH_OPEN,
                .pmf_cfg = {
                .capable = true,
                .required = false
            }
        },
    };
    strncpy((char*)wifi_cfg.sta.ssid, (char*)current_ap_record.ssid, sizeof(wifi_cfg.sta.ssid));
    strncpy((char*)wifi_cfg.sta.password, wifi_password, sizeof(wifi_cfg.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Connecting to Wi-Fi network %s ...", wifi_cfg.sta.ssid);

    ESP_LOGI(TAG, "Attempting to connect to AP...");
    esp_wifi_disconnect();
    memset(&ap_info, 0, sizeof(ap_info));
    esp_wifi_connect();
    for (int wifi_retry_count = 1; wifi_retry_count <= MAX_AP_RETRY_ATTEMPT; wifi_retry_count++) {
        if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
            ESP_LOGI(TAG, "Reconnecting to AP...");
            esp_wifi_connect();
        }
    }

    esp_netif_t * netif;
    esp_netif_ip_info_t netif_ip_info;
    uint8_t attempts = 0;
    while (attempts < 50) {
        vTaskDelay(pdMS_TO_TICKS(250));
        netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK && esp_netif_get_ip_info(netif, &netif_ip_info) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully IP from AP");
            status = WIFI_CONNECTED_BIT;
            network_status = NETWORK_CONNECTED;

            if (time_sync_task_handle == NULL) {
                xTaskCreate(time_sync_task, "time_sync_task", 2048, NULL, 10, &time_sync_task_handle);
            }

            return status;
        }
        ESP_LOGI(TAG, "Waiting for IP from AP... (%u)", attempts);
        attempts++;
    }

    ESP_LOGE(TAG, "Failed to acquire IP from AP");
    status = WIFI_FAIL_BIT;

    return status;
}

static esp_err_t wifi_disconnect(void) {
    if (wifi_event_group) {
        vEventGroupDelete(wifi_event_group);
    }

    return esp_wifi_disconnect();
}

esp_err_t wifi_search_ap_list(void) {
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Start scanning
    found_ap_count = MAX_AP_RETRY_ATTEMPT;
    esp_err_t ret = esp_wifi_scan_start(NULL, true);
    if (ret != ESP_OK) {
        printf("wifi_scan() - esp_wifi_scan_start(): Failed to start scan: %s\n", esp_err_to_name(ret));
        found_ap_count = 0;
        return ret;
    }

    // Get scan results
    ret = esp_wifi_scan_get_ap_records(&found_ap_count, ap_records);
    if (ret != ESP_OK) {
        printf("wifi_scan(): Failed to get wifi scan results: %s\n", esp_err_to_name(ret));
        found_ap_count = 0;
        return ret;
    }

    // Print scan results
    printf("Found %zu access points:\n", found_ap_count);
    for (int i = 0; i < found_ap_count; i++) {
        printf("SSID: %s, RSSI: %d\n", ap_records[i].ssid, ap_records[i].rssi);
    }
    return ret;
}

static void lv_timer(void *arg) {
    (void)arg;
    lv_tick_inc(LV_TICK_PERIOD_MS);
}

static void lwip_timer_handle(void *pv) {
    while (1) {
        sys_check_timeouts(); // lwip timer
        vTaskDelay(pdMS_TO_TICKS(LWIP_TICK_PERIOD_MS));
    }
}

static void wifi_list_btn_cb(lv_obj_t * obj, lv_event_t event) {
    if (event != LV_EVENT_CLICKED) {
        return;
    }
    for (int i = 0; i < MAX_SCAN_RESULTS; i++) {
        if (wifi_btn_list[i] == obj) {
            // Set the popup label to the SSID of the clicked AP
            current_ap_record = ap_records[i];
            lv_label_set_text_fmt(wifi_popup_title_label, "%s", ap_records[i].ssid);
            lv_obj_move_foreground(wifi_popup);

            break;
        }
    }
}

static void ipv4_scan_list_btn_cb(lv_obj_t * obj, lv_event_t event) {
    if (scan_status != NO_SCAN) {
        return;
    }
    if (event != LV_EVENT_CLICKED) {
        return;
    }
    network_status = NETWORK_SCANNING;
    scan_status = PORT_SINGLE;
    for (int i = 0; i < IPV4_LIST_SIZE; i++) {
        if (ipv4_scan_btn_list[i] == obj) {
            current_port_scan_target_index = i;
            break;
        }
    }
}

static void show_found_wifi_list() {
    if (found_ap_count <= 0) {
        if( xSemaphoreTake( xGuiSemaphore, portMAX_DELAY) == pdTRUE ) {
            lv_list_clean(wifi_list);
            lv_obj_t * btn = lv_list_add_btn(wifi_list, LV_SYMBOL_WARNING, "No networks found!");
            wifi_btn_list[0] = btn;
            xSemaphoreGive(xGuiSemaphore);
        }
        return;
    }

    for (int i = 0; i < found_ap_count && i < 32; i++) {
        const char *auth_str = (ap_records[i].authmode == WIFI_AUTH_OPEN) ? "open" : "locked";
        snprintf(ap_label_list[i], SIZEOF_AP_LABEL, "%s    (RSSI: %d AUTH: %s)", (char *)ap_records[i].ssid, ap_records[i].rssi, auth_str);
    }

    if (xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
        lv_list_clean(wifi_list);
        for (int i = 0; i < found_ap_count && i < 32; i++) {
            lv_obj_t *btn = lv_list_add_btn(wifi_list, NULL, ap_label_list[i]);

            lv_obj_t *label = lv_obj_get_child(btn, NULL);
            lv_obj_set_style_local_text_font(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &lv_font_montserrat_12);
            lv_obj_set_height(btn, 12);

            wifi_btn_list[i] = btn;
            lv_obj_set_event_cb(btn, wifi_list_btn_cb);
        }

        xSemaphoreGive(xGuiSemaphore);
    }
}

static void show_found_ip_list() {
    if (last_ipv4_list == NULL) {
        return;
    }

    if (last_ipv4_list->size <= 0) {
        if ( xSemaphoreTake( xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
            lv_list_clean(ipv4_scan_list);
            lv_obj_t *btn = lv_list_add_btn(ipv4_scan_list, LV_SYMBOL_WARNING, "No online IP's found!");
            ipv4_scan_btn_list[0] = btn;
            xSemaphoreGive(xGuiSemaphore);
        }
        return;
    }

    if (xSemaphoreTake( xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
        lv_list_clean(ipv4_scan_list);
        xSemaphoreGive(xGuiSemaphore);
    }
    char ip_string_buf[IP4ADDR_STRLEN_MAX];
    for (int i = 0; i < last_ipv4_list->size; i++) {
        ipv4_info * current_ipv4_info = get_from_ipv4_list_at(last_ipv4_list, i);

        uint32_t ip = htonl(current_ipv4_info->ip);
        esp_ip4addr_ntoa((esp_ip4_addr_t*)&ip, ip_string_buf, IP4ADDR_STRLEN_MAX);
        printf("Displaying ipv4 device %i with IP: %s\n", i, ip_string_buf);

        if (xSemaphoreTake( xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
            lv_obj_t *btn = lv_list_add_btn(ipv4_scan_list, NULL, ip_string_buf);
            ipv4_scan_btn_list[i] = btn;
            lv_obj_set_event_cb(btn, ipv4_scan_list_btn_cb);
            xSemaphoreGive(xGuiSemaphore);
        }
    }

    return;
}

void wifi_scan_event_handler(lv_obj_t * obj, lv_event_t event) {
    wifi_search_ap_list();
}

void wifi_task(void *arg) {
    while(1) {
        if (network_status == SERVING) {
            // ...
        } else if (network_status == NETWORK_CONNECTED) {
            // ...
        } else if (network_status == START_SERVE) {
            wifi_start_ap();

            vTaskDelay(100);

            uint32_t ip = get_ap_ip().addr;
            char * ip_str = ip4addr_ntoa((ip4_addr_t*)&ip);
            ESP_LOGI(TAG, "AP gateway ip is %s", ip_str);

            if (xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
                lv_label_set_text_fmt(server_http_ip, "IP: %s", ip_str);
                xSemaphoreGive(xGuiSemaphore);
            }

            server = start_webserver();

            network_status = SERVING;
        } else if (network_status == STOP_SERVE) {
            if (server != NULL) {
                stop_webserver(server);
            }
            network_status = NETWORK_SEARCHING;
        } else if (network_status == NETWORK_SCANNING) {
            if (scan_status == ARP_FULL) {
                last_ipv4_list = arp_scan_full();
                show_found_ip_list();

                // Write to file
                update_time_check_if_valid();
                record_ipv4_list_data_to_file(now, last_ipv4_list);
            }
            if (scan_status == PORT_SINGLE) {
                ipv4_info * last_ipv4_info = get_from_ipv4_list_at(last_ipv4_list, current_port_scan_target_index);
                uint32_t ip = htonl(last_ipv4_info->ip);
                ESP_LOGI(TAG, "Starting port scan for target %s", ip4addr_ntoa((ip4_addr_t*)&ip));
                scan_ports(*(esp_ip4_addr_t*)&ip, last_ipv4_port_map);

                // Write to file
                update_time_check_if_valid();
                record_single_port_data_to_file(now, last_ipv4_info, last_ipv4_port_map);
            }
            if (scan_status == PORT_FULL) {
                //last_ipv4_list = arp_scan_full();
                //show_found_ip_list();
                //...
            }
            scan_status = NO_SCAN;
            network_status = NETWORK_CONNECTED;
        } else if (network_status == NETWORK_CONNECTING) {
            if (wifi_connect((char*)lv_textarea_get_text(wifi_popup_password_textarea)) == WIFI_CONNECTED_BIT) {
                // Successful connection to AP
                network_status = NETWORK_CONNECTED;
                if (xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
                    lv_obj_move_background(wifi_popup);
                    xSemaphoreGive(xGuiSemaphore);
                }
            } else {
                // Failed to connect to AP
                network_status = NETWORK_CONNECT_FAILED;
                if (xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
                    show_popup_msg_box("Error", "WiFi connection failed!");
                    xSemaphoreGive(xGuiSemaphore);
                }
                network_status = NETWORK_SEARCHING;
            }
        } else {
            network_status = NETWORK_SEARCHING;
            wifi_search_ap_list();

            // Write to file
            update_time_check_if_valid();
            record_ap_records_data_to_file(now, ap_records, found_ap_count);

            show_found_wifi_list();
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void create_wifi_task() {
    network_status = NETWORK_SEARCHING;
    xTaskCreatePinnedToCore(wifi_task, "wifi_task", 4096 * 4, NULL, 4, &wifi_task_handle, 0);
}

void wifi_connect_wifi_event_cb(lv_obj_t * obj, lv_event_t event) {
    network_status = NETWORK_CONNECTING;
}

void arp_full_scan_btn_cb(lv_obj_t * obj, lv_event_t event) {
    if (scan_status != NO_SCAN) {
        return;
    }
    if (network_status != NETWORK_CONNECTED) {
        ESP_LOGI(TAG, "device_arp_scan_btn_cb(): Can't begin scan. Not connected to network!");
        return;
    }
    network_status = NETWORK_SCANNING;
    scan_status = ARP_FULL;
}

void serve_switch_cb(lv_obj_t * obj, lv_event_t event) {
    if (event == LV_EVENT_VALUE_CHANGED) {
        bool state = lv_switch_get_state(obj);
        if (state == true) {
            network_status = START_SERVE;
        } else {
            ESP_LOGI(TAG, "Switch is OFF");
            network_status = NETWORK_SEARCHING;
        }
    }
}

const char * statusbar_dots_from_state(uint8_t * state) {
    if (*state == 1) {
        return "";
    } else if (*state == 2) {
        return ".";
    } else if (*state == 3) {
        return "..";
    } else if (*state == 0) {
        return "...";
    }
    return "";
}

void update_label(lv_obj_t *label, const char *str1, const char *str2, const char *str3) {
    if (str1 == NULL) {
        return;
    }
    static char prev_text[64];
    static char next_text[64];

    snprintf(next_text, sizeof(next_text), "%s%s%s", str1, str2 ? str2 : "", str3 ? str3 : "");

    if (strcmp(next_text, prev_text) != 0) {
        if (xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
            lv_label_set_text(label, next_text);
            xSemaphoreGive(xGuiSemaphore);
            strncpy(prev_text, next_text, sizeof(prev_text));
        }
    }
}

void statusbar_task(void *args) {
    (void)args;
    uint8_t state = 0;
    while (1) {
        if (network_status == NETWORK_CONNECTED) {
            update_label(status_label, "N: ", (char*)current_ap_record.ssid, NULL);
        } else if (network_status == NETWORK_CONNECTING) {
            update_label(status_label, "N: CONNECTING", (char*)statusbar_dots_from_state(&state), NULL);
        } else if (network_status == SERVING) {
            update_label(status_label, "AP: SERVING", NULL, NULL);
        } else if (network_status == START_SERVE) {
            update_label(status_label, "AP: STARTING SRV", NULL, NULL);
        } else if (network_status == STOP_SERVE) {
            update_label(status_label, "AP: STOPPING SRV", NULL, NULL);
        } else if (network_status == NETWORK_SEARCHING) {
            update_label(status_label, "N: SEARCHING", (char*)statusbar_dots_from_state(&state), NULL);
        } else if (network_status == NETWORK_SCANNING) {
            if (scan_status == ARP_FULL) {
                update_label(status_label, "SCAN: ARP FULL ", (char*)statusbar_dots_from_state(&state), NULL);
            } else if (scan_status == PORT_SINGLE) {
                uint32_t ip = htonl(get_from_ipv4_list_at(last_ipv4_list, current_port_scan_target_index)->ip);
                update_label(status_label, "SCAN: PORT  ", ip4addr_ntoa((ip4_addr_t*)&ip), (char*)statusbar_dots_from_state(&state));
            }
        } else if (network_status == NONE) {
            update_label(status_label, "DOWN/ERR", NULL, NULL);
        }
        update_time_check_if_valid();
        if (network_status == SYNCING_TIME) {
            if (xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
                update_label(time_label, "...", (char*)statusbar_dots_from_state(&state), NULL);
                xSemaphoreGive(xGuiSemaphore);
            }
        } else {
            if (xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
                lv_label_set_text_fmt(time_label, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
                xSemaphoreGive(xGuiSemaphore);
            }
        }
        state++;
        state = state % 4; //4 states...
        vTaskDelay(pdMS_TO_TICKS(STATUSBAR_PERIOD));
    }
}

void gui_task(void *pvParameter) {
    xGuiSemaphore = xSemaphoreCreateMutex();

    // Initialize LVGL
    lv_init();

    // Initialize SPI andif (etharp_find_addr(NULL, (ip4_addr_t*)&current_addresses[i], &eth_ret, &ipaddr_ret) != -1) { display, touch drivers among other LVGL things
    lvgl_driver_init();

    const esp_timer_create_args_t lv_periodic_timer_args = {
        .callback = &lv_timer,
        .name = "lv_timer",
    };
    esp_timer_handle_t lv_periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&lv_periodic_timer_args, &lv_periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lv_periodic_timer, pdMS_TO_TICKS(LV_TICK_PERIOD_MS)));

    // Setting up display
    static lv_disp_buf_t draw_buf;
    static lv_color_t buf[LV_HOR_RES_MAX * LV_VER_RES_MAX / 10];                     /*Declare a buffer for 1/10 screen size*/
    lv_disp_buf_init(&draw_buf, buf, NULL, LV_HOR_RES_MAX * LV_VER_RES_MAX / 10);    /*Initialize the display buffer*/

    static lv_disp_drv_t disp_drv;             /*Descriptor of a display driver*/
    lv_disp_drv_init(&disp_drv);               /*Basic initialization*/
    disp_drv.flush_cb = disp_driver_flush;     /*Set your driver function*/
    disp_drv.buffer = &draw_buf;               /*Assign the buffer to the display*/
    disp_drv.hor_res = 320;
    disp_drv.ver_res = 240;
    lv_disp_drv_register(&disp_drv);           /*Finally register the driver*/

    // Setting up touch input
    static lv_indev_drv_t indev_drv;           /*Descriptor of a input device driver*/
    lv_indev_drv_init(&indev_drv);             /*Basic initialization*/
    indev_drv.type = LV_INDEV_TYPE_POINTER;    /*Touch pad is a pointer-like device*/
    indev_drv.read_cb = touch_driver_read;     /*Set your driver function*/
    lv_indev_drv_register(&indev_drv);         /*Finally register the driver*/

    // Building LVGL gui objects
    init_style();
    build_gui();
    build_statusbar();

    make_keyboard();

    build_wifi_connection_box();
    lv_obj_move_background(wifi_popup);

    // Setting event handlers for GUI objects...
    lv_obj_set_event_cb(wifi_popup_connect_btn, wifi_connect_wifi_event_cb);
    lv_obj_set_event_cb(ipv4_scan_btn, arp_full_scan_btn_cb);
    lv_obj_set_event_cb(serve_switch, serve_switch_cb);

    xTaskCreate(statusbar_task, "statusbar_task", 2048, NULL, 0, NULL);

    // This part should run indefinitely all the time
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(LV_TASK_HANDLER_MS));
        // Try to take the semaphore and call lv_task_handler()
        if (xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
    }

    // This should not happen
    vTaskDelete(NULL);
}

void app_main(void) {
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Initialize LittleFS
    err = esp_vfs_littlefs_register(&lfs_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount LittleFS: (%s)", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "LittleFS mounted at %s", lfs_conf.base_path);

    // Initialize network interface, TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Initialize the event loop (i think this is somewhat broken)
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize Wi-FI
    ESP_ERROR_CHECK(wifi_init());

    // Lwip sys_check_timeouts doesn't belong in a esp timer...
    xTaskCreate(lwip_timer_handle, "lwip_timer", 2048, NULL, 5, NULL);

    // Pin gui drawing task to CPU Core 1. Core 1 just for GUI.
    xTaskCreatePinnedToCore(gui_task, "gui_task", 4096 * 4, NULL, 1, NULL, 1);

    // Create a task for for Wi-Fi networks
    create_wifi_task();
}
