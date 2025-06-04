#ifndef GUI_H_
#define GUI_H_

#include "lv_core/lv_obj.h"
#include "lv_core/lv_style.h"

#define DISPLAY_W 320
#define DISPLAY_H 240

#define STYLE_SET 1
#define STYLE_UNSET 0


extern lv_style_t desc_text_style;
extern lv_style_t btn_style;
// extern lv_obj_t * status_bar;
extern lv_obj_t * keyboard;
extern lv_obj_t * scan_btn;
// extern lv_obj_t * time_label;

// extern lv_obj_t * settings_btn;
// extern lv_obj_t * settings_btn_label;


extern lv_obj_t * tabview;

extern lv_obj_t * wifi_connect_tab;
extern lv_obj_t * wifi_list;
extern lv_obj_t * wifi_scan_list_label;
extern lv_obj_t * wifi_connected_status_label;

extern lv_obj_t * ipv4_scan_tab;
extern lv_obj_t * ipv4_scan_btn;
extern lv_obj_t * ipv4_scan_list;


extern lv_obj_t * tab2;
extern lv_obj_t * tab3;
extern lv_obj_t * tab4;

extern lv_obj_t * wifi_popup_connect;
extern lv_obj_t * wifi_popup_password_textarea;
extern lv_obj_t * wifi_popup_title_label;
extern lv_obj_t * wifi_popup_connect_btn;
extern lv_obj_t * wifi_popup_btn_label;
extern lv_obj_t * wifi_popup_close_btn;


void set_style(void);
void build_gui(void);

void make_keyboard(void);
void build_statusbar(void);

void build_pwmsg_box(void);
void show_popup_msg_box(char * title, char * msg);

#endif // GUI_H_
