#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "esp_timer.h"
#include "esp_wifi_default.h"
#include "esp_wifi_types.h"
#include "freertos/FreeRTOS.h"
#include "lv_core/lv_obj.h"
#include "lv_widgets/lv_tabview.h"
#include "lv_widgets/lv_textarea.h"
#include "lv_widgets/lv_list.h"
#include "lwip/timeouts.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "freertos/portmacro.h"
#include "lv_conf_internal.h"
#include "nvs_flash.h"

#include "conf.h"
#include "scanner.h"
#include "port-scanner.h"

/*   WiFi   */
#include "esp_wifi.h"

#define WIFI_AUTHMODE WIFI_AUTH_WPA2_PSK
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

#define MAX_SCAN_RESULTS 20
static uint16_t found_wifi_count = 0;
static uint8_t current_ap_record_index = 0;
wifi_ap_record_t ap_records[MAX_SCAN_RESULTS];

char ap_password_str[64];
static const int MAX_WIFI_RETRY_ATTEMPT = 10;
static int wifi_retry_count = 0;
wifi_ap_record_t ap_info;

esp_netif_t * netif = NULL;
/********/


/*   LVGL   */
#include "lv_hal/lv_hal_disp.h"
#include "lv_misc/lv_task.h"
#include "lv_font/lv_symbol_def.h"
#include "lvgl_helpers.h"

char line_buf[64*2];

#define LV_TICK_PERIOD_MS 1

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

// LVGL
static SemaphoreHandle_t xGuiSemaphore;
/************/


/*  Time   */
#include "time.h"
time_t now;
char strftime_buf[64];
struct tm timeinfo;
/***********/

/* VFS / lwip */
#include "vfs_lwip.h"
#include "lwip/init.h"

void init_vfs() {
    esp_vfs_lwip_sockets_register(); //self-error checks?
    return;
}

#define LWIP_TICK_PERIOD_MS 250
/**************/



typedef enum {
    NONE,
    NETWORK_SEARCHING,
    NETWORK_CONNECTING,
    NETWORK_SCANNING,
    NETWORK_CONNECTED,
    NETWORK_CONNECT_FAILED,
} Network_Status_t;
Network_Status_t network_status = NONE;
Network_Status_t prev_network_status = NONE;

ipv4_list * current_ipv4_list = NULL;

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

    // Wi-Fi stack configuration parameters
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ret;
}

int wifi_connect(const char* wifi_password) {
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
    strncpy((char*)wifi_cfg.sta.ssid, (char*)ap_records[current_ap_record_index].ssid, sizeof(wifi_cfg.sta.ssid));
    strncpy((char*)wifi_cfg.sta.password, wifi_password, sizeof(wifi_cfg.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Connecting to Wi-Fi network %s ...", wifi_cfg.sta.ssid);

    ESP_LOGI(TAG, "Attempting to connect to AP...");
    esp_wifi_disconnect();
    memset(&ap_info, 0, sizeof(ap_info));
    esp_wifi_connect();
    for (int wifi_retry_count = 1; wifi_retry_count <= MAX_WIFI_RETRY_ATTEMPT; wifi_retry_count++) {
        if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
            ESP_LOGI(TAG, "Reconnecting to AP...");
            esp_wifi_connect();
        }
    }
    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to AP");
        status = WIFI_FAIL_BIT;
    } else {
        ESP_LOGI(TAG, "Successfully connected to AP");
        status = WIFI_CONNECTED_BIT;
        network_status = NETWORK_CONNECTED;
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay because WiFi connection is still setting up...

        time(&now);
        setenv("TZ", "GMT-2", 1);
        tzset();

        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

    }

    return status;
}

static esp_err_t wifi_disconnect(void) {
    if (wifi_event_group) {
        vEventGroupDelete(wifi_event_group);
    }

    return esp_wifi_disconnect();
}

