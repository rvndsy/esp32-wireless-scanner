#ifndef GUI_H_
#define GUI_H_

#include "lv_core/lv_obj.h"
#include "lv_core/lv_style.h"

#define DISPLAY_W 320
#define DISPLAY_H 240

#define STATUS_BAR_H 20


extern lv_style_t default_style;
extern lv_obj_t * keyboard;
extern lv_obj_t * scan_btn;
extern lv_obj_t * tabview;

extern lv_obj_t * wifi_scan_tab;
extern lv_obj_t * wifi_list;
extern lv_obj_t * wifi_scan_list_label;
extern lv_obj_t * wifi_connected_status_label;

extern lv_obj_t * ipv4_scan_tab;
extern lv_obj_t * ipv4_scan_btn;
extern lv_obj_t * ipv4_scan_list;
//extern lv_obj_t * ipv4_scan_ip_label;
//extern lv_obj_t * ipv4_scan_mac_label;
//extern lv_obj_t * ipv4_scan_gw_label;

extern lv_obj_t * server_tab;
extern lv_obj_t * serve_switch;
extern lv_obj_t * server_ap_ssid_label;
extern lv_obj_t * server_ap_password;
extern lv_obj_t * server_http_ip;

extern lv_obj_t * wifi_popup;
extern lv_obj_t * wifi_popup_password_textarea;
extern lv_obj_t * wifi_popup_title_label;
extern lv_obj_t * wifi_popup_connect_btn;
extern lv_obj_t * wifi_popup_btn_label;
extern lv_obj_t * wifi_popup_close_btn;

extern lv_obj_t * time_label;
extern lv_obj_t * wifi_label;
extern lv_obj_t * status_label;



void init_style(void);
void build_gui(void);

void make_keyboard(void);
void build_statusbar(void);

void build_wifi_connection_box(void);
void show_popup_msg_box(char * title, char * msg);

#endif // GUI_H_
