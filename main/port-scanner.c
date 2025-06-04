#include "port-scanner.h"
#include "conf.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_netif_ip_addr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lwip/sockets.h"

#define TAG "Port scanner"


void scan_ports(esp_ip4_addr_t target_ip) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        ESP_LOGE(TAG, "Could not create socket");
    }

    struct sockaddr_in server;

    for (int port = 1; port <= 65532; port++) {
        //ESP_LOGI(TAG, "Trying to connect to port %i", port);

        server.sin_family = AF_INET;
        server.sin_port = htons(port);
        server.sin_addr.s_addr = target_ip.addr; // Use the esp_ip4_addr_t directly

        // // Set socket to non-blocking
        // fcntl(sock, F_SETFL, O_NONBLOCK);

        // Attempt to connect
        int err = connect(sock, (struct sockaddr *)&server, sizeof(server));
        if (err < 0) {
            if (errno != EINPROGRESS) {
                //ESP_LOGI(TAG, "Port %d is closed", port);
            }
        } else {
            ESP_LOGI(TAG, "Port %d is open", port);
        }

        close(sock);
        vTaskDelay(10 / portTICK_PERIOD_MS); // Delay to avoid overwhelming the network
    }
}
