#include "lvgl/lvgl.h"
#include "lvgl/src/drivers/sdl/lv_sdl_window.h"
#include "lvgl/src/drivers/sdl/lv_sdl_mouse.h"
#include "lvgl/src/drivers/sdl/lv_sdl_keyboard.h"
#include "lvgl/src/drivers/sdl/lv_sdl_mousewheel.h"
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <stdlib.h>

#define HOR_RES  1024
#define VER_RES  600

/* =========================
 * 全局句柄：方便定时器里刷新
 * ========================= */
static lv_obj_t * g_temp_bar;
static lv_obj_t * g_temp_label;
static lv_obj_t * g_humidity_label;
static lv_obj_t * g_speed_value_label;
static lv_obj_t * g_speed_arc;
static lv_obj_t * g_chart;
static lv_chart_series_t * g_chart_ser_temp;
static lv_chart_series_t * g_chart_ser_hum;
static lv_obj_t * g_log_ta;
static lv_obj_t * g_device_status_label;
static lv_obj_t * g_led_power;
static lv_obj_t * g_led_alarm;

static uint32_t g_runtime_sec = 0;
static int32_t  g_target_speed = 50;
static bool g_alarm_fired = false;

/* =========================
 * 小工具：追加日志到 TextArea
 * ========================= */
static void log_append(const char * fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    char ts[32];
    snprintf(ts, sizeof(ts), "[%02lu:%02lu] ",
             (unsigned long)(g_runtime_sec / 60),
             (unsigned long)(g_runtime_sec % 60));
    lv_textarea_add_text(g_log_ta, ts);
    lv_textarea_add_text(g_log_ta, buf);
    lv_textarea_add_text(g_log_ta, "\n");
}

/* =========================
 * 通用事件回调
 * ========================= */
static void slider_speed_cb(lv_event_t * e)
{
    lv_obj_t * slider = lv_event_get_target(e);
    g_target_speed = (int32_t)lv_slider_get_value(slider);
    lv_label_set_text_fmt(g_speed_value_label, "%ld %%", (long)g_target_speed);
    lv_arc_set_value(g_speed_arc, g_target_speed);
    log_append("Set speed to %ld%%", (long)g_target_speed);
}

static void switch_power_cb(lv_event_t * e)
{
    lv_obj_t * sw = lv_event_get_target(e);
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    if(on) {
        lv_led_on(g_led_power);
        lv_label_set_text(g_device_status_label, "Running");
        lv_obj_set_style_text_color(g_device_status_label, lv_color_hex(0x1aa260), 0);
        log_append("Power ON");
    } else {
        lv_led_off(g_led_power);
        lv_label_set_text(g_device_status_label, "Stopped");
        lv_obj_set_style_text_color(g_device_status_label, lv_palette_main(LV_PALETTE_GREY), 0);
        log_append("Power OFF");
    }
}

static void switch_alarm_cb(lv_event_t * e)
{
    lv_obj_t * sw = lv_event_get_target(e);
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    if(on) { lv_led_on(g_led_alarm);  g_alarm_fired = true;  log_append("Alarm ENABLED"); }
    else   { lv_led_off(g_led_alarm); g_alarm_fired = false; log_append("Alarm disabled"); }
}

static void mode_dropdown_cb(lv_event_t * e)
{
    lv_obj_t * dd = lv_event_get_target(e);
    uint16_t idx = lv_dropdown_get_selected(dd);
    const char * txt = lv_dropdown_get_text(dd);
    char buf[64];
    lv_dropdown_get_selected_str(dd, buf, sizeof(buf));
    (void)txt; (void)idx;
    log_append("Mode changed: %s", buf);
}

static void btn_save_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    log_append("Settings saved!");
    lv_obj_t * mbox = lv_msgbox_create(NULL);
    lv_msgbox_add_title(mbox, "Success");
    lv_msgbox_add_text(mbox, "Settings have been saved successfully.\n(This is a fake demo messagebox)");
    lv_msgbox_add_close_button(mbox);
    lv_obj_center(mbox);
}

static void btn_reset_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    lv_textarea_set_text(g_log_ta, "");
    log_append("Log cleared by user");
}

/* =========================
 *  1s 定时器：模拟采集数据 + 刷新 UI
 * ========================= */
