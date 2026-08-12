/* ==========================================================================
 * LVGL v9 学习实验框架 (Playground)
 * --------------------------------------------------------------------------
 * 结构分层：
 *   main()                  [不动] SDL 初始化 + 主循环
 *   lab_entry_create()      [框架] 5 个主 Tab + 每个主 Tab 内的二级选择器
 *     ├─ Tab 1 Widgets      [控件实验] 8 个核心 widget，Dropdown 选 → 内容区加载
 *     ├─ Tab 2 Layouts      [布局实验] Flex / Grid 4 个专题
 *     ├─ Tab 3 Events       [事件状态] 事件分发 / 冒泡 / 状态样式 / 定时器动画
 *     ├─ Tab 4 Porting      [移植模拟] Tick / 刷新 / Mutex(预留)
 *     └─ Tab 5 Sandbox      [自由沙盒] 随便造
 *   底部 lab_log()          [日志] 统一输出到 TextArea，自动滚底 + 时间戳
 *
 * 使用方法：
 *   1. 编译运行后，切到对应 Tab → 用 Dropdown 选具体子实验 → 点【加载实验】
 *   2. 找到对应 lab_xxx() 函数，删了函数里的 TODO，写你自己的代码
 *   3. 任何需要打日志的地方直接 lab_log("xxx = %d", val)
 * ========================================================================== */

#include "lvgl/lvgl.h"
#include "lvgl/src/drivers/sdl/lv_sdl_window.h"
#include "lvgl/src/drivers/sdl/lv_sdl_mouse.h"
#include "lvgl/src/drivers/sdl/lv_sdl_keyboard.h"
#include "lvgl/src/drivers/sdl/lv_sdl_mousewheel.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define HOR_RES  1024
#define VER_RES  600

/* ================== 全局句柄（框架内部用，实验代码里尽量不用） ================== */
static lv_obj_t * g_log_ta;
static lv_obj_t * g_content_widgets;   /* Tab1 内容区 */
static lv_obj_t * g_content_layouts;   /* Tab2 内容区 */
static lv_obj_t * g_content_events;    /* Tab3 内容区 */
static lv_obj_t * g_content_porting;   /* Tab4 内容区 */
static lv_obj_t * g_content_sandbox;   /* Tab5 内容区 */

/* ============================================================
 *  公共工具：统一日志（底部 TextArea，带 mm:ss 时间戳，自动滚底）
 * ============================================================ */
static uint32_t g_runtime_ms = 0;

void lab_log(const char * fmt, ...)
{
    if (!g_log_ta) return;

    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    char ts[32];
    uint32_t s = g_runtime_ms / 1000;
    snprintf(ts, sizeof(ts), "[%02lu:%02lu] ",
             (unsigned long)(s / 60), (unsigned long)(s % 60));
    lv_textarea_add_text(g_log_ta, ts);
    lv_textarea_add_text(g_log_ta, buf);
    lv_textarea_add_text(g_log_ta, "\n");

    /* 日志总行数超过阈值，砍掉最前面的（防止无边界长） */
    const char * text = lv_textarea_get_text(g_log_ta);
    uint32_t nl = 0;
    const char * p = text;
    while (*p) { if (*p++ == '\n') nl++; }
    if (nl > 80) {
        /* 找到第 (nl-60) 个换行位置截掉 */
        p = text;
        uint32_t target = nl - 60;
        uint32_t cnt = 0;
        while (*p && cnt < target) {
            if (*p++ == '\n') cnt++;
        }
        lv_textarea_set_text(g_log_ta, p);
    }

    lv_textarea_set_cursor_pos(g_log_ta, LV_TEXTAREA_CURSOR_LAST);
    lv_obj_scroll_to_view(g_log_ta, LV_ANIM_OFF);
}

/* ============================================================
 *  公共工具：造一个统一风格的"卡片容器"
 *  所有实验都尽量往 card 里放，外观一致 + 天然隔离父样式
 * ============================================================ */
static lv_obj_t * lab_make_card(lv_obj_t * parent, const char * title)
{
    lv_obj_t * card = lv_obj_create(parent);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 10, 0);

    if (title) {
        lv_obj_t * lbl = lv_label_create(card);
        lv_label_set_text(lbl, title);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lbl, lv_palette_main(LV_PALETTE_DEEP_PURPLE), 0);
    }
    return card;
}

/* ============================================================
 *  实验 ID 枚举（对应每个主 Tab 的 Dropdown 选项顺序）
 *  新增实验：在 enum + 对应 Dropdown options + load 函数 switch 里各加一项
 * ============================================================ */
enum lab_widget_id_e {
    LAB_W_LABEL = 0,
    LAB_W_BUTTON,
    LAB_W_SLIDER_BAR_ARC,
    LAB_W_SWITCH,
    LAB_W_CHART,
    LAB_W_DROPDOWN_ROLLER,
    LAB_W_TEXTAREA,
    LAB_W_TABLE,
    _LAB_W_MAX,
};
enum lab_layout_id_e {
    LAB_L_FLEX_BASIC = 0,
    LAB_L_FLEX_GROW,
    LAB_L_GRID_BASIC,
    LAB_L_GRID_RESPONSIVE,
    _LAB_L_MAX,
};
enum lab_event_id_e {
    LAB_E_BASIC = 0,
    LAB_E_BUBBLE,
    LAB_E_STATE_STYLE,
    LAB_E_TIMER_ANIM,
    _LAB_E_MAX,
};
enum lab_porting_id_e {
    LAB_P_TICK = 0,
    LAB_P_REFRESH,
    LAB_P_MUTEX_SKELETON,
    _LAB_P_MAX,
};

/* ==================== 实验函数前置声明（按 Tab 分组） ==================== */

/* --- Tab 1 Widgets --- */
static void lab_widget_label(lv_obj_t * parent);
static void lab_widget_button(lv_obj_t * parent);
static void lab_widget_slider_bar_arc(lv_obj_t * parent);
static void lab_widget_switch(lv_obj_t * parent);
static void lab_widget_chart(lv_obj_t * parent);
static void lab_widget_dropdown_roller(lv_obj_t * parent);
static void lab_widget_textarea(lv_obj_t * parent);
static void lab_widget_table(lv_obj_t * parent);

