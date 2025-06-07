#include "gui.h"
#include "conf.h"

#include "lv_conf_internal.h"
#include "lv_core/lv_disp.h"
#include "lv_core/lv_obj.h"
#include "lv_core/lv_obj_style_dec.h"
#include "lv_core/lv_style.h"
#include "lv_font/lv_font.h"
#include "lv_misc/lv_area.h"
#include "lv_misc/lv_color.h"
#include "lv_themes/lv_theme.h"
#include "lv_widgets/lv_btn.h"
#include "lv_widgets/lv_keyboard.h"
#include "lv_widgets/lv_label.h"
#include "lv_widgets/lv_list.h"
#include "lv_widgets/lv_switch.h"
#include "lv_widgets/lv_tabview.h"
#include "lv_widgets/lv_textarea.h"

lv_style_t default_style;
lv_style_t statusbar_style;
lv_obj_t * keyboard;

/** TABS **/
lv_obj_t * tabview;

/** WiFi scan/connect list tab **/
lv_obj_t * wifi_list;
lv_obj_t * wifi_scan_tab;
lv_obj_t * wifi_scan_list_label;
lv_obj_t * wifi_connected_status_label;

/* WiFi network device scanning tab */
lv_obj_t * ipv4_scan_tab;
lv_obj_t * ipv4_scan_btn;
lv_obj_t * ipv4_scan_list;
//lv_obj_t * ipv4_scan_ip_label;
//lv_obj_t * ipv4_scan_mac_label;
//lv_obj_t * ipv4_scan_gw_label;

/* Tab for serving scanned data */
lv_obj_t * server_tab;
lv_obj_t * serve_switch;
lv_obj_t * server_ap_ssid_label;
lv_obj_t * server_ap_password;
lv_obj_t * server_http_ip;

/* Wifi connect pop-up */
lv_obj_t * wifi_popup;
lv_obj_t * wifi_popup_password_textarea;
lv_obj_t * wifi_popup_title_label;
lv_obj_t * wifi_popup_connect_btn;
lv_obj_t * wifi_popup_btn_label;
lv_obj_t * wifi_popup_close_btn;

lv_obj_t * popup_box;
lv_obj_t * popup_msg_label;
lv_obj_t * popup_title_label;
lv_obj_t * popup_box_close_btn;
lv_obj_t * popup_box_close_btn_label;

/* Top status bar */
lv_obj_t * time_label;
lv_obj_t * wifi_label;
lv_obj_t * status_label;

#define LV_STATE_ALL LV_STATE_DEFAULT | LV_STATE_CHECKED | LV_STATE_FOCUSED | LV_STATE_EDITED  | LV_STATE_HOVERED | LV_STATE_PRESSED | LV_STATE_DISABLED


void set_default_style(lv_obj_t * obj) {
    lv_obj_add_style(obj, LV_STATE_ALL, &default_style);
}