static void timer_1s_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    g_runtime_sec++;

    /* 模拟温度（基于 sin 波动，25~40°C） */
    float t = 32.0f + 7.0f * sinf(g_runtime_sec * 0.25f) +
              (float)(rand() % 100) / 500.0f;
    /* 模拟湿度 （40~70%） */
    float h = 55.0f + 12.0f * cosf(g_runtime_sec * 0.18f) +
              (float)(rand() % 100) / 500.0f;
    if(t < 0) t = 0; if(h < 0) h = 0;
    if(h > 100) h = 100;

    /* --- 刷新仪表盘 --- */
    lv_bar_set_value(g_temp_bar, (int32_t)(t * 10), LV_ANIM_ON);
    lv_label_set_text_fmt(g_temp_label, "%.1f °C", t);
    lv_label_set_text_fmt(g_humidity_label, "%.1f %%", h);

    /* --- 动态模拟转速：目标值附近轻微波动 --- */
    int32_t actual_speed = g_target_speed + (int32_t)((rand() % 7) - 3);
    if(actual_speed < 0) actual_speed = 0;
    if(actual_speed > 100) actual_speed = 100;
    lv_arc_set_value(g_speed_arc, actual_speed);
    lv_label_set_text_fmt(g_speed_value_label, "%ld %%", (long)actual_speed);

    /* --- 推数据进 Chart --- */
    lv_chart_set_next_value(g_chart, g_chart_ser_temp, (lv_coord_t)(t * 10));
    lv_chart_set_next_value(g_chart, g_chart_ser_hum,  (lv_coord_t)(h * 10));

    /* --- 偶尔告警 --- */
    if(t > 39.0f && g_alarm_fired == false) {
        g_alarm_fired = true;
        lv_led_on(g_led_alarm);
        log_append("WARNING: Temp high! %.1fC", t);
    } else if(t < 37.0f && g_alarm_fired == true) {
        g_alarm_fired = false;
    }
}

/* =========================
 * 页面 1：监控页（仪表盘 + 曲线 + 状态）
 * ========================= */
