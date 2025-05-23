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
#include "nvs_flash.h"
#include "esp_event.h"
#include "freertos/portmacro.h"
#include "lv_conf_internal.h"
#include "nvs_flash.h"

#include "conf.h"
#include "scanner.h"

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
static lv_obj_t * wifi_btn_list[MAX_SCAN_RESULTS];
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



typedef enum {
    NONE,
    NETWORK_SEARCHING,
    NETWORK_CONNECTING,
    NETWORK_SCANNING,
    NETWORK_CONNECTED,
    NETWORK_CONNECT_FAILED,
} Network_Status_t;
Network_Status_t network_status = NONE;


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

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    ESP_LOGI(TAG, "Wi-Fi event handler called with event_id: %d", event_id);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Connecting to AP...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (wifi_retry_count < MAX_WIFI_RETRY_ATTEMPT) {
            ESP_LOGI(TAG, "Reconnecting to AP...");
            esp_wifi_connect();
            wifi_retry_count++;
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
    }
}

static void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t * event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "STA IP: "IPSTR, IP2STR(&event->ip_info.ip));
        wifi_retry_count = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

int wifi_connect(char* wifi_password) {
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

static void tickInc(void *arg) {
    (void)arg;
    lv_tick_inc(LV_TICK_PERIOD_MS);
}

static void list_event_handler(lv_obj_t * obj, lv_event_t event) {
    for (int i = 0; i < MAX_SCAN_RESULTS; i++) {
        if (wifi_btn_list[i] == obj) { //what could go wrong
            lv_label_set_text_fmt(mbox_title_label, "%s", ap_records[i].ssid);
            lv_obj_move_foreground(mbox_connect);
            current_ap_record_index = i;
            break;
        }
    }
}

static void showing_found_wifi_list() {
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
        //lv_list_add_text(wifi_list, found_wifi_count > 1 ? "WiFi: Found Networks" : "WiFi: Not Found!");

        for (int i = 0; i < found_wifi_count; i++) {
            printf("Displaying wifi %i with ssid: %s\n", i, (char*)ap_records[i].ssid);
            lv_obj_t *btn = lv_list_add_btn(wifi_list, LV_SYMBOL_WIFI, (char*)ap_records[i].ssid);
            wifi_btn_list[i] = btn;

            //printf(line_buf, "%s %-32.32s %4d %2d %s\n", LV_SYMBOL_WIFI, ap_records[i].ssid, ap_records[i].rssi, ap_records[i].primary, enc_buf);
            lv_obj_set_event_cb(btn, list_event_handler);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        xSemaphoreGive(xGuiSemaphore);
    }
}

void wifi_scan_event_handler(lv_obj_t * obj, lv_event_t event) {
    wifi_scan();
}

void wifi_task(void *arg) {
    while(1) {
        if (network_status == NETWORK_CONNECTING) {
            if (wifi_connect((char*)lv_textarea_get_text(mbox_password_textarea)) == WIFI_CONNECTED_BIT) {
                // Successful connection to AP
                network_status = NETWORK_CONNECTED;
                if (xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
                    lv_obj_move_background(mbox_connect);
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
        } else {
            if (network_status != NETWORK_CONNECTED) {
                network_status = NETWORK_SEARCHING;
                wifi_scan();
                //if (lv_tabview_get_tab_act(tabview) == 0 && xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
                // this function takes the semaphore itself
                showing_found_wifi_list();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void create_wifi_task() {
    network_status = NETWORK_SEARCHING;
    xTaskCreatePinnedToCore(wifi_task, "wifi_scan_task", 4096, NULL, 5, &wifi_task_handle, 0);
}

void mbox_connect_wifi_event_cb(lv_obj_t * obj, lv_event_t event) {
    network_status = NETWORK_CONNECTING;
}

void device_arp_scan_btn_cb(lv_obj_t * obj, lv_event_t event) {
    arp_scan_full();
}

void gui_task(void *pvParameter) {
    xGuiSemaphore = xSemaphoreCreateMutex();

    // Initialize LVGL
    lv_init();

    // Initialize SPI and display, touch drivers among other LVGL things
    lvgl_driver_init();

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &tickInc,
        .name = "lvgl_tick_inc",
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));

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
    lv_obj_move_background(mbox_connect);


    //lv_label_set_text(wifi_connected_status_label, "Not connected to Wi-Fi...");
    lv_obj_set_event_cb(mbox_connect_btn, mbox_connect_wifi_event_cb);

    //lv_label_set_text(wifi_connected_status_label, "Not connected to Wi-Fi...");
    lv_obj_set_event_cb(device_scan_btn, device_arp_scan_btn_cb);

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

    // Initialize network interface, TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Initialize the event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize Wi-FI
    ESP_ERROR_CHECK(wifi_init());

    // Pin gui drawing task to CPU core 1
    xTaskCreatePinnedToCore(gui_task, "gui_task", 4096 * 4, NULL, 0, NULL, 1);

    // Create a task for for Wi-Fi networks
    create_wifi_task();
}
