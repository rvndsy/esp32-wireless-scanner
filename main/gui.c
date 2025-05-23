#include "gui.h"

#include "lv_core/lv_disp.h"
#include "lv_core/lv_obj.h"
#include "lv_core/lv_obj_style_dec.h"
#include "lv_font/lv_font.h"
#include "lv_misc/lv_area.h"
#include "lv_widgets/lv_btn.h"
#include "lv_widgets/lv_keyboard.h"
#include "lv_widgets/lv_label.h"
#include "lv_widgets/lv_list.h"
#include "lv_widgets/lv_tabview.h"
#include "lv_widgets/lv_textarea.h"

lv_style_t desc_text_style;
lv_style_t btn_style;
lv_style_t border_style;
lv_style_t popup_box_style;

lv_obj_t * status_bar;
lv_obj_t * keyboard;
lv_obj_t * scan_btn;
// lv_obj_t * time_label;
// lv_obj_t * settings_btn;
// lv_obj_t * settings_btn_label;


/** TABS **/
lv_obj_t * tabview;

/** WiFi scan/connect list tab **/
lv_obj_t * wifi_list;
lv_obj_t * wifi_scan_tab;
lv_obj_t * wifi_scan_list_label;
lv_obj_t * wifi_connected_status_label;

/* WiFi network device scanning tab */
lv_obj_t * device_scan_tab;
lv_obj_t * device_scan_btn;

/* Placeholder tabs */
lv_obj_t * tab3;
lv_obj_t * tab4;

/* Wifi connect pop-up */
lv_obj_t * mbox_connect;
lv_obj_t * mbox_password_textarea;
lv_obj_t * mbox_title_label;
lv_obj_t * mbox_connect_btn;
lv_obj_t * mbox_btn_label;
lv_obj_t * mbox_close_btn;

lv_obj_t * popup_box;
lv_obj_t * popup_msg_label;
lv_obj_t * popup_title_label;
lv_obj_t * popup_box_close_btn;
lv_obj_t * popup_box_close_btn_label;

