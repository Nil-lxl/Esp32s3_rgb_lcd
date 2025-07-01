/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "lvgl.h"
#include "demos/lv_demos.h"
#include "lcd_config.h"
#include "wifi_component.h"

static lv_style_t style_bullet;
static lv_obj_t *scale1;
static const lv_font_t *font_normal = &lv_font_montserrat_14;

static lv_obj_t *create_scale_box(lv_obj_t *parent, const char *text1, const char *text2, const char *text3) {
    lv_obj_t *scale = lv_scale_create(parent);
    lv_obj_center(scale);
    lv_obj_set_size(scale, 300, 300);
    lv_scale_set_mode(scale, LV_SCALE_MODE_ROUND_OUTER);
    lv_scale_set_label_show(scale, false);
    lv_scale_set_post_draw(scale, true);
    lv_obj_set_width(scale, LV_PCT(100));
    lv_obj_set_style_pad_all(scale, 30, 0);

    lv_obj_t *bullet1 = lv_obj_create(parent);
    lv_obj_set_size(bullet1, 13, 13);
    lv_obj_remove_style(bullet1, NULL, LV_PART_SCROLLBAR);
    lv_obj_add_style(bullet1, &style_bullet, 0);
    lv_obj_set_style_bg_color(bullet1, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_t *label1 = lv_label_create(parent);
    lv_label_set_text(label1, text1);

    lv_obj_t *bullet2 = lv_obj_create(parent);
    lv_obj_set_size(bullet2, 13, 13);
    lv_obj_remove_style(bullet2, NULL, LV_PART_SCROLLBAR);
    lv_obj_add_style(bullet2, &style_bullet, 0);
    lv_obj_set_style_bg_color(bullet2, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_t *label2 = lv_label_create(parent);
    lv_label_set_text(label2, text2);

    lv_obj_t *bullet3 = lv_obj_create(parent);
    lv_obj_set_size(bullet3, 13, 13);
    lv_obj_remove_style(bullet3, NULL, LV_PART_SCROLLBAR);
    lv_obj_add_style(bullet3, &style_bullet, 0);
    lv_obj_set_style_bg_color(bullet3, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_t *label3 = lv_label_create(parent);
    lv_label_set_text(label3, text3);

    static int32_t grid_col_dsc[] = { LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    static int32_t grid_row_dsc[] = { LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST };
    lv_obj_set_grid_dsc_array(parent, grid_col_dsc, grid_row_dsc);
    lv_obj_set_grid_cell(scale, LV_GRID_ALIGN_START, 0, 2, LV_GRID_ALIGN_START, 1, 1);
    lv_obj_set_grid_cell(bullet1, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_START, 2, 1);
    lv_obj_set_grid_cell(bullet2, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_START, 3, 1);
    lv_obj_set_grid_cell(bullet3, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_START, 4, 1);
    lv_obj_set_grid_cell(label1, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_START, 2, 1);
    lv_obj_set_grid_cell(label2, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_START, 3, 1);
    lv_obj_set_grid_cell(label3, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_START, 4, 1);
    return scale;
}

static void scale1_indic1_anim_cb(void *var, int32_t v) {
    lv_arc_set_value(var, v);

    lv_obj_t *card = lv_obj_get_parent(scale1);
    lv_obj_t *label = lv_obj_get_child(card, -5);
    lv_label_set_text_fmt(label, "Revenue: %"LV_PRId32" %%", v);
}

static void scale1_indic2_anim_cb(void *var, int32_t v) {
    lv_arc_set_value(var, v);

    lv_obj_t *card = lv_obj_get_parent(scale1);
    lv_obj_t *label = lv_obj_get_child(card, -3);
    lv_label_set_text_fmt(label, "Sales: %"LV_PRId32" %%", v);
}

static void scale1_indic3_anim_cb(void *var, int32_t v) {
    lv_arc_set_value(var, v);

    lv_obj_t *card = lv_obj_get_parent(scale1);
    lv_obj_t *label = lv_obj_get_child(card, -1);
    lv_label_set_text_fmt(label, "Costs: %"LV_PRId32" %%", v);
}




void cancel_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *input_box = lv_event_get_user_data(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_del(input_box);
    }
}
void connect_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *pwd_ta = lv_event_get_user_data(e);

    const char *wifi_pwd = lv_textarea_get_text(pwd_ta);
    if (code == LV_EVENT_CLICKED) {
        printf("%s\n", wifi_pwd);
    }
}
void input_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *keyboard = lv_event_get_user_data(e);
    if (code == LV_EVENT_FOCUSED) {
        lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }
    if (code == LV_EVENT_DEFOCUSED) {
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}
void keyboard_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *keyboard = lv_event_get_target(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        uint16_t id = lv_btnmatrix_get_selected_btn(keyboard);
        const char *txt = lv_btnmatrix_get_btn_text(keyboard, id);
        if (strcmp(txt, LV_SYMBOL_KEYBOARD) == 0) {
            lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void wifi_list_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *wifi_choose = lv_event_get_target(e);
    lv_obj_t *container = lv_event_get_user_data(e);

    lv_obj_t *input_box = NULL;
    if (code == LV_EVENT_FOCUSED) {
        lv_obj_set_style_bg_color(wifi_choose, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
    } else if (code == LV_EVENT_DEFOCUSED) {
        lv_obj_set_style_bg_color(wifi_choose, lv_color_white(), 0);
    }

    if (code == LV_EVENT_CLICKED) {
        lv_obj_t *wifi_ssid_label = lv_obj_get_child(wifi_choose, 1);
        const char *wifi_ssid = lv_label_get_text(wifi_ssid_label);
        const char *wifi_pwd;

        input_box = lv_obj_create(container);
        lv_obj_set_size(input_box, 300, 200);
        lv_obj_set_align(input_box, LV_ALIGN_CENTER);
        lv_obj_set_style_radius(input_box, 30, 0);
        lv_obj_set_style_pad_all(input_box, 0, 0);

        lv_obj_t *wifi_title = lv_obj_create(input_box);
        lv_obj_center(input_box);
        lv_obj_set_size(wifi_title, lv_pct(100), 40);
        lv_obj_set_style_border_width(wifi_title, 0, 0);
        lv_obj_clear_flag(wifi_title, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *title_name = lv_label_create(wifi_title);
        lv_obj_center(title_name);
        lv_label_set_text(title_name, wifi_ssid);

        lv_obj_t *pwd_ta = lv_textarea_create(input_box);
        lv_obj_set_height(pwd_ta, 40);
        lv_obj_set_align(pwd_ta, LV_ALIGN_TOP_MID);
        lv_obj_set_y(pwd_ta, lv_pct(20));
        lv_obj_set_style_radius(pwd_ta, 30, 0);

        lv_obj_t *connect_btn = lv_btn_create(input_box);
        lv_obj_set_size(connect_btn, 80, 40);
        lv_obj_align_to(connect_btn, input_box, LV_ALIGN_BOTTOM_LEFT, lv_pct(10), lv_pct(-10));

        lv_obj_t *connect_label = lv_label_create(connect_btn);
        lv_label_set_text(connect_label, "Connect");
        lv_obj_center(connect_label);

        lv_obj_t *cancel_btn = lv_btn_create(input_box);
        lv_obj_set_size(cancel_btn, 80, 40);
        lv_obj_align_to(cancel_btn, input_box, LV_ALIGN_BOTTOM_RIGHT, lv_pct(-10), lv_pct(-10));

        lv_obj_t *cancel_label = lv_label_create(cancel_btn);
        lv_label_set_text(cancel_label, "Cancel");
        lv_obj_center(cancel_label);

        lv_obj_t *pwd_keyboard = lv_keyboard_create(container);
        lv_keyboard_set_textarea(pwd_keyboard, pwd_ta);
        lv_keyboard_set_mode(pwd_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
        lv_obj_add_flag(pwd_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_popovers(pwd_keyboard, true);

        lv_obj_add_event_cb(pwd_ta, input_cb, LV_EVENT_ALL, pwd_keyboard);
        lv_obj_add_event_cb(connect_btn, connect_cb, LV_EVENT_CLICKED, pwd_ta);
        lv_obj_add_event_cb(cancel_btn, cancel_cb, LV_EVENT_CLICKED, input_box);
        lv_obj_add_event_cb(pwd_keyboard, keyboard_cb, LV_EVENT_ALL, NULL);
    }

}
void wifi_scan_cb(lv_event_t *e) {

    wifi_scan();

    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *scan_btn = lv_event_get_target(e);
    lv_obj_t *container = lv_event_get_user_data(e);

    static lv_obj_t *wifi_list[WIFI_SCAN_LIST_NUM];
    if (code == LV_EVENT_CLICKED) {

        lv_obj_add_flag(scan_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(container, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
        lv_obj_set_style_pad_all(container, 0, 0);
        lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);

        for (int i = 0;i < WIFI_SCAN_LIST_NUM;i++) {
            wifi_list[i] = lv_obj_create(container);
            lv_obj_set_size(wifi_list[i], lv_pct(100), 40);
            lv_obj_set_pos(wifi_list[i], 0, 40 * i);
            lv_obj_set_style_border_width(wifi_list[i], 0, 0);
            lv_obj_set_style_bg_color(wifi_list[i], lv_color_white(), 0);
            lv_obj_set_style_radius(wifi_list[i], 0, 0);
            lv_obj_set_style_shadow_width(wifi_list[i], 0, 0);
            lv_obj_add_flag(wifi_list[i], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(wifi_list[i], LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t *wifi_symbol = lv_img_create(wifi_list[i]);
            lv_img_set_src(wifi_symbol, LV_SYMBOL_WIFI);

            lv_obj_t *wifi_ssid = lv_label_create(wifi_list[i]);
            lv_obj_set_style_text_color(wifi_ssid, lv_color_black(), 0);
            lv_label_set_text_fmt(wifi_ssid, "%s", wifi_ap_info[i].ssid);
            lv_obj_set_pos(wifi_ssid, 50, 0);

            lv_obj_t *wifi_rssi = lv_label_create(wifi_list[i]);
            lv_obj_set_style_text_color(wifi_rssi, lv_color_black(), 0);
            lv_label_set_text_fmt(wifi_rssi, "%d", wifi_ap_info[i].rssi);
            lv_obj_set_pos(wifi_rssi, 200, 0);

            lv_obj_t *right_symbol = lv_img_create(wifi_list[i]);
            lv_img_set_src(right_symbol, LV_SYMBOL_RIGHT);
            lv_obj_set_pos(right_symbol, 250, 0);

            lv_obj_add_event_cb(wifi_list[i], wifi_list_cb, LV_EVENT_ALL, container);
        }
    }
}
void lv_wifi_scr() {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_t *container = lv_obj_create(scr);
    lv_obj_set_size(container, 300, 400);
    lv_obj_set_align(container, LV_ALIGN_CENTER);

    lv_obj_t *scan_btn = lv_btn_create(container);
    lv_obj_set_size(scan_btn, 80, 50);
    lv_obj_set_align(scan_btn, LV_ALIGN_CENTER);

    lv_obj_t *scan_text = lv_label_create(scan_btn);
    lv_obj_center(scan_text);
    lv_label_set_text(scan_text, "Scan wifi");

    lv_obj_add_event_cb(scan_btn, wifi_scan_cb, LV_EVENT_CLICKED, container);

}
void example_lvgl_demo_ui(lv_display_t *disp) {
    lv_wifi_scr();

    // lv_demo_widgets();

    // init default theme
    // lv_theme_default_init(disp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), LV_THEME_DEFAULT_DARK,
    //                       font_normal);
    // // bullet style
    // lv_style_init(&style_bullet);
    // lv_style_set_border_width(&style_bullet, 0);
    // lv_style_set_radius(&style_bullet, LV_RADIUS_CIRCLE);

    // lv_obj_t *parent = lv_display_get_screen_active(disp);

    // // create scale widget
    // scale1 = create_scale_box(parent, "Revenue", "Sales", "Costs");

    // // create arc indicators
    // lv_obj_t *arc;
    // arc = lv_arc_create(scale1);
    // lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    // lv_obj_remove_style(arc, NULL, LV_PART_MAIN);
    // lv_obj_set_size(arc, lv_pct(100), lv_pct(100));
    // lv_obj_set_style_arc_opa(arc, 0, 0);
    // lv_obj_set_style_arc_width(arc, 15, LV_PART_INDICATOR);
    // lv_obj_set_style_arc_color(arc, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
    // lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    // // animation
    // lv_anim_t a;
    // lv_anim_init(&a);
    // lv_anim_set_values(&a, 20, 100);
    // lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    // lv_anim_set_exec_cb(&a, scale1_indic1_anim_cb);
    // lv_anim_set_var(&a, arc);
    // lv_anim_set_duration(&a, 4100);
    // lv_anim_set_playback_duration(&a, 2700);
    // lv_anim_start(&a);

    // arc = lv_arc_create(scale1);
    // lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    // lv_obj_set_size(arc, lv_pct(100), lv_pct(100));
    // lv_obj_set_style_margin_all(arc, 20, 0);
    // lv_obj_set_style_arc_opa(arc, 0, 0);
    // lv_obj_set_style_arc_width(arc, 15, LV_PART_INDICATOR);
    // lv_obj_set_style_arc_color(arc, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);
    // lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    // lv_obj_center(arc);

    // lv_anim_set_exec_cb(&a, scale1_indic2_anim_cb);
    // lv_anim_set_var(&a, arc);
    // lv_anim_set_duration(&a, 2600);
    // lv_anim_set_playback_duration(&a, 3200);
    // lv_anim_start(&a);

    // arc = lv_arc_create(scale1);
    // lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    // lv_obj_set_size(arc, lv_pct(100), lv_pct(100));
    // lv_obj_set_style_margin_all(arc, 40, 0);
    // lv_obj_set_style_arc_opa(arc, 0, 0);
    // lv_obj_set_style_arc_width(arc, 15, LV_PART_INDICATOR);
    // lv_obj_set_style_arc_color(arc, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
    // lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    // lv_obj_center(arc);

    // lv_anim_set_exec_cb(&a, scale1_indic3_anim_cb);
    // lv_anim_set_var(&a, arc);
    // lv_anim_set_duration(&a, 2800);
    // lv_anim_set_playback_duration(&a, 1800);
    // lv_anim_start(&a);
}