/* --- Tab 2 Layouts --- */
static void lab_layout_flex_basic(lv_obj_t * parent);
static void lab_layout_flex_grow(lv_obj_t * parent);
static void lab_layout_grid_basic(lv_obj_t * parent);
static void lab_layout_grid_responsive(lv_obj_t * parent);

/* --- Tab 3 Events --- */
static void lab_event_basic(lv_obj_t * parent);
static void lab_event_bubble(lv_obj_t * parent);
static void lab_event_state_style(lv_obj_t * parent);
static void lab_event_timer_anim(lv_obj_t * parent);

/* --- Tab 4 Porting --- */
static void lab_porting_tick(lv_obj_t * parent);
static void lab_porting_refresh(lv_obj_t * parent);
static void lab_porting_mutex_skeleton(lv_obj_t * parent);

/* --- Tab 5 Sandbox --- */
static void lab_sandbox(lv_obj_t * parent);
static void clear_log_cb(lv_event_t * e);
static void sandbox_tab_switch_cb(lv_event_t * e);

/* ============================================================
 *  实验分发：选哪个 ID 就调哪个 lab_xxx(parent)
 *  调之前自动 lv_obj_clean(内容区)，防止上一个实验的控件和内存残留
 * ============================================================ */
static void lab_load_widgets(lv_obj_t * content, int id)
{
    lv_obj_clean(content);
    switch (id) {
    case LAB_W_LABEL:             lab_widget_label(content); break;
    case LAB_W_BUTTON:            lab_widget_button(content); break;
    case LAB_W_SLIDER_BAR_ARC:    lab_widget_slider_bar_arc(content); break;
    case LAB_W_SWITCH:            lab_widget_switch(content); break;
    case LAB_W_CHART:             lab_widget_chart(content); break;
    case LAB_W_DROPDOWN_ROLLER:   lab_widget_dropdown_roller(content); break;
    case LAB_W_TEXTAREA:          lab_widget_textarea(content); break;
    case LAB_W_TABLE:             lab_widget_table(content); break;
    default: lab_log("未知实验 ID=%d", id); break;
    }
}
static void lab_load_layouts(lv_obj_t * content, int id)
{
    lv_obj_clean(content);
    switch (id) {
    case LAB_L_FLEX_BASIC:       lab_layout_flex_basic(content); break;
    case LAB_L_FLEX_GROW:        lab_layout_flex_grow(content); break;
    case LAB_L_GRID_BASIC:       lab_layout_grid_basic(content); break;
    case LAB_L_GRID_RESPONSIVE:  lab_layout_grid_responsive(content); break;
    default: lab_log("未知实验 ID=%d", id); break;
    }
}
static void lab_load_events(lv_obj_t * content, int id)
{
    lv_obj_clean(content);
    switch (id) {
    case LAB_E_BASIC:         lab_event_basic(content); break;
    case LAB_E_BUBBLE:        lab_event_bubble(content); break;
    case LAB_E_STATE_STYLE:   lab_event_state_style(content); break;
    case LAB_E_TIMER_ANIM:    lab_event_timer_anim(content); break;
    default: lab_log("未知实验 ID=%d", id); break;
    }
}
static void lab_load_porting(lv_obj_t * content, int id)
{
    lv_obj_clean(content);
    switch (id) {
    case LAB_P_TICK:              lab_porting_tick(content); break;
    case LAB_P_REFRESH:           lab_porting_refresh(content); break;
    case LAB_P_MUTEX_SKELETON:    lab_porting_mutex_skeleton(content); break;
    default: lab_log("未知实验 ID=%d", id); break;
    }
}

/* ============================================================
 *  框架：5 个主 Tab 的 UI 构造器
 *  每个主 Tab 统一结构：[Dropdown 选实验 + 加载按钮]  +  内容区
 * ============================================================ */

/* --- 通用：造一个 "Dropdown + 加载按钮" 的选择器行 --- */
typedef struct {
    lv_obj_t * dd;
    lv_obj_t * content;
    void (*load_cb)(lv_obj_t *, int);
} lab_picker_ctx_t;

static void picker_load_cb(lv_event_t * e)
{
    lab_picker_ctx_t * ctx = (lab_picker_ctx_t *)lv_event_get_user_data(e);
    int id = (int)lv_dropdown_get_selected(ctx->dd);
    lab_log("加载实验 (id=%d)", id);
    ctx->load_cb(ctx->content, id);
}

static lv_obj_t * lab_build_picker(lv_obj_t * parent,
                                    const char * dd_options,
                                    lv_obj_t * content_area,
                                    void (*load_cb)(lv_obj_t *, int),
                                    lab_picker_ctx_t ** out_ctx)
{
    lv_obj_t * row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 10, 0);
    lv_obj_set_style_pad_bottom(row, 8, 0);

    lab_picker_ctx_t * ctx = (lab_picker_ctx_t *)lv_malloc(sizeof(lab_picker_ctx_t));
    LV_ASSERT_MALLOC(ctx);
    memset(ctx, 0, sizeof(*ctx));
    ctx->content = content_area;
    ctx->load_cb = load_cb;

    ctx->dd = lv_dropdown_create(row);
    lv_dropdown_set_options(ctx->dd, dd_options);

    lv_obj_t * btn = lv_button_create(row);
    lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_t * lbl = lv_label_create(btn);
    lv_label_set_text(lbl, LV_SYMBOL_PLAY " 加载实验");
    lv_obj_add_event_cb(btn, picker_load_cb, LV_EVENT_CLICKED, ctx);

    /* 给 Dropdown 本身也绑定 VALUE_CHANGED 时自动加载（可选，更顺手） */
    lv_obj_add_event_cb(ctx->dd, picker_load_cb, LV_EVENT_VALUE_CHANGED, ctx);

    if (out_ctx) *out_ctx = ctx;   /* 为了后续手动 free（框架不回收也没事，生命周期=进程生命） */
    return row;
}

/* ============================================================
 *  入口：lab_entry_create(scr)
 * ============================================================ */