static void page_monitor_create(lv_obj_t * parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 14, 0);
    lv_obj_set_style_pad_row(parent, 12, 0);

    /* ---------- 顶部状态条 ---------- */
    lv_obj_t * header = lv_obj_create(parent);
    lv_obj_remove_style_all(header);
    lv_obj_set_width(header, LV_PCT(100));
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(header, 0, 0);

    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, "Device Control Panel");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_palette_main(LV_PALETTE_DEEP_PURPLE), 0);

    lv_obj_t * status_wrap = lv_obj_create(header);
    lv_obj_remove_style_all(status_wrap);
    lv_obj_set_flex_flow(status_wrap, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_wrap, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(status_wrap, 8, 0);

    lv_obj_t * label1 = lv_label_create(status_wrap);
    lv_label_set_text(label1, "PWR:");
    g_led_power = lv_led_create(status_wrap);
    lv_obj_set_size(g_led_power, 14, 14);
    lv_led_set_color(g_led_power, lv_palette_main(LV_PALETTE_GREEN));
    lv_led_off(g_led_power);

    lv_obj_t * label2 = lv_label_create(status_wrap);
    lv_label_set_text(label2, "ALM:");
    g_led_alarm = lv_led_create(status_wrap);
    lv_obj_set_size(g_led_alarm, 14, 14);
    lv_led_set_color(g_led_alarm, lv_palette_main(LV_PALETTE_RED));
    lv_led_off(g_led_alarm);

    g_device_status_label = lv_label_create(status_wrap);
    lv_label_set_text(g_device_status_label, "Stopped");
    lv_obj_set_style_text_color(g_device_status_label, lv_palette_main(LV_PALETTE_GREY), 0);

    /* ---------- 上半：三个小仪表盘（Grid 布局） ---------- */
    lv_obj_t * grid = lv_obj_create(parent);
    lv_obj_remove_style_all(grid);
    lv_obj_set_width(grid, LV_PCT(100));
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_style_pad_column(grid, 10, 0);
    lv_obj_set_style_pad_row(grid, 10, 0);

    static lv_coord_t grid_col_dsc[] = {LV_PCT(33), LV_PCT(33), LV_PCT(33), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t grid_row_dsc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(grid, grid_col_dsc, grid_row_dsc);

    /* --- 温度卡片 --- */
    lv_obj_t * card_temp = lv_obj_create(grid);
    lv_obj_set_grid_cell(card_temp, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_style_pad_all(card_temp, 10, 0);
    lv_obj_set_style_radius(card_temp, 8, 0);
    lv_obj_set_flex_flow(card_temp, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card_temp, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(card_temp, lv_color_hex(0xFFF8E1), 0);
    lv_obj_set_style_border_width(card_temp, 0, 0);

    lv_obj_t * ct_title = lv_label_create(card_temp);
    lv_label_set_text(ct_title, "Temperature");
    lv_obj_set_style_text_color(ct_title, lv_color_hex(0xE65100), 0);

    g_temp_label = lv_label_create(card_temp);
    lv_label_set_text(g_temp_label, "-- °C");
    lv_obj_set_style_text_font(g_temp_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(g_temp_label, lv_color_hex(0xEF6C00), 0);

    g_temp_bar = lv_bar_create(card_temp);
    lv_bar_set_range(g_temp_bar, 0, 500);   /* 0~50°C，放大10倍存整数 */
    lv_obj_set_width(g_temp_bar, LV_PCT(90));
    lv_obj_set_height(g_temp_bar, 10);
    lv_bar_set_value(g_temp_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_temp_bar, lv_color_hex(0xFFE0B2), 0);
    lv_obj_set_style_bg_color(g_temp_bar, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_INDICATOR);

    /* --- 湿度卡片 --- */
    lv_obj_t * card_hum = lv_obj_create(grid);
    lv_obj_set_grid_cell(card_hum, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_style_pad_all(card_hum, 10, 0);
    lv_obj_set_style_radius(card_hum, 8, 0);
    lv_obj_set_flex_flow(card_hum, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card_hum, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(card_hum, lv_color_hex(0xE1F5FE), 0);
    lv_obj_set_style_border_width(card_hum, 0, 0);

    lv_obj_t * ch_title = lv_label_create(card_hum);
    lv_label_set_text(ch_title, "Humidity");
    lv_obj_set_style_text_color(ch_title, lv_palette_main(LV_PALETTE_BLUE), 0);

    g_humidity_label = lv_label_create(card_hum);
    lv_label_set_text(g_humidity_label, "-- %");
    lv_obj_set_style_text_font(g_humidity_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(g_humidity_label, lv_palette_main(LV_PALETTE_BLUE), 0);

    lv_obj_t * hum_icon = lv_label_create(card_hum);
    lv_label_set_text(hum_icon, "💧 💧 💧");
    (void)hum_icon;

    /* --- 转速 Arc 卡片 --- */
    lv_obj_t * card_spd = lv_obj_create(grid);
    lv_obj_set_grid_cell(card_spd, LV_GRID_ALIGN_STRETCH, 2, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_style_pad_all(card_spd, 10, 0);
    lv_obj_set_style_radius(card_spd, 8, 0);
    lv_obj_set_flex_flow(card_spd, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card_spd, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(card_spd, lv_color_hex(0xE8F5E9), 0);
    lv_obj_set_style_border_width(card_spd, 0, 0);

    lv_obj_t * cs_title = lv_label_create(card_spd);
    lv_label_set_text(cs_title, "Speed");
    lv_obj_set_style_text_color(cs_title, lv_palette_main(LV_PALETTE_GREEN), 0);

    g_speed_arc = lv_arc_create(card_spd);
    lv_arc_set_range(g_speed_arc, 0, 100);
    lv_arc_set_value(g_speed_arc, 50);
    lv_obj_set_size(g_speed_arc, 100, 100);
    lv_obj_remove_style(g_speed_arc, 0, LV_PART_KNOB);
    lv_obj_clear_flag(g_speed_arc, LV_OBJ_FLAG_CLICKABLE);

    g_speed_value_label = lv_label_create(card_spd);
    lv_label_set_text(g_speed_value_label, "50 %");
    lv_obj_set_style_text_font(g_speed_value_label, &lv_font_montserrat_16, 0);

    /* ---------- 下半：Chart 曲线 ---------- */
    lv_obj_t * chart_card = lv_obj_create(parent);
    lv_obj_remove_style_all(chart_card);
    lv_obj_set_width(chart_card, LV_PCT(100));
    lv_obj_set_style_pad_all(chart_card, 8, 0);
    lv_obj_set_style_radius(chart_card, 8, 0);
    lv_obj_set_style_border_width(chart_card, 1, 0);
    lv_obj_set_style_border_color(chart_card, lv_palette_main(LV_PALETTE_GREY), 0);

    lv_obj_t * ct_header = lv_obj_create(chart_card);
    lv_obj_remove_style_all(ct_header);
    lv_obj_set_width(ct_header, LV_PCT(100));
    lv_obj_set_flex_flow(ct_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ct_header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * cht_title = lv_label_create(ct_header);
    lv_label_set_text(cht_title, "Sensor History");
    lv_obj_set_style_text_font(cht_title, &lv_font_montserrat_14, 0);

    lv_obj_t * legend = lv_obj_create(ct_header);
    lv_obj_remove_style_all(legend);
    lv_obj_set_flex_flow(legend, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(legend, 10, 0);

    lv_obj_t * d1 = lv_obj_create(legend); lv_obj_set_size(d1, 10, 10);
    lv_obj_set_style_radius(d1, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(d1, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_set_style_border_width(d1, 0, 0);
    lv_obj_t * l1 = lv_label_create(legend); lv_label_set_text(l1, "Temp");
    lv_obj_t * d2 = lv_obj_create(legend); lv_obj_set_size(d2, 10, 10);
    lv_obj_set_style_radius(d2, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(d2, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_border_width(d2, 0, 0);
    lv_obj_t * l2 = lv_label_create(legend); lv_label_set_text(l2, "Humidity");

    g_chart = lv_chart_create(chart_card);
    lv_obj_set_width(g_chart, LV_PCT(100));
    lv_obj_set_height(g_chart, 160);
    lv_chart_set_type(g_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(g_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 500);   /* 0~500 = 0~50°C （放大10倍） */
    lv_chart_set_range(g_chart, LV_CHART_AXIS_SECONDARY_Y, 0, 1000); /* 0~100% 湿度 放大10倍 */
    lv_chart_set_point_count(g_chart, 80);
    lv_chart_set_div_line_count(g_chart, 5, 8);

    g_chart_ser_temp = lv_chart_add_series(g_chart,
                                           lv_palette_main(LV_PALETTE_ORANGE),
                                           LV_CHART_AXIS_PRIMARY_Y);
    g_chart_ser_hum  = lv_chart_add_series(g_chart,
                                           lv_palette_main(LV_PALETTE_BLUE),
                                           LV_CHART_AXIS_SECONDARY_Y);
}

/* =========================
 * 页面 2：设置页
 * ========================= */
static void page_settings_create(lv_obj_t * parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 14, 0);
    lv_obj_set_style_pad_row(parent, 10, 0);

    lv_obj_t * title = lv_label_create(parent);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

    /* --- Power Switch + Mode Dropdown (Row) --- */
    lv_obj_t * row1 = lv_obj_create(parent);
    lv_obj_remove_style_all(row1);
    lv_obj_set_width(row1, LV_PCT(100));
    lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(row1, 24, 0);

    lv_obj_t * pw_label = lv_label_create(row1);
    lv_label_set_text(pw_label, "Power:");
    lv_obj_t * sw_power = lv_switch_create(row1);
    lv_obj_add_event_cb(sw_power, switch_power_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t * mode_label = lv_label_create(row1);
    lv_label_set_text(mode_label, "Mode:");
    lv_obj_t * dd_mode = lv_dropdown_create(row1);
    lv_dropdown_set_options(dd_mode, "Manual\nAuto\nEco\nTurbo\nMaintenance");
    lv_dropdown_set_selected(dd_mode, 0);
    lv_obj_add_event_cb(dd_mode, mode_dropdown_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* --- Alarm Switch + Mode 2 --- */
    lv_obj_t * row_b = lv_obj_create(parent);
    lv_obj_remove_style_all(row_b);
    lv_obj_set_width(row_b, LV_PCT(100));
    lv_obj_set_flex_flow(row_b, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(row_b, 24, 0);

    lv_obj_t * al_label = lv_label_create(row_b);
    lv_label_set_text(al_label, "High-Temp Alarm:");
    lv_obj_t * sw_alarm = lv_switch_create(row_b);
    lv_obj_add_event_cb(sw_alarm, switch_alarm_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* --- Target Speed Slider --- */
    lv_obj_t * slider_label = lv_label_create(parent);
    lv_label_set_text(slider_label, "Target Speed");

    lv_obj_t * slider_wrap = lv_obj_create(parent);
    lv_obj_remove_style_all(slider_wrap);
    lv_obj_set_width(slider_wrap, LV_PCT(100));
    lv_obj_set_flex_flow(slider_wrap, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(slider_wrap, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(slider_wrap, 12, 0);

    lv_obj_t * slider_speed = lv_slider_create(slider_wrap);
    lv_slider_set_range(slider_speed, 0, 100);
    lv_slider_set_value(slider_speed, g_target_speed, LV_ANIM_OFF);
    lv_obj_set_width(slider_speed, 320);
    lv_obj_add_event_cb(slider_speed, slider_speed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t * sv = lv_label_create(slider_wrap);
    lv_label_set_text_fmt(sv, "%d %%", g_target_speed);

    /* --- Buttons --- */
    lv_obj_t * btns = lv_obj_create(parent);
    lv_obj_remove_style_all(btns);
    lv_obj_set_width(btns, LV_PCT(100));
    lv_obj_set_flex_flow(btns, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btns, 10, 0);

    lv_obj_t * btn_save = lv_button_create(btns);
    lv_obj_set_style_bg_color(btn_save, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_t * btn_save_l = lv_label_create(btn_save);
    lv_label_set_text(btn_save_l, LV_SYMBOL_SAVE " Save");
    lv_obj_add_event_cb(btn_save, btn_save_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_reset = lv_button_create(btns);
    lv_obj_set_style_bg_color(btn_reset, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_t * btn_reset_l = lv_label_create(btn_reset);
    lv_label_set_text(btn_reset_l, LV_SYMBOL_REFRESH " Clear Log");
    lv_obj_add_event_cb(btn_reset, btn_reset_cb, LV_EVENT_CLICKED, NULL);

    /* --- Log Area --- */
    lv_obj_t * log_label = lv_label_create(parent);
    lv_label_set_text(log_label, "Event Log");

    g_log_ta = lv_textarea_create(parent);
    lv_obj_set_width(g_log_ta, LV_PCT(100));
    lv_obj_set_height(g_log_ta, LV_SIZE_CONTENT);
    lv_textarea_set_placeholder_text(g_log_ta, "Logs will appear here...");
    lv_obj_set_style_min_height(g_log_ta, 120, 0);
    lv_obj_set_style_max_height(g_log_ta, 220, 0);
    lv_textarea_set_cursor_click_pos(g_log_ta, false);
    log_append("System started. Have fun with LVGL v9!");
}

/* =========================
 * 入口
 * ========================= */
void my_ui_create(void)
{
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xF5F7FA), 0);

    lv_obj_t * tv = lv_tabview_create(scr);
    lv_obj_set_size(tv, LV_PCT(100), LV_PCT(100));

    lv_obj_t * tab_monitor  = lv_tabview_add_tab(tv, "Monitor");
    lv_obj_t * tab_settings = lv_tabview_add_tab(tv, "Settings");

    page_monitor_create(tab_monitor);
    page_settings_create(tab_settings);

    /* 1s 定时采集 */
    lv_timer_create(timer_1s_cb, 1000, NULL);
}

int main(int argc, char ** argv)
{
    LV_UNUSED(argc);
    LV_UNUSED(argv);

    lv_init();

    lv_display_t * disp = lv_sdl_window_create(HOR_RES, VER_RES);
    lv_display_set_default(disp);
    lv_sdl_window_set_title(disp, "LVGL v9 Playground");
    lv_sdl_window_set_resizeable(disp, true);

    lv_sdl_mouse_create();
    lv_sdl_keyboard_create();
    lv_sdl_mousewheel_create();

    my_ui_create();

    while(1) {
        uint32_t t = lv_timer_handler();
        lv_delay_ms(t);
    }

    return 0;
}
