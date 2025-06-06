#include "port-scanner.h"
#include "conf.h"
#include "scanner.h"

#include "esp_log.h"
#include "esp_netif_ip_addr.h"
#include "esp_timer.h"
#include "freertos/projdefs.h"
#include "lwip/ip_addr.h"
#include "lwip/tcp.h"
#include "lwip/tcpbase.h"
#include "sys_arch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "Port_scanner"

typedef struct tcp_conn_ctx {
    struct tcp_pcb * pcb;
    esp_timer_handle_t timeout_timer;
    bool lock;
    uint16_t port;
    uint8_t * port_map;
} tcp_conn_ctx;

tcp_conn_ctx * ctx_list[MAX_ONGOING_CONNECTIONS];
uint8_t * local_port_map;

err_t tcp_connection_finished_cb(void *arg, struct tcp_pcb *tpcb, err_t err) {
    //printf("Connection status changed...\n");
    tcp_conn_ctx * ctx = (tcp_conn_ctx*)arg;
    if (err == ERR_OK) {
        printf("OK: %u\n", tpcb->remote_port);
        local_port_map[ctx->port / 8] |= (0x1 << (ctx->port % 8));
    } else {
        // This doesnt happen for some reason... thus the need for a timeout
        printf("FAIL: %u\n", tpcb->remote_port);
    }

    if (ctx == NULL) {
        return ERR_ARG;
    }

    if (ctx->timeout_timer != NULL) {
        esp_timer_stop(ctx->timeout_timer);
        esp_timer_delete(ctx->timeout_timer);
        ctx->timeout_timer = NULL;
    }

    if (ctx->lock == true) {
        ctx->lock = false;
        //if (ctx->pcb->state != CLOSED) {
        //    tcp_close(ctx->pcb);
        //}
    }

    return err;
}

void connection_timeout_cb(void *arg) {
    tcp_conn_ctx * ctx = (tcp_conn_ctx*)arg;

    //printf("Timeout on port %i\n", ctx->port);
    if (ctx == NULL) {
        return;
    }
    ctx->lock = false;
    //printf(".\n");
}

uint8_t * scan_ports(const esp_ip4_addr_t target_ip, uint8_t * port_map) {
    memset(port_map, 0x0, OPEN_PORT_MAP_SIZE);
    local_port_map = port_map;
    ESP_LOGI(TAG, "Initializing port scan for %s\n", ip4addr_ntoa((ip4_addr_t*)&target_ip.addr));

    //ESP_LOGI(TAG, "scan_ports(): Checking if host is up...");
    //ipv4_info * check_if_up = arp_scan_single(target_ip);
    //if (check_if_up == NULL) {
    //    ESP_LOGI(TAG, "scan_ports(): Host down, skipping port scan");
    //    return NULL;
    //}
    //free(check_if_up);
    //ESP_LOGI(TAG, "scan_ports(): Host up, initializing scan");

    for (int i = 0; i < MAX_ONGOING_CONNECTIONS; i++) {
        ctx_list[i] = calloc(1, sizeof(tcp_conn_ctx));
        if (ctx_list[i] == NULL) {
            ESP_LOGE(TAG, "scan_ports(): Failed to calloc tcp_conn_ctx %i of %i", i, MAX_ONGOING_CONNECTIONS);
            return NULL;
        }
        ctx_list[i]->pcb = NULL;
        ctx_list[i]->lock = false;
    }

    // Target ip in ip_addr_t so lwip understands
    ip_addr_t ip_addr;
    ip_addr.u_addr.ip4.addr = target_ip.addr;

    // in case anyone defined MAX_PORT too large
    uint16_t max_port = MAX_PORT;
    if (MAX_PORT > UINT16_MAX-1) {
        max_port = UINT16_MAX-1;
    }
    // scan every port patiently...
    for (uint16_t port = MIN_PORT; port <= max_port; port++) {
        int i = 0;
        while (1) {
            if (i == MAX_ONGOING_CONNECTIONS) {
                i = 0;
                continue;
            }
            if (ctx_list[i]->lock == false) {
                if (ctx_list[i]->pcb != NULL && (ctx_list[i]->pcb->state != CLOSED || ctx_list[i]->pcb->state != ESTABLISHED)) {

                }
                ctx_list[i]->pcb = tcp_new();
                if (ctx_list[i] == NULL) {
                    ESP_LOGE(TAG, "scan_ports(): Error creating pcb #%i with tcp_new()", i);
                    return NULL;
                }
                break;
            }
            i++;
            vTaskDelay(pdMS_TO_TICKS(25));
        }
        ctx_list[i]->port = port;

        //ESP_LOGI(TAG, "scan_ports(): Connecting to port %i", port);
        tcp_arg(ctx_list[i]->pcb, ctx_list[i]);
        tcp_connect(ctx_list[i]->pcb, &ip_addr, port, tcp_connection_finished_cb);

        ctx_list[i]->lock = true;
        esp_timer_create_args_t timer_args = {
            .callback = connection_timeout_cb,
            .arg = ctx_list[i],
            .name = "tcp_connect_timeout"
        };
        esp_timer_create(&timer_args, &ctx_list[i]->timeout_timer);
        esp_timer_start_once(ctx_list[i]->timeout_timer, TCP_CONN_TIMEOUT_MS * 1000);
    }

    int freed_ctx_count = 0;
    int i = 0;
    while (freed_ctx_count < MAX_ONGOING_CONNECTIONS) {
        if (i == MAX_ONGOING_CONNECTIONS) {
            i = 0;
            continue;
        }
        if (ctx_list[i] != NULL && ctx_list[i]->lock == false) {
            if (ctx_list[i]->timeout_timer != NULL) {
                esp_timer_delete(ctx_list[i]->timeout_timer);
                ctx_list[i]->timeout_timer = NULL;
            }
            if (ctx_list[i] != NULL) {
                //memset(ctx_list[i]->pcb, 0x0, sizeof(struct tcp_pcb));
                //memset(ctx_list[i], 0x0, sizeof(tcp_conn_ctx));
                free(ctx_list[i]);
                ctx_list[i] = NULL;
            }
            freed_ctx_count++;
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Avoid watchdog being triggered
        i++;
    }
    ESP_LOGI(TAG, "Port scan done for %s\n", ip4addr_ntoa((ip4_addr_t*)&target_ip.addr));

    return local_port_map;
}