static void lab_entry_create(lv_obj_t * scr)
{
    /* 整个页面分上下：上部实验区（TabView），下部日志区（固定高） */
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(scr, 8, 0);
    lv_obj_set_style_pad_row(scr, 8, 0);

    /* --------- 上部：TabView --------- */
    lv_obj_t * tv = lv_tabview_create(scr);
    lv_obj_set_width(tv, LV_PCT(100));
    lv_obj_set_flex_grow(tv, 1);  /* 吃满剩余空间 */

    lv_obj_t * tab_widgets = lv_tabview_add_tab(tv, "1.Widgets");
    lv_obj_t * tab_layouts = lv_tabview_add_tab(tv, "2.Layouts");
    lv_obj_t * tab_events  = lv_tabview_add_tab(tv, "3.Events");
    lv_obj_t * tab_porting = lv_tabview_add_tab(tv, "4.Porting");
    lv_obj_t * tab_sandbox = lv_tabview_add_tab(tv, "5.Sandbox");

    /* ============== Tab 1 : Widgets ============== */
    lv_obj_set_flex_flow(tab_widgets, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tab_widgets, 10, 0);

    g_content_widgets = lv_obj_create(tab_widgets);
    lv_obj_remove_style_all(g_content_widgets);
    lv_obj_set_width(g_content_widgets, LV_PCT(100));
    lv_obj_set_flex_grow(g_content_widgets, 1);
    lv_obj_set_flex_flow(g_content_widgets, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(g_content_widgets, 10, 0);
    /* 内容区允许纵向滚动（卡片多了也能看全） */
    lv_obj_add_flag(g_content_widgets, LV_OBJ_FLAG_SCROLLABLE);

    lab_build_picker(tab_widgets,
        "Label\n"
        "Button\n"
        "Slider+Bar+Arc (三方联动)\n"
        "Switch\n"
        "Chart\n"
        "Dropdown+Roller\n"
        "TextArea\n"
        "Table (参数配置)",
        g_content_widgets,
        lab_load_widgets,
        NULL);

    /* 默认加载第一个（Label 提示卡片） */
    lab_load_widgets(g_content_widgets, LAB_W_LABEL);

    /* ============== Tab 2 : Layouts ============== */
    lv_obj_set_flex_flow(tab_layouts, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tab_layouts, 10, 0);

    g_content_layouts = lv_obj_create(tab_layouts);
    lv_obj_remove_style_all(g_content_layouts);
    lv_obj_set_width(g_content_layouts, LV_PCT(100));
    lv_obj_set_flex_grow(g_content_layouts, 1);
    lv_obj_add_flag(g_content_layouts, LV_OBJ_FLAG_SCROLLABLE);

    lab_build_picker(tab_layouts,
        "Flex 基础 (方向/对齐)\n"
        "Flex-Grow 三栏\n"
        "Grid 基础 (dsc数组)\n"
        "Grid 横竖屏切换",
        g_content_layouts,
        lab_load_layouts,
        NULL);
    lab_load_layouts(g_content_layouts, LAB_L_FLEX_BASIC);

    /* ============== Tab 3 : Events ============== */
    lv_obj_set_flex_flow(tab_events, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tab_events, 10, 0);

    g_content_events = lv_obj_create(tab_events);
    lv_obj_remove_style_all(g_content_events);
    lv_obj_set_width(g_content_events, LV_PCT(100));
    lv_obj_set_flex_grow(g_content_events, 1);
    lv_obj_add_flag(g_content_events, LV_OBJ_FLAG_SCROLLABLE);

    lab_build_picker(tab_events,
        "事件基础 (触发顺序日志)\n"
        "事件冒泡 (三层捕获)\n"
        "状态 x 样式 (自动切换)\n"
        "定时器 vs 动画 对比",
        g_content_events,
        lab_load_events,
        NULL);
    lab_load_events(g_content_events, LAB_E_BASIC);

    /* ============== Tab 4 : Porting ============== */
    lv_obj_set_flex_flow(tab_porting, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tab_porting, 10, 0);

    g_content_porting = lv_obj_create(tab_porting);
    lv_obj_remove_style_all(g_content_porting);
    lv_obj_set_width(g_content_porting, LV_PCT(100));
    lv_obj_set_flex_grow(g_content_porting, 1);
    lv_obj_add_flag(g_content_porting, LV_OBJ_FLAG_SCROLLABLE);

    lab_build_picker(tab_porting,
        "Tick 时基观察\n"
        "刷新 / 脏矩形监控\n"
        "RTOS Mutex 保护(骨架占位)",
        g_content_porting,
        lab_load_porting,
        NULL);
    lab_load_porting(g_content_porting, LAB_P_TICK);

    /* ============== Tab 5 : Sandbox ============== */
    /* 沙盒不用二级选择器，所有内容都在 lab_sandbox()，切 Tab 会清理内容区 */
    lv_obj_set_flex_flow(tab_sandbox, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tab_sandbox, 10, 0);

    g_content_sandbox = lv_obj_create(tab_sandbox);
    lv_obj_remove_style_all(g_content_sandbox);
    lv_obj_set_size(g_content_sandbox, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_grow(g_content_sandbox, 1);
    lv_obj_add_flag(g_content_sandbox, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(tv, sandbox_tab_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lab_sandbox(g_content_sandbox);

    /* --------- 下部：日志区 --------- */
    lv_obj_t * log_wrap = lv_obj_create(scr);
    lv_obj_remove_style_all(log_wrap);
    lv_obj_set_width(log_wrap, LV_PCT(100));
    lv_obj_set_height(log_wrap, 170);
    lv_obj_set_flex_flow(log_wrap, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_border_width(log_wrap, 1, 0);
    lv_obj_set_style_border_color(log_wrap, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_radius(log_wrap, 8, 0);
    lv_obj_set_style_pad_all(log_wrap, 8, 0);

    lv_obj_t * log_title_row = lv_obj_create(log_wrap);
    lv_obj_remove_style_all(log_title_row);
    lv_obj_set_width(log_title_row, LV_PCT(100));
    lv_obj_set_flex_flow(log_title_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(log_title_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * log_title = lv_label_create(log_title_row);
    lv_label_set_text(log_title, "Lab Log");
    lv_obj_set_style_text_font(log_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(log_title, lv_palette_main(LV_PALETTE_TEAL), 0);

    lv_obj_t * clear_btn = lv_button_create(log_title_row);
    lv_obj_set_style_bg_color(clear_btn, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_t * clear_lbl = lv_label_create(clear_btn);
    lv_label_set_text(clear_lbl, LV_SYMBOL_TRASH " 清空");
    lv_obj_add_event_cb(clear_btn, clear_log_cb, LV_EVENT_CLICKED, NULL);

    g_log_ta = lv_textarea_create(log_wrap);
    lv_obj_set_width(g_log_ta, LV_PCT(100));
    lv_obj_set_flex_grow(g_log_ta, 1);
    lv_textarea_set_placeholder_text(g_log_ta, "实验日志输出区 (任何实验都可调用 lab_log)");
    lv_textarea_set_cursor_click_pos(g_log_ta, false);
    lv_obj_set_style_text_font(g_log_ta, &lv_font_montserrat_14, 0);

    lab_log("LVGL Playground 框架启动完毕 ✅");
    lab_log("提示：切 Tab 后用 Dropdown 选具体实验，或直接点【加载实验】按钮");
}

/* ============================================================
 *  1s 心跳定时器（给全局 g_runtime_ms 加时，用于日志时间戳）
 * ============================================================ */
static void lab_heartbeat(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    g_runtime_ms += 1000;
}

/* ============================================================
 *  日志清空按钮的回调（替换 lambda，纯 C）
 * ============================================================ */
static void clear_log_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if (g_log_ta) lv_textarea_set_text(g_log_ta, "");
}

/* ============================================================
 *  Tab 切换的监听（切换到 Sandbox 时重建内容）
 * ============================================================ */
static void sandbox_tab_switch_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    /* 目前简单：每次切 Tab 都清沙盒再重建，避免实验内容残留 */
    if (g_content_sandbox) {
        lab_sandbox(g_content_sandbox);
    }
}

/* ============================================================
 *  main()
 * ============================================================ */
int main(int argc, char ** argv)
{
    LV_UNUSED(argc); LV_UNUSED(argv);

    lv_init();

    lv_display_t * disp = lv_sdl_window_create(HOR_RES, VER_RES);
    lv_display_set_default(disp);
    lv_sdl_window_set_title(disp, "LVGL v9 Learning Playground");
    lv_sdl_window_set_resizeable(disp, true);

    lv_sdl_mouse_create();
    lv_sdl_keyboard_create();
    lv_sdl_mousewheel_create();

    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xF5F7FA), 0);

    lab_entry_create(scr);

    lv_timer_create(lab_heartbeat, 1000, NULL);

    while(1) {
        uint32_t t = lv_timer_handler();
        lv_delay_ms(t);
    }
    return 0;
}

/* ==========================================================================
 *  >>>>>  以下都是「实验函数骨架」
 *         找到对应 lab_xxx() 删掉 TODO 写你的代码即可
 *         函数顶部注释 = 学习目标 / 必做练习 / 掌握点
 * ========================================================================== */

/* =========================================================================
 * =============================  Tab 1 : Widgets  =========================
 * ========================================================================= */

/* ================================================================
 * 实验：lab_widget_label
 * 学习目标：
 *   1. 熟悉 Label 的所有长文本策略
 *   2. 用内置 LV_SYMBOL_* 做图标（不用图片资源）
 *   3. 中文字体实测（lv_conf 里开了 LV_FONT_SIMSUN_16_CJK）
 *
 * 必做 3 个小练习：
 *   [ ] 练习 1：把同一段长文本 "The quick brown fox..." 分别用 4 种
 *              LV_LABEL_LONG_* 策略显示，观察视觉差异
 *   [ ] 练习 2：构造一个 Label = LV_SYMBOL_OK "  保存" 格式，同时显示
 *              图标和文字，用不同字号对比效果
 *   [ ] 练习 3：显示一句中文 "你好，LVGL 学习框架！"，如果显示不全，
 *              到 lv_conf.h 里确认 SIMSUN 字重开了没
 *
 * 完成后应该掌握：
 *   - 任何文本显示问题，先想：是字体没裁剪？还是 long_mode 不对？还是宽度不够？
 *   - 会用 lv_label_set_text_fmt() 做数字的实时刷新（配合定时器）
 * ================================================================ */
static void lab_widget_label(lv_obj_t * parent)
{
    lv_obj_t * card = lab_make_card(parent, "Label 练习区");

    /* TODO: 你的代码写在这里 —— 下面 3 行是占位，删了自己写 */
    lv_obj_t * hint = lv_label_create(card);
    lv_obj_set_style_text_color(hint, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_label_set_text(hint,
        "【占位提示】请到 main.c 找到 lab_widget_label() 替换 TODO 部分。\n"
        "建议：用 Flex 纵向排 4 个 Label（4 种 long_mode）+ 1 个中文 Label。");
    lab_log("已加载: lab_widget_label (等待你的练习代码)");
}

/* ================================================================
 * 实验：lab_widget_button
 * 学习目标：
 *   1. 三种点击事件的触发时机差异
 *   2. 状态样式（默认 / 按下 / 禁用 / 聚焦）
 *   3. 图标 + 文字组合 Button
 *
 * 必做 3 个小练习：
 *   [ ] 练习 1：一个按钮，三种事件各打一条日志，快速点/长按/拖出去松看差异
 *   [ ] 练习 2：不写任何回调改色，纯靠 Style，实现 "默认浅蓝/按下深蓝/禁用灰"
 *   [ ] 练习 3：再加一个 Button "禁用 A 按钮"，点一下让第一个按钮切换 DISABLED 状态
 *
 * 完成后应该掌握：
 *   - 状态样式 + LV_STATE_* 位图的搭配方法（不用每次回调里手改颜色）
 * ================================================================ */
static void lab_widget_button(lv_obj_t * parent)
{
    lv_obj_t * card = lab_make_card(parent, "Button 练习区");
    LV_UNUSED(card);
    lab_log("已加载: lab_widget_button");
}

/* ================================================================
 * 实验：lab_widget_slider_bar_arc  ⭐ 学习路线必做
 * 学习目标：
 *   1. 所有「数值型控件」的三件套 API 共性：set_range / set_value / get_value
 *   2. LV_EVENT_VALUE_CHANGED 触发频率（拖动过程中持续发）
 *   3. 「一个动作改多控件」的联动模式
 *
 * 必做 3 个小练习：
 *   [ ] 练习 1：横向 Slider + 圆形 Arc + 数字 Label 三方联动，拖任何一个其他两个同步
 *   [ ] 练习 2：把 range 改成 0~1000，Label 显示 "xx.x %" 保留 1 位小数
 *   [ ] 练习 3：再加 Bar（纵向横向都行）参与「四方联动」
 *
 * 完成后应该掌握：
 *   - 任何新增数值类控件（Spinner / Scale 等），扫一眼 API 文档就会用
 *   - 联动里避免死循环：A 改 B → B 改 A，LVGL 内部有 guard 吗？自己测试
 * ================================================================ */
static void lab_widget_slider_bar_arc(lv_obj_t * parent)
{
    lv_obj_t * card = lab_make_card(parent, "Slider + Bar + Arc 三方联动");
    LV_UNUSED(card);
    lab_log("已加载: lab_widget_slider_bar_arc (⭐ 必做联动练习)");
}

/* ================================================================
 * 实验：lab_widget_switch
 * 学习目标：
 *   1. LV_STATE_CHECKED 的读取和设置
 *   2. Switch 的「状态样式」（开/关两个视觉态）
 *   3. 和 LED / Label 的绑定
 *
 * 必做 3 个小练习：
 *   [ ] 练习 1：Switch + Label "状态: 开/关" + LED 同步亮灭（参考你之前的 demo）
 *   [ ] 练习 2：再加一个按钮"切换开关"，用 lv_obj_add_state / remove_state 改，
 *              观察 VALUE_CHANGED 事件会不会触发
 *   [ ] 练习 3：Debounce 模拟：定时器 1s 内把 Sw 切 3 次，观察事件日志次数
 *
 * 完成后应该掌握：
 *   - 「用户手动切换」和「代码强制切换」的事件差异（要不要手动发事件？）
 * ================================================================ */
static void lab_widget_switch(lv_obj_t * parent)
{
    lv_obj_t * card = lab_make_card(parent, "Switch 状态练习");
    LV_UNUSED(card);
    lab_log("已加载: lab_widget_switch");
}

/* ================================================================
 * 实验：lab_widget_chart
 * 学习目标：
 *   1. series / axis / point_count 三个核心概念
 *   2. 实时滚动：lv_chart_set_next_value vs 直接写数组后 refresh
 *   3. 双 Y 轴独立 range
 *
 * 必做 3 个小练习：
 *   [ ] 练习 1：双 Y 轴，左轴温度 0~500（放大 10 倍），右轴湿度 0~1000，双 series
 *   [ ] 练习 2：用 200ms 定时器模拟传感器，实时推 80 个点后滚动（参考你原 demo）
 *   [ ] 练习 3：算内存：chart 开 point_count=200，2 个 series，总共占多少字节？
 *              (series 内的点 = lv_coord_t = int16_t → 每个点 2B × 200 × 2 = ?)
 *
 * 完成后应该掌握：
 *   - 在 MCU RAM 紧张时，怎么压 chart 内存（点数、轴数、type）
 * ================================================================ */
static void lab_widget_chart(lv_obj_t * parent)
{
    lv_obj_t * card = lab_make_card(parent, "Chart 实时曲线");
    LV_UNUSED(card);
    lab_log("已加载: lab_widget_chart");
}

/* ================================================================
 * 实验：lab_widget_dropdown_roller
 * 学习目标：
 *   1. 静态选项 / 动态增删选项
 *   2. 两级联动（省 → 市）
 *   3. Roller 做多段时分秒选择器
 *
 * 必做 3 个小练习：
 *   [ ] 练习 1：Dropdown 选"省份"，第二个 Dropdown 根据选中动态刷新"城市"
 *   [ ] 练习 2：三个 Roller 并排（时 0-23 / 分 0-59 / 秒 0-59），下面 Label 实时显示
 *   [ ] 练习 3：给 Dropdown 加一个"自定义列表高度"的样式，滚着看 20 项
 *
 * 完成后应该掌握：
 *   - 选项类控件的「索引 vs 文本」互转：get_selected vs get_selected_str
 * ================================================================ */
static void lab_widget_dropdown_roller(lv_obj_t * parent)
{
    lv_obj_t * card = lab_make_card(parent, "Dropdown + Roller");
    LV_UNUSED(card);
    lab_log("已加载: lab_widget_dropdown_roller");
}

/* ================================================================
 * 实验：lab_widget_textarea
 * 学习目标：
 *   1. 日志模式：自动滚底 + 限制行数
 *   2. Password 模式 + 可见/隐藏按钮
 *   3. Placeholder 文本 + 光标样式
 *
 * 必做 3 个小练习：
 *   [ ] 练习 1：200ms 定时器往 TA 追加一行 "log line N"，超过 30 行删最旧的
 *   [ ] 练习 2：Password TA + 一个"眼睛图标"按钮，切换密码是否显示为明文
 *   [ ] 练习 3：放一个 lv_keyboard() widget 和 TA 联动（不用真键盘输入）
 *
 * 完成后应该掌握：
 *   - TextArea 是「多行 Label + 光标」的组合体，cursor_pos 控制能力非常强
 * ================================================================ */
static void lab_widget_textarea(lv_obj_t * parent)
{
    lv_obj_t * card = lab_make_card(parent, "TextArea 日志 + 密码框");
    LV_UNUSED(card);
    lab_log("已加载: lab_widget_textarea");
}

/* ================================================================
 * 实验：lab_widget_table  ⭐ 工业 UI 必补（你原 demo 没有）
 * 学习目标：
 *   1. Table 的行/列/单元格操作三件套
 *   2. 选中行高亮 + 行点击事件
 *   3. 单元格编辑：点单元格弹出 Roller 选值回填
 *
 * 必做 3 个小练习：
 *   [ ] 练习 1：建 12 行 × 4 列的参数表，内容随意（参数名 / 当前值 / 单位 / 备注）
 *   [ ] 练习 2：点某行，整行背景色变深蓝白字（LV_EVENT_VALUE_CHANGED）
 *   [ ] 练习 3：点"当前值"那一列单元格，弹一个 Roller 选数值，确认后回填表格
 *
 * 完成后应该掌握：
 *   - 工业产品的「参数配置页」就用 Table 做，效率最高
 * ================================================================ */
static void lab_widget_table(lv_obj_t * parent)
{
    lv_obj_t * card = lab_make_card(parent, "Table 参数配置表 ⭐");
    LV_UNUSED(card);
    lab_log("已加载: lab_widget_table (工业 UI 核心控件)");
}

/* =========================================================================
 * =============================  Tab 2 : Layouts  =========================
 * ========================================================================= */

/* ================================================================
 * 实验：lab_layout_flex_basic
 * 学习目标：
 *   1. 4 个方向 FLOW 的排列差异
 *   2. flex_align 三个参数：main / cross / track 各管什么
 *   3. wrap / no-wrap：子控件超出父宽怎么表现
 *
 * 必做 3 个小练习：
 *   [ ] 练习 1：上面放 4 个按钮切换 ROW / COLUMN / ROW_REV / COL_REV，
 *              下面 6 个彩色方块会跟着重排
 *   [ ] 练习 2：三个 Dropdown，分别选 main/cross/track 的对齐枚举值，
 *              改完立刻生效（对照文档理解每个参数）
 *   [ ] 练习 3：开 LV_FLEX_FLOW_ROW_WRAP，往父里塞 30 个 40×40 方块，
 *              缩小窗口宽度看 wrap 怎么换行
 *
 * 完成后应该掌握：
 *   - 以后拿到设计稿，一眼知道该 Flex 还是 Grid
 * ================================================================ */
static void lab_layout_flex_basic(lv_obj_t * parent)
{
    lv_obj_t * card = lab_make_card(parent, "Flex 基础：方向 + 对齐 + Wrap");
    LV_UNUSED(card);
    lab_log("已加载: lab_layout_flex_basic");
}

/* ================================================================
 * 实验：lab_layout_flex_grow  ⭐ 学习路线必做
 * 学习目标：
 *   1. lv_obj_set_flex_grow(child, N) 的语义："剩余空间按比例分 N 份"
 *   2. 固定宽度 + grow 混用：三栏布局（左固定 / 中间吃满 / 右固定）
 *
 * 必做 2 个小练习（别嫌少，这一个练习顶 3 个）：
 *   [ ] 练习 1：三栏布局，左 150px（导航色）、中 剩余全部（内容色）、右 100px（边栏色），
 *              改变父宽度，中间栏必须自适应伸缩
 *   [ ] 练习 2：中间栏放一个 TextArea（如 lab_log 区），缩窗口时不会把左/右栏压变形
 *
 * 完成后应该掌握：
 *   - 90% 的 PC 模拟器 / 大屏 UI，骨架就是 Flex-Grow 三栏
 * ================================================================ */
static void lab_layout_flex_grow(lv_obj_t * parent)
{
    lv_obj_t * card = lab_make_card(parent, "Flex-Grow 三栏布局 ⭐");
    LV_UNUSED(card);
    lab_log("已加载: lab_layout_flex_grow");
}

/* ================================================================
 * 实验：lab_layout_grid_basic
 * 学习目标：
 *   1. grid col_dsc / row_dsc 描述数组的 4 种尺寸写法（px / % / FR / CONTENT）
 *   2. set_grid_cell 的：对齐 / 列号 / 列跨度 / 行号 / 行跨度 —— 6 个参数全理解
 *   3. pad_row / pad_column 设置格子间距
 *
 * 必做 3 个小练习：
 *   [ ] 练习 1：3×3 九宫格，每个格子里一个 Label 写自己的坐标 (0,0) (1,2) ...
 *   [ ] 练习 2：FR 用法：col = {1 FR, 2 FR, 1 FR} → 中间列是左右的两倍宽
 *   [ ] 练习 3：跨行跨列：第 1 行一个大控件占满 3 列，第 2-3 行 3×2 小控件
 *
 * 完成后应该掌握：
 *   - Grid dsc 数组怎么写，FR 比例怎么设计面板骨架
 * ================================================================ */
static void lab_layout_grid_basic(lv_obj_t * parent)
{
    lv_obj_t * card = lab_make_card(parent, "Grid 基础：九宫格 + 跨行跨列");
    LV_UNUSED(card);
    lab_log("已加载: lab_layout_grid_basic");
}

/* ================================================================
 * 实验：lab_layout_grid_responsive  ⭐ 学习路线必做
 * 学习目标：
 *   1. 「卡片内部不动，只改 Grid 的 dsc 数组」实现布局剧变
 *   2. 模拟横竖屏切换 / 大小屏切换（用 Switch 触发）
 *
 * 必做 2 个小练习：
 *   [ ] 练习 1：3 个卡片（温度/湿度/转速），Switch "横屏/竖屏"：
 *              - 横屏（默认）→ 1 行 3 列（1×3）
 *              - 竖屏模式 → 3 行 1 列（3×1）
 *   [ ] 练习 2：再加一个 "2×2 模式" Switch：第 3 个卡片占满 2 列
 *              → 关键：只改 col_dsc[] / row_dsc[] + grid_cell 的 col/col_span，
 *                不要去改卡片内部的任何 label/bar/arc 代码
 *
 * 完成后应该掌握：
 *   - 一套 UI 支持多种分辨率的正确姿势：容器用 Grid，内部不用改
 * ================================================================ */
static void lab_layout_grid_responsive(lv_obj_t * parent)
{
    lv_obj_t * card = lab_make_card(parent, "Grid 横竖屏/响应式 ⭐");
    LV_UNUSED(card);
    lab_log("已加载: lab_layout_grid_responsive");
}

/* =========================================================================
 * ============================  Tab 3 : Events  ===========================
 * ========================================================================= */

/* ================================================================
 * 实验：lab_event_basic
 * 学习目标：
 *   1. 所有事件类型的"触发时机和顺序"（做对照日志看）
 *   2. lv_event_get_target vs get_current_target（先记着，下个实验看差异）
 *   3. user_data 的传递用法
 *
 * 必做 3 个小练习：
 *   [ ] 练习 1：一个按钮注册 PRESSED / PRESSING / CLICKED / LONG_PRESSED
 *              / LONG_PRESSED_REPEAT / RELEASED / SHORT_CLICKED 共 7 个过滤器
 *              每个都 lab_log() 一条，各种点击姿势都试一遍看顺序
 *   [ ] 练习 2：用 user_data 传一个整数（如 42），回调里打印出来
 *              （进阶：再传结构体指针）
 *   [ ] 练习 3：两个按钮共用同一个回调函数，靠 user_data 里的 ID 区分谁触发的
 *
 * 完成后应该掌握：
 *   - 做一个自定义控件时，哪些事件该对外暴露（通常就是 CLICKED / VALUE_CHANGED）
 * ================================================================ */
static void lab_event_basic(lv_obj_t * parent)
{
    lv_obj_t * card = lab_make_card(parent, "事件基础：7 种事件触发时机日志");
    LV_UNUSED(card);
    lab_log("已加载: lab_event_basic — 各种姿势戳按钮，看下面 Lab Log 顺序");
}

/* ================================================================
 * 实验：lab_event_bubble
 * 学习目标：
 *   1. 事件冒泡：子对象触发 → 父对象要不要也收到同一个事件
 *   2. target vs current_target 的核心区别：
 *        target = 真正被点到的那个最底层子对象
 *        current_target = 当前正在处理回调的对象（冒泡链上的某个父）
 *   3. LV_OBJ_FLAG_EVENT_BUBBLE 开关的效果
 *
 * 必做 3 个小练习：
 *   [ ] 练习 1：三层嵌套：页面(card) > 容器(row) > 按钮(btn)
 *              三层都注册 CLICKED 回调 + 各自 lab_log，点按钮看谁收得到
 *              （第一次：默认不开 bubble，看是不是只有按钮自己收到）
 *   [ ] 练习 2：给 btn 加 LV_OBJ_FLAG_EVENT_BUBBLE → 再点按钮，
 *              三层都收到事件。回调里对比：target 永远 = btn；
 *              current_target 在按钮回调里 = btn，row 回调里 = row，card 里 = card
 *   [ ] 练习 3：在中间一层（row）的回调里调 lv_event_stop(e)，
 *              看最上层 card 还能不能收到
 *
 * 完成后应该掌握：
 *   - 10 个按钮都要做同样的事？不要每个都绑回调——给父容器开 bubble，父容器一个回调全接
 * ================================================================ */
static void lab_event_bubble(lv_obj_t * parent)
{
    lv_obj_t * card = lab_make_card(parent, "事件冒泡：三层捕获 + stop()");
    LV_UNUSED(card);
    lab_log("已加载: lab_event_bubble");
}

/* ================================================================
 * 实验：lab_event_state_style  ⭐ 核心练习
 * 学习目标：
 *   1. 一张"状态位图"同时携带多个 flag（可以 PRESSED + CHECKED 同时存在）
 *   2. 样式选择器第三个参数：「selector = Part | State」组合匹配
 *   3. 任何情况不用在回调里手改颜色，全靠状态样式自动切换
 *
 * 必做 3 个小练习：
 *   [ ] 练习 1：一个按钮注册 4 套样式（DEFAULT / PRESSED / CHECKED / DISABLED），
 *              颜色要明显不同；四个外部按钮分别对应 "切 CHECKED / 切 DISABLED"
 *              每次切完看主按钮颜色有没有正确变化
 *   [ ] 练习 2：再加一个样式组合：CHECKED + PRESSED 叠加态 = 深紫色
 *              （主按钮是 CHECKED 时按下去的瞬间变紫，抬起来保持 CHECKED 色）
 *   [ ] 练习 3：改按钮 LV_PART_INDICATOR（如果是可切换型）或 KNOB 的颜色，
 *              学习「Part 概念」= 同一个 widget 内的不同绘制区
 *
 * 完成后应该掌握：
 *   - 这是 LVGL 最大的生产力提升点：以后不要在回调里写 5 行 if else 改颜色了
 * ================================================================ */
static void lab_event_state_style(lv_obj_t * parent)
{
    lv_obj_t * card = lab_make_card(parent, "状态 x 样式 = 自动切换 ⭐");
    LV_UNUSED(card);
    lab_log("已加载: lab_event_state_style — 最值得反复做的练习");
}

/* ================================================================
 * 实验：lab_event_timer_anim  ⭐ 定时器 vs 动画视觉对比
 * 学习目标：
 *   1. lv_timer_t = 你写函数自己步进，值是跳的（逐帧感）
 *   2. lv_anim_t  = LVGL 帮你插值缓动，值是连续的（丝滑感）
 *   3. 缓动路径 path_cb 的 5 种典型视觉：linear / ease_in / ease_out / ease_in_out / bounce
 *
 * 必做 3 个小练习：
 *   [ ] 练习 1：左右放两个 Slider + 两个 "启动"按钮：
 *                左：lv_timer 每 30ms 把值 +1，从 0 → 100（明显一格一格跳）
 *                右：lv_anim 时长 3000ms ease_out（丝滑，越接近顶越慢）
 *              同时启动两个，肉眼对比"值的变化曲线"
 *   [ ] 练习 2：再加一个 Bounce 缓动的动画按钮，观察反弹曲线
 *   [ ] 练习 3：在 5 种 path 之间切换动画，感受什么场景用什么缓动（ease_out 最常用）
 *
 * 完成后应该掌握：
 *   - 定时器适合"业务逻辑"（每秒采集）；动画适合"视觉过渡"（弹窗出现、按钮缩放）
 * ================================================================ */
static void lab_event_timer_anim(lv_obj_t * parent)
{
    lv_obj_t * card = lab_make_card(parent, "定时器 vs 动画：并排对比视觉差异 ⭐");
    LV_UNUSED(card);
    lab_log("已加载: lab_event_timer_anim — 感受缓动 vs 线性步进");
}

/* =========================================================================
 * ============================  Tab 4 : Porting  ==========================
 * ========================================================================= */

/* ================================================================
 * 实验：lab_porting_tick
 * 学习目标：
 *   1. lv_tick_inc() 是 LVGL 的时间基准（所有定时器/动画的唯一时钟源）
 *   2. tick 不准 / 跳变 → 动画卡 / 按键响应异常
 *   3. RTOS 下应该用什么喂 tick：SysTick 中断，还是线程里 rt_tick_get() 差值
 *
 * 必做 3 个小练习：
 *   [ ] 练习 1：显示 3 个数字：
 *              a) g_runtime_ms（lab 自己的 1s 心跳）
 *              b) lv_tick_get()（LVGL 内部 tick，SDL 喂的）
 *              c) Windows API GetTickCount() 或 SDL_GetTicks()
 *              三个值放一起每秒刷新，观察谁最准、谁可能被调度延迟
 *   [ ] 练习 2：做一个"故意不喂 tick"的 Toggle，开了后在另一个低优先级线程
 *              里 Sleep(200ms) 故意卡 200ms，再看 Slider 的动画会不会跳帧
 *   [ ] 练习 3：写一段 RT-Thread 伪代码注释：
 *              - systick 中断里 lv_tick_inc(1) 的写法
 *              - 和独立线程里每 10ms lv_tick_inc(10) 的写法
 *              各自的优缺点（中断喂=精度高/占中断；线程喂=好测/抖动大）
 *
 * 完成后应该掌握：
 *   - 以后板子动画卡，第一个查 tick（90% 的情况都是 tick 没喂对）
 * ================================================================ */
static void lab_porting_tick(lv_obj_t * parent)
{
    lv_obj_t * card = lab_make_card(parent, "Tick 时基观察");
    LV_UNUSED(card);
    lab_log("已加载: lab_porting_tick");
}

/* ================================================================
 * 实验：lab_porting_refresh
 * 学习目标：
 *   1. 脏矩形机制：只有"变了"的像素才重新渲染
 *   2. 强制全屏重绘 vs 正常局部重绘的 CPU/帧率差异
 *   3. lv_conf.h 里的 LV_LOG_LEVEL_TRACE + LV_LOG_TRACE_MEM 开了看
 *
 * 必做 3 个小练习：
 *   [ ] 练习 1：开日志到 TRACE 级别（改 lv_conf.h 的 LV_LOG_LEVEL），
 *              点一个按钮看日志里「invalidate area」的坐标和面积，通常很小
 *   [ ] 练习 2：加一个按钮"强制全屏无效"：lv_obj_invalidate(scr)，
 *              看日志里重绘面积 = 全屏，肉眼对比两帧间 CPU 占用
 *   [ ] 练习 3：把 chart 的 point_count 从 80 加到 500，再连续推数据，
 *              看无效面积是"整个 chart"还是只"列宽 1px 的带"
 *              （这就是 LVGL 针对 chart 的局部刷新优化）
 *
 * 完成后应该掌握：
 *   - 板子刷新慢，先看"每次 invalidated area 有多大"：全屏 = 代码哪里乱 invalidate 了
 * ================================================================ */
static void lab_porting_refresh(lv_obj_t * parent)
{
    lv_obj_t * card = lab_make_card(parent, "刷新 / 脏矩形监控");
    LV_UNUSED(card);
    lab_log("已加载: lab_porting_refresh — 记得把 lv_conf.h 日志开到 TRACE");
}

/* ================================================================
 * 实验：lab_porting_mutex_skeleton（占位骨架，暂时不引入 pthread）
 * 学习目标：
 *   1. LVGL API 非线程安全：A 线程改 UI，B 线程也改，一定崩
 *   2. 正确姿势：所有 LVGL API 调用前后拿同一个 Mutex
 *   3. flush_cb + DMA 完成中断的 Semaphore 同步机制（双缓冲/乒乓缓冲）
 *
 * 必做 3 个小练习（以"写伪代码注释"为主，真机/模拟器到 RTOS 移植再实装）：
 *   [ ] 练习 1：在 card 里放一个大 Label，列 8 行伪代码：
 *              - Mutex 创建 / lvgl_thread_entry / sensor_thread_entry
 *              - 跨线程调 lv_bar_set_value 前后加解锁对
 *   [ ] 练习 2：再放 flush_cb 的伪代码 + DMA IRQHandler 释放 sem + lv_display_flush_ready()
 *   [ ] 练习 3：思考「死锁怎么发生」—— sensor 拿 mutex 等 DMA；
 *              flush 拿 sem 等 lv_timer_handler 拿 mutex？画一个 4 步的时序分析
 *
 * 完成后应该掌握：
 *   - RTOS 下的 LVGL 初始化 8 步伪代码（学习路线阶段 5 的要求）能默写
 *
 * 进阶：真要在 PC 上跑 Mutex 实验，把 Windows CRITICAL_SECTION 或 pthread 引入，
 *       两个线程同时 lv_bar_set_value 100 万次，看不加锁 vs 加锁的崩溃率差异。
 * ================================================================ */
static void lab_porting_mutex_skeleton(lv_obj_t * parent)
{
    lv_obj_t * card = lab_make_card(parent, "RTOS Mutex/Sem 同步机制（骨架占位）");
    LV_UNUSED(card);
    lab_log("已加载: lab_porting_mutex_skeleton — 写伪代码过一遍流程");
}

/* =========================================================================
 * ============================  Tab 5 : Sandbox  ==========================
 * ========================================================================= */

/* ================================================================
 * 实验：lab_sandbox — 自由沙盒
 * 规则：
 *   - 随便写，任何临时测的东西都扔这里
 *   - 想保留的成果，记得搬到对应 Tab 的 lab_xxx() 里整理
 * ================================================================ */
static void lab_sandbox(lv_obj_t * parent)
{
    lv_obj_clean(parent);
    lv_obj_t * card = lab_make_card(parent, "Sandbox 自由沙盒 — 随便造");
    lv_obj_t * tip = lv_label_create(card);
    lv_label_set_text(tip,
        "这里是你的实验田，\n"
        "任何想快速测的想法直接写进 lab_sandbox() 里。\n"
        "\n"
        "如果想保留：请把代码搬到 1.Widgets / 2.Layouts 等对应 Tab 的实验函数里。");

    /* TODO: 你的临时代码从这里开始 */

    lab_log("已加载: lab_sandbox — 随便造，大不了切回其他 Tab 不会互相影响");
}