void set_style(void) {
    lv_style_init(&desc_text_style);
    lv_style_set_bg_color(&desc_text_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_style_set_text_color(&desc_text_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_style_set_text_font(&desc_text_style, LV_STATE_DEFAULT, &lv_font_montserrat_12);

    lv_style_init(&border_style);
    lv_style_set_border_width(&border_style, LV_STATE_DEFAULT, 2);
    lv_style_set_border_color(&border_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);

    lv_style_init(&popup_box_style);
    lv_style_set_radius(&popup_box_style, LV_STATE_DEFAULT, 10);
    lv_style_set_bg_opa(&popup_box_style, LV_STATE_DEFAULT, LV_OPA_COVER);
    lv_style_set_border_color(&popup_box_style, LV_STATE_DEFAULT, LV_COLOR_BLUE);
    lv_style_set_border_width(&popup_box_style, LV_STATE_DEFAULT, 5);
}

void build_gui(void) {
    //scan_btn = lv_btn_create(lv_scr_act(), NULL);
    //lv_obj_align(scan_btn, NULL, LV_ALIGN_CENTER, 0, 0);
    //lv_obj_set_size(scan_btn, 80, 40);

    tabview = lv_tabview_create(lv_scr_act(), NULL);
    wifi_scan_tab = lv_tabview_add_tab(tabview, "Wifi");

    wifi_connected_status_label = lv_label_create(wifi_scan_tab, NULL);
    lv_label_set_text(wifi_connected_status_label, "Not connected to Wi-Fi...");
    lv_obj_add_style(wifi_connected_status_label, LV_TABVIEW_PART_BG, &desc_text_style);
    lv_obj_align(wifi_connected_status_label, wifi_scan_tab, LV_ALIGN_IN_TOP_LEFT, 10, 0);

    wifi_list = lv_list_create(wifi_scan_tab, NULL);
    lv_obj_set_size(wifi_list, DISPLAY_W - 20, DISPLAY_H);
    lv_obj_align(wifi_list, wifi_scan_tab, LV_ALIGN_IN_TOP_MID, 0, 20);

    device_scan_tab = lv_tabview_add_tab(tabview, "Scan");
    device_scan_btn = lv_btn_create(device_scan_tab, NULL);
    lv_obj_align(device_scan_btn, device_scan_tab, LV_ALIGN_IN_TOP_LEFT, 10, 10);

    // Placeholders...
    tab3 = lv_tabview_add_tab(tabview, "Tab3");
    tab4 = lv_tabview_add_tab(tabview, "Tab4");

    //wifi_scan_list_label = lv_label_create(wifi_scan_tab1, NULL);
    //lv_obj_add_style(wifi_scan_list_label, LV_TABVIEW_PART_BG, &desc_text_style);
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
    if (obj == mbox_close_btn) {
        lv_obj_move_background(mbox_connect);
        lv_obj_set_hidden(keyboard, 1);
    } else if (popup_box_close_btn) {
        lv_obj_move_background(popup_box);
        lv_obj_set_hidden(keyboard, 1);
    }
}

void build_pwmsg_box() {
    mbox_connect = lv_obj_create(lv_scr_act(), NULL);
    lv_obj_add_style(mbox_connect, LV_TABVIEW_PART_BG, &border_style);
    lv_obj_set_size(mbox_connect, DISPLAY_W * 3 / 4, DISPLAY_H * 2 / 3);
    lv_obj_align(mbox_connect, lv_scr_act(), LV_ALIGN_CENTER, 0, 0);

    mbox_title_label = lv_label_create(mbox_connect, NULL);
    lv_label_set_text(mbox_title_label, "Selected WiFi SSID: ThatProject");
    lv_obj_align(mbox_title_label, mbox_connect, LV_ALIGN_IN_TOP_LEFT, 0, 0);

    mbox_password_textarea = lv_textarea_create(mbox_connect, NULL);
    lv_textarea_set_text(mbox_password_textarea, "");
    lv_obj_set_size(mbox_password_textarea, DISPLAY_W / 2, 40);
    lv_obj_align(mbox_password_textarea, mbox_title_label, LV_ALIGN_IN_TOP_LEFT, 5, 30);
    lv_obj_set_event_cb(mbox_password_textarea, on_focus_keyboard_popup_cb);
    lv_textarea_set_placeholder_text(mbox_password_textarea, "Password?");
    lv_keyboard_set_textarea(keyboard, mbox_password_textarea); /* Focus it on one of the text areas to start */
    lv_keyboard_set_cursor_manage(keyboard, true); /* Automatically show/hide cursors on text areas */

    mbox_connect_btn = lv_btn_create(mbox_connect, NULL);
    lv_obj_set_size(mbox_connect_btn, DISPLAY_W / 4, DISPLAY_H / 8);
    lv_obj_align(mbox_connect_btn, mbox_connect, LV_ALIGN_IN_BOTTOM_LEFT, 5, 5);
    mbox_btn_label = lv_label_create(mbox_connect_btn, NULL);
    lv_label_set_text(mbox_btn_label, "Connect");
    lv_obj_align(mbox_btn_label, mbox_connect_btn, LV_ALIGN_CENTER, 0, 0);

    mbox_close_btn = lv_btn_create(mbox_connect, NULL);
    lv_obj_set_size(mbox_close_btn, DISPLAY_W / 4, DISPLAY_H / 8);
    lv_obj_set_event_cb(mbox_close_btn, close_btn_event_cb);
    lv_obj_align(mbox_close_btn, mbox_connect, LV_ALIGN_IN_BOTTOM_RIGHT, 5, 5);
    lv_obj_t * mbox_btn_label = lv_label_create(mbox_close_btn, NULL);
    lv_label_set_text(mbox_btn_label, "Cancel");
    lv_obj_align(mbox_btn_label, mbox_close_btn, LV_ALIGN_CENTER, 0, 0);
}

void show_popup_msg_box(char * title, char * msg) {
    if (popup_box != NULL) {
        lv_obj_del(popup_box);
    }

    popup_box = lv_obj_create(lv_scr_act(), NULL);
    lv_obj_add_style(popup_box, LV_PAGE_PART_BG, &popup_box_style);
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

// void build_statusbar(void) {
// 
// 
//     status_bar = lv_obj_create(lv_scr_act(), NULL);
//     lv_obj_set_size(status_bar, DISPLAY_W, 30);
//     lv_obj_align(status_bar, lv_scr_act(), LV_ALIGN_IN_TOP_MID, 0, 0);
// 
//     //lv_obj_remove_style(status_bar, NULL, LV_PART_SCROLLBAR | LV_STATE_ANY);
// 
//     time_label = lv_label_create(status_bar, NULL);
//     lv_obj_set_size(time_label, DISPLAY_W - 50, 30);
// 
//     lv_label_set_text(time_label, "WiFi Not Connected!    " LV_SYMBOL_CLOSE);
//     lv_obj_align(time_label, status_bar, LV_ALIGN_IN_LEFT_MID, 8, 4);
// 
//     settings_btn = lv_btn_create(status_bar, NULL);
//     lv_obj_set_size(settings_btn, 30, 30);
//     lv_obj_align(settings_btn, status_bar, LV_ALIGN_IN_RIGHT_MID, 0, 0);
// 
//     //lv_obj_set_event_cb(settings_btn, btn_event_cb);
// 
//     settings_btn_label = lv_label_create(settings_btn, NULL); /*Add a label to the button*/
//     lv_label_set_text(settings_btn_label, LV_SYMBOL_SETTINGS);  /*Set the labels text*/
//     lv_style_set_value_align(&btn_style, LV_STATE_DEFAULT, LV_ALIGN_CENTER);
// }