void init_style(void) {
    /*** Style ***/
    lv_style_init(&default_style);
    // Background
    lv_style_set_bg_color(&default_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_style_set_bg_color(&default_style, LV_STATE_PRESSED, LV_COLOR_WHITE);
    // Text
    lv_style_set_text_color(&default_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_style_set_text_font(&default_style, LV_STATE_ALL, &lv_font_montserrat_8);
    lv_style_set_text_color(&default_style, LV_STATE_PRESSED, LV_COLOR_BLACK);
    // Border
    lv_style_set_border_color(&default_style, LV_STATE_DEFAULT, lv_color_hex(0x777777));
    lv_style_set_border_width(&default_style, LV_STATE_ALL, 1);
}



void build_gui(void) {
    tabview = lv_tabview_create(lv_scr_act(), NULL);

    // make room for statusbar
    lv_obj_align(tabview, lv_scr_act(), LV_ALIGN_IN_TOP_LEFT, 0, STATUS_BAR_H);
    wifi_scan_tab = lv_tabview_add_tab(tabview, "Wifi");
    lv_obj_align(wifi_scan_tab, lv_scr_act(), LV_ALIGN_IN_TOP_LEFT, 0, 50);
    set_default_style(wifi_scan_tab);

    wifi_list = lv_list_create(wifi_scan_tab, NULL);
    lv_obj_set_width(wifi_list, DISPLAY_W);
    lv_obj_align(wifi_list, wifi_scan_tab, LV_ALIGN_IN_TOP_MID, 0, 5);
    //lv_obj_add_style(wifi_list, LV_STATE_ALL, &no_border_style);

    ipv4_scan_tab = lv_tabview_add_tab(tabview, "Scan");
    ipv4_scan_btn = lv_btn_create(ipv4_scan_tab, NULL);
    lv_obj_t * ipv4_scan_btn_label = lv_label_create(ipv4_scan_btn, NULL);
    lv_label_set_text(ipv4_scan_btn_label, "Full ARP scan");
    lv_obj_set_size(ipv4_scan_btn, DISPLAY_W / 2 - 20, 35);
    lv_obj_align(ipv4_scan_btn, ipv4_scan_tab, LV_ALIGN_IN_TOP_MID, 0, 5);
    ipv4_scan_list = lv_list_create(ipv4_scan_tab, NULL);
    lv_obj_set_size(ipv4_scan_list, DISPLAY_W - 20, DISPLAY_H);
    lv_obj_align(ipv4_scan_list, ipv4_scan_tab, LV_ALIGN_IN_TOP_MID, 0, 45);
    //ipv4_scan_ip_label = lv_label_create(ipv4_scan_tab, NULL);
    //lv_label_set_text(ipv4_scan_ip_label, "IP:");
    //lv_obj_set_style_local_text_font(ipv4_scan_ip_label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &lv_font_montserrat_10);
    //lv_obj_align(ipv4_scan_ip_label, ipv4_scan_tab, LV_ALIGN_IN_TOP_LEFT, DISPLAY_W / 2 - 20, 10);
    //ipv4_scan_mac_label = lv_label_create(ipv4_scan_tab, NULL);
    //lv_label_set_text(ipv4_scan_mac_label, "MAC:");
    //lv_obj_set_style_local_text_font(ipv4_scan_mac_label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &lv_font_montserrat_10);
    //lv_obj_align(ipv4_scan_mac_label, ipv4_scan_tab, LV_ALIGN_IN_TOP_LEFT, DISPLAY_W / 2 - 20, 20);
    //ipv4_scan_gw_label = lv_label_create(ipv4_scan_tab, NULL);
    //lv_label_set_text(ipv4_scan_gw_label, "GW:");
    //lv_obj_set_style_local_text_font(ipv4_scan_gw_label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &lv_font_montserrat_10);
    //lv_obj_align(ipv4_scan_gw_label, ipv4_scan_tab, LV_ALIGN_IN_TOP_LEFT, DISPLAY_W / 2 - 20, 30);

    server_tab = lv_tabview_add_tab(tabview, "Server");
    server_ap_ssid_label = lv_label_create(server_tab, NULL);
    lv_obj_align(server_ap_ssid_label, server_tab, LV_ALIGN_IN_TOP_MID, -60, 20);
    lv_label_set_text_fmt(server_ap_ssid_label, "SSID: %s", CONFIG_LWIP_LOCAL_HOSTNAME);
    server_ap_password = lv_label_create(server_tab, NULL);
    lv_obj_align(server_ap_password, server_tab, LV_ALIGN_IN_TOP_MID, -60, 40);
    lv_label_set_text_fmt(server_ap_password, "Password: %s", AP_PASSWORD);
    server_http_ip = lv_label_create(server_tab, NULL);
    lv_obj_align(server_http_ip, server_tab, LV_ALIGN_IN_TOP_MID, -60, 60);
    lv_label_set_text(server_http_ip, "IP:"); // change after wifi initialisation or when starting AP...
    serve_switch = lv_switch_create(server_tab, NULL);
    lv_obj_align(serve_switch, server_tab, LV_ALIGN_CENTER, -40, 10);
    lv_obj_set_size(serve_switch, 120, 60);         // assign callback to start server
}

void make_keyboard(void) {
    keyboard = lv_keyboard_create(lv_scr_act(), NULL);
    lv_obj_set_hidden(keyboard, 1);
}

void on_focus_keyboard_popup_cb(lv_obj_t *obj, lv_event_t e) {
    if (e == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(keyboard, obj);
        lv_obj_move_foreground(keyboard);
        lv_obj_set_hidden(keyboard, 0);
    }

    if (e == LV_EVENT_DEFOCUSED) {
        lv_keyboard_set_textarea(keyboard, obj);
        lv_obj_set_hidden(keyboard, 1);
    }
}

void close_btn_event_cb(lv_obj_t *obj, lv_event_t e) {
    if (obj == wifi_popup_close_btn) {
        lv_obj_move_background(wifi_popup);
        lv_obj_set_hidden(keyboard, 1);
    } else if (popup_box_close_btn) {
        lv_obj_move_background(popup_box);
        lv_obj_set_hidden(keyboard, 1);
    }
}

void build_wifi_connection_box() {
    wifi_popup = lv_obj_create(lv_scr_act(), NULL);
    lv_obj_add_style(wifi_popup, LV_TABVIEW_PART_BG, &default_style);
    lv_obj_set_size(wifi_popup, DISPLAY_W * 3 / 4, DISPLAY_H * 2 / 3);
    lv_obj_align(wifi_popup, lv_scr_act(), LV_ALIGN_CENTER, 0, 0);

    wifi_popup_title_label = lv_label_create(wifi_popup, NULL);
    lv_label_set_text(wifi_popup_title_label, "");
    lv_obj_align(wifi_popup_title_label, wifi_popup, LV_ALIGN_IN_TOP_LEFT, 0, 0);

    wifi_popup_password_textarea = lv_textarea_create(wifi_popup, NULL);
    lv_textarea_set_text(wifi_popup_password_textarea, DEFAULT_WIFI_INPUT_PASSWORD);
    lv_obj_set_size(wifi_popup_password_textarea, DISPLAY_W / 2, 40);
    lv_obj_align(wifi_popup_password_textarea, wifi_popup_title_label, LV_ALIGN_IN_TOP_LEFT, 5, 30);
    lv_obj_set_event_cb(wifi_popup_password_textarea, on_focus_keyboard_popup_cb);
    lv_textarea_set_placeholder_text(wifi_popup_password_textarea, "Password?");
    lv_keyboard_set_textarea(keyboard, wifi_popup_password_textarea); /* Focus it on one of the text areas to start */
    lv_keyboard_set_cursor_manage(keyboard, true); /* Automatically show/hide cursors on text areas */

    wifi_popup_connect_btn = lv_btn_create(wifi_popup, NULL);
    lv_obj_set_size(wifi_popup_connect_btn, DISPLAY_W / 3, DISPLAY_H / 6);
    lv_obj_align(wifi_popup_connect_btn, wifi_popup, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
    wifi_popup_btn_label = lv_label_create(wifi_popup_connect_btn, NULL);
    lv_label_set_text(wifi_popup_btn_label, "Connect");
    lv_obj_align(wifi_popup_btn_label, wifi_popup_connect_btn, LV_ALIGN_CENTER, 0, 0);

    wifi_popup_close_btn = lv_btn_create(wifi_popup, NULL);
    lv_obj_set_size(wifi_popup_close_btn, DISPLAY_W / 3, DISPLAY_H / 6);
    lv_obj_set_event_cb(wifi_popup_close_btn, close_btn_event_cb);
    lv_obj_align(wifi_popup_close_btn, wifi_popup, LV_ALIGN_IN_BOTTOM_RIGHT, 0, 0);
    lv_obj_t * mbox_btn_label = lv_label_create(wifi_popup_close_btn, NULL);
    lv_label_set_text(mbox_btn_label, "Cancel");
    lv_obj_align(mbox_btn_label, wifi_popup_close_btn, LV_ALIGN_CENTER, 0, 0);
}

void show_popup_msg_box(char * title, char * msg) {
    if (popup_box != NULL) {
        lv_obj_del(popup_box);
    }

    popup_box = lv_obj_create(lv_scr_act(), NULL);
    lv_obj_add_style(popup_box, LV_PAGE_PART_BG, &default_style);
    lv_obj_set_size(popup_box, DISPLAY_W * 2 / 3, DISPLAY_H / 2);
    lv_obj_align(popup_box, lv_scr_act(), LV_ALIGN_CENTER, 0, 0);

    popup_title_label = lv_label_create(popup_box, NULL);
    lv_label_set_text(popup_title_label, title);
    lv_obj_set_width(popup_title_label, DISPLAY_W * 2 / 3 - 50);
    lv_obj_align(popup_title_label, popup_box, LV_ALIGN_IN_TOP_LEFT, 5, 5);

    popup_msg_label = lv_label_create(popup_box, NULL);
    lv_label_set_text(popup_title_label, msg);
    lv_obj_set_width(popup_title_label, DISPLAY_W * 2 / 3 - 50);
    lv_obj_align(popup_title_label, popup_box, LV_ALIGN_IN_TOP_LEFT, 0, 40);

    popup_box_close_btn = lv_btn_create(popup_box, NULL);
    lv_obj_set_event_cb(popup_box_close_btn, close_btn_event_cb);
    lv_obj_align(popup_box_close_btn, popup_box, LV_ALIGN_IN_BOTTOM_RIGHT, 0, 0);
    popup_box_close_btn_label = lv_label_create(popup_box_close_btn, NULL);
    lv_label_set_text(popup_box_close_btn_label, "okay");
    lv_obj_align(popup_box_close_btn_label, popup_box_close_btn, LV_ALIGN_CENTER, 0, 0);

    lv_obj_move_foreground(popup_box);
}

void build_statusbar(void) {
    lv_style_init(&statusbar_style);
    lv_style_copy(&statusbar_style, &default_style);
    lv_style_set_text_font(&statusbar_style, LV_STATE_ALL, &lv_font_montserrat_8);
    lv_style_set_border_width(&statusbar_style, LV_STATE_DEFAULT, 0);
    lv_style_set_border_color(&statusbar_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);

    status_label = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_text(status_label, "Status");
    lv_obj_align(status_label, lv_scr_act(), LV_ALIGN_IN_TOP_LEFT, 5, 0);
    lv_obj_add_style(status_label, LV_OBJ_PART_MAIN, &statusbar_style);

    time_label = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_text(time_label, "HH:MM" LV_SYMBOL_CLOSE);
    lv_obj_align(time_label, lv_scr_act(), LV_ALIGN_IN_TOP_RIGHT, 15, 0);
    lv_obj_add_style(time_label, LV_OBJ_PART_MAIN, &statusbar_style);
}