esp_err_t wifi_scan(void) {
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Start scanning
    found_wifi_count = MAX_WIFI_RETRY_ATTEMPT;
    esp_err_t ret = esp_wifi_scan_start(NULL, true);
    if (ret != ESP_OK) {
        printf("wifi_scan() - esp_wifi_scan_start(): Failed to start scan: %s\n", esp_err_to_name(ret));
        found_wifi_count = 0;
        return ret;
    }

    // Get scan results
    ret = esp_wifi_scan_get_ap_records(&found_wifi_count, ap_records);
    if (ret != ESP_OK) {
        printf("wifi_scan(): Failed to get wifi scan results: %s\n", esp_err_to_name(ret));
        found_wifi_count = 0;
        return ret;
    }

    // Print scan results
    printf("Found %d access points:\n", found_wifi_count);
    for (int i = 0; i < found_wifi_count; i++) {
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

static void wifi_list_event_handler(lv_obj_t * obj, lv_event_t event) {
    for (int i = 0; i < MAX_SCAN_RESULTS; i++) {
        if (wifi_btn_list[i] == obj) { //what could go wrong
            lv_label_set_text_fmt(wifi_popup_title_label, "%s", ap_records[i].ssid);
            lv_obj_move_foreground(wifi_popup_connect);
            current_ap_record_index = i;
            break;
        }
    }
}

static void ipv4_scan_list_event_handler(lv_obj_t * obj, lv_event_t event) {

    return;

//  for (int i = 0; i < MAX_SCAN_RESULTS; i++) {
//      if (wifi_btn_list[i] == obj) { //what could go wrong
//          lv_label_set_text_fmt(mbox_title_label, "%s", ap_records[i].ssid);
//          lv_obj_move_foreground(mbox_connect);
//          current_ap_record_index = i;
//          break;
//      }
//  }
}

static void show_found_wifi_list() {
    if (found_wifi_count <= 0) {
        if( xSemaphoreTake( xGuiSemaphore, portMAX_DELAY) == pdTRUE ) {
            lv_list_clean(wifi_list);
            lv_obj_t *btn = lv_list_add_btn(wifi_list, LV_SYMBOL_WARNING, "No networks found!");
            wifi_btn_list[0] = btn;
            xSemaphoreGive(xGuiSemaphore);
        }
        return;
    }

    if( xSemaphoreTake( xGuiSemaphore, portMAX_DELAY) == pdTRUE ) {
        lv_list_clean(wifi_list);
        for (int i = 0; i < found_wifi_count; i++) {
            printf("Displaying wifi %i with ssid: %s\n", i, (char*)ap_records[i].ssid);
            lv_obj_t *btn = lv_list_add_btn(wifi_list, LV_SYMBOL_WIFI, (char*)ap_records[i].ssid);
            wifi_btn_list[i] = btn;
            //printf(line_buf, "%s %-32.32s %4d %2d %s\n", LV_SYMBOL_WIFI, ap_records[i].ssid, ap_records[i].rssi, ap_records[i].primary, enc_buf);
            lv_obj_set_event_cb(btn, wifi_list_event_handler);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        xSemaphoreGive(xGuiSemaphore);
    }
}

static void show_found_ip_list() {
    if (current_ipv4_list == NULL) {
        return;
    }

    if (current_ipv4_list->size <= 0) {
        if ( xSemaphoreTake( xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
            lv_list_clean(ipv4_scan_list);
            lv_obj_t *btn = lv_list_add_btn(ipv4_scan_list, LV_SYMBOL_WARNING, "No online IP's found!");
            ipv4_scan_btn_list[0] = btn;
            xSemaphoreGive(xGuiSemaphore);
        }
        return;
    }

    //if (xSemaphoreTake( xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
        //lv_list_clean(ipv4_scan_list);
        char ip_string_buf[IP4ADDR_STRLEN_MAX];
        for (int i = 0; i < current_ipv4_list->size; i++) {
            ipv4_info * current_ipv4_info = get_from_ipv4_list_at(current_ipv4_list, i);
            esp_ip4addr_ntoa((esp_ip4_addr_t*)&current_ipv4_info->ip, ip_string_buf, IP4ADDR_STRLEN_MAX);
            printf("Displaying ipv4 device %i with IP: %s\n", i, ip_string_buf);

            scan_ports(*(esp_ip4_addr_t*)&current_ipv4_info->ip); // Why on Earth is this needed?!

            //lv_obj_t *btn = lv_list_add_btn(ipv4_scan_list, LV_SYMBOL_WIFI, ip_string_buf);
            //ipv4_scan_btn_list[i] = btn;
            //lv_obj_set_event_cb(btn, ipv4_scan_list_event_handler);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    //    xSemaphoreGive(xGuiSemaphore);
    //}

    return;
}

void wifi_scan_event_handler(lv_obj_t * obj, lv_event_t event) {
    wifi_scan();
}

void wifi_task(void *arg) {
    while(1) {
        if (network_status == NETWORK_SCANNING) {
            current_ipv4_list = arp_scan_full();
            show_found_ip_list();
            network_status = NETWORK_CONNECTED;
        } else if (network_status == NETWORK_CONNECTING) {
            if (wifi_connect((char*)lv_textarea_get_text(wifi_popup_password_textarea)) == WIFI_CONNECTED_BIT) {
                // Successful connection to AP
                network_status = NETWORK_CONNECTED;
                if (xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
                    lv_obj_move_background(wifi_popup_connect);
                    xSemaphoreGive(xGuiSemaphore);
                }
            } else {
                // Failed to connect to AP
                network_status = NETWORK_CONNECT_FAILED;
                if (xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
                    show_popup_msg_box("Error", "WiFi connection failed!");
                    xSemaphoreGive(xGuiSemaphore);
                }
            }
        } else if (network_status != NETWORK_CONNECTED) {
            network_status = NETWORK_SEARCHING;
            wifi_scan();



            current_ap_record_index = 0;
            wifi_connect(""); // REMOVE LATER!!!!!!!!!!!!!!!!!!!!!!!!!!!!!



            //if (lv_tabview_get_tab_act(tabview) == 0 && xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
            // this function takes the semaphore itself
            show_found_wifi_list();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void create_wifi_task() {
    network_status = NETWORK_SEARCHING;
    xTaskCreatePinnedToCore(wifi_task, "wifi_scan_task", 4096 * 4, NULL, 5, &wifi_task_handle, 0);
}

void mbox_connect_wifi_event_cb(lv_obj_t * obj, lv_event_t event) {
    network_status = NETWORK_CONNECTING;
}

void device_arp_scan_btn_cb(lv_obj_t * obj, lv_event_t event) {
    if (network_status != NETWORK_CONNECTED) {
        ESP_LOGI(TAG, "device_arp_scan_btn_cb(): Can't begin scan. Not connected to network!");
        return;
    }
    prev_network_status = network_status;
    network_status = NETWORK_SCANNING;
}

void gui_task(void *pvParameter) {
    xGuiSemaphore = xSemaphoreCreateMutex();

    // Initialize LVGL
    lv_init();

    // Initialize SPI and display, touch drivers among other LVGL things
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

    set_style();
    build_gui();

    make_keyboard();

    // Not using this...
    //build_statusbar();
    //lv_obj_set_event_cb(, wifi_scan_event_handler);

    build_pwmsg_box();
    lv_obj_move_background(wifi_popup_connect);


    //lv_label_set_text(wifi_connected_status_label, "Not connected to Wi-Fi...");
    lv_obj_set_event_cb(wifi_popup_connect_btn, mbox_connect_wifi_event_cb);

    //lv_label_set_text(wifi_connected_status_label, "Not connected to Wi-Fi...");
    lv_obj_set_event_cb(ipv4_scan_btn, device_arp_scan_btn_cb);

    //buildSettings();

    //tryPreviousNetwork();

    //lv_example_flex_1();

    // // Setting event handlers for GUI objects...

    // This part should run indefinitely all the time
    while (1) {
      // Delay 10ms
      vTaskDelay(pdMS_TO_TICKS(10));

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
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize VFS for lwip and lwip itself
    //init_vfs();
    //lwip_init();

    // Initialize network interface, TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Initialize the event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize Wi-FI
    ESP_ERROR_CHECK(wifi_init());

    // Lwip sys_check_timeouts doesn't belong in a esp timer... pinned to Core 0
    xTaskCreatePinnedToCore(lwip_timer_handle, "lwip_timer", 2048, NULL, 5, NULL, 0);

    // Pin gui drawing task to CPU Core 1. Core 1 just for GUI.
    xTaskCreatePinnedToCore(gui_task, "gui_task", 4096 * 4, NULL, 0, NULL, 1);

    // Create a task for for Wi-Fi networks
    create_wifi_task();
}
