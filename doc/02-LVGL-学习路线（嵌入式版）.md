# LVGL 学习路线（嵌入式开发者版）

面向有 RT-Thread / RTOS / 驱动开发背景的开发者，共 **6 个阶段**，建议总学习周期 **3~5 周**（每天 1~2 小时）。

> 核心原则：**不要死记 API，先建立架构心智模型，再基于当前项目做改动实验。**
> 你的 `lvgl-in-windows` 项目（LVGL v9 + SDL2 模拟器 + 现成的控制面板 demo）就是最好的起点。

---

## 阶段 0（前置）：先看「UI 通用概念」文档

**时间**：1 天

**必做**：
- 阅读 `doc/01-UI-通用概念速查手册.md`
- 对照 `main.c`，把 10 个概念在代码里的对应位置都点一遍
- 搞懂 `main()` 的 while 循环在干嘛、`lv_timer_handler()` 是什么地位

**通关标准**：
- 能指着 `page_monitor_create()` 随口说出：哪些是控件、哪些是属性、哪些是样式、谁是谁的父、Z 顺序怎么排的
- 能说出"事件和中断的 3 个类比点"

---

## 阶段 1：搞懂 LVGL 的 4 个底层概念

**时间**：1~2 天（不要写大量新代码，以读源码 + 看文档 + 小改验证为主）

这 4 个概念是「LVGL 架构骨架」，没搞懂就写代码会变成"API 拼接怪"。

| 概念 | 看什么 | 对应源码/文档 |
|---|---|---|
| **obj 树**（一切皆对象，父子继承） | 读 `page_monitor_create` 的嵌套结构，对照 `lv_obj_scroll` / `lv_obj_tree` 两个文件 | `lvgl/src/core/lv_obj.h` + `lv_obj_tree.c` |
| **Style 系统**（样式、状态、Part 三维组合） | 把 `card_temp` 改成别的颜色/圆角/边框，观察每改一个参数 UI 怎么变 | `lvgl/docs/overview/style.rst` + `style-props.rst`（所有属性清单，当字典查） |
| **Event 回调模型**（target / current_target / user_data 的区别） | 写一个按钮，回调里打印 `lv_event_get_target()` 和 `lv_event_get_current_target()`，看按钮 vs 父容器谁收到事件 | `lvgl/docs/overview/event.rst` + `lvgl/src/misc/lv_event.c` |
| **Display / Refresh 刷新周期**（脏矩形 + flush_cb + 帧缓冲策略） | 打开 LVGL 日志 (lv_conf.h 里 `LV_LOG_LEVEL_DEBUG`)，看每次刷新打印的「重绘区域坐标和面积」 | `lvgl/docs/overview/display.rst` + `lvgl/docs/porting/display.rst` |

**动手练习（小改验证）**：
1. 把 `card_temp` 的背景色换成红色，再给它加 2px 宽的蓝色边框、圆角改成 20px
2. 尝试在 btn_save 回调里，同时打印 target 和 current_target（开启 bubble 对比差异）
3. 日志级别开 DEBUG 后，拖动 speed 滑块，观察日志中「invalidated area」的面积变化

**通关标准**：
- 知道「为什么 LVGL 不需要全屏重绘」，能解释脏矩形的 3 个好处
- 能自己写出"按钮默认浅蓝 / 按下深蓝 / 禁用灰色"的样式（不用回调改色，靠状态样式）

---

## 阶段 2：用「8 个最常用 widget」练手

**时间**：3~5 天

嵌入式 UI 80% 的场景只需要这 8 个 widget。不要看全量 widget（40+ 个），先把这 8 个用熟，其他 widget 都是"这 8 个的变体"。

**学习方法**：在 `main.c` 的 TabView 里**新增第三个 Tab 叫 "Experiments"**，每个 widget 写一个独立的创建函数。

### 8 个核心 Widget 清单及学习要点

| # | Widget | 你代码里已有？ | 学习要点（必须实践） |
|---|---|---|---|
| 1 | **Label** (文本) | ✅ 大量使用 | 长文本换行 `LV_LABEL_LONG_WRAP` / 滚动 `_SCROLL_CIRCLE` / 省略 `_DOT`；`LV_SYMBOL_*` 内置图标；**中文字体裁剪**（lv_conf 里开了 `SIMSUN_16_CJK`，实测显示中文有没有缺字） |
| 2 | **Button** (按钮) | ✅ btn_save / btn_reset | 长按事件 `LV_EVENT_LONG_PRESSED`；重复点击 `_LONG_PRESSED_REPEAT`；**按钮 + label + 图标 组合**（已有 `LV_SYMBOL_SAVE` 用例）；禁用状态样式 |
| 3 | **Slider / Bar / Arc** | ✅ 全都有 | 做一个「滑块 ↔ Arc ↔ 数字 Label 三方联动」（改任意一个，另外两个自动同步）；Range Bar（双滑块选区间，做阈值设置用） |
| 4 | **Switch** (开关) | ✅ sw_power / sw_alarm | 模拟 debounce（快速连点 10 次观察是否有"丢事件"）；**开关 + 事件冒泡**：父容器也能收到 VALUE_CHANGED |
| 5 | **Chart** (图表) | ✅ g_chart | 双 Y 轴（已有）+ Cursor 标线；实时滚动 vs 模式切换；散点图 `LV_CHART_TYPE_SCATTER`；**Chart 的数据点内存占用计算**（这是嵌入式重点） |
| 6 | **Dropdown / Roller** | ✅ dd_mode | 动态增删选项；多级菜单（用两个 dropdown 做联动）；Roller 做"时分秒选择器" |
| 7 | **TextArea** (文本框) | ✅ g_log_ta | **自动滚到底部**（日志场景）；限制最大行数自动丢弃最旧；**Password 模式**；Placeholder + 光标样式；和 Keyboard widget 联动（lv_conf 里 `LV_USE_KEYBOARD` 打开） |
| 8 | **Table** (表格) ⚠️ | ❌ 没用到 | 必须补！嵌入式 UI 大量用表格（参数配置表、版本信息、寄存器 Dump）。做一个「10 行 × 4 列参数配置表」：点击单元格弹出 Roller 选值，支持选中行高亮 |

**本阶段练习模板**（直接往 Experiments Tab 里塞）：
```c
static void page_experiments_create(lv_obj_t * parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 14, 0);
    lv_obj_set_style_pad_row(parent, 10, 0);

    /* 一个实验一个函数，保持代码整洁 */
    exp_label_create(parent);
    exp_button_create(parent);
    exp_triple_link_create(parent);     // 滑块+Arc+Label 三方联动
    exp_table_create(parent);           // ⭐ 参数配置表
    // ...
}
```

**通关标准**：
- 上面 8 个 widget，每个都能"闭眼写出创建 → 设置属性/样式 → 注册事件回调"三件套
- Table 那个配置表能正常用

---

## 阶段 3：掌握 Flex + Grid 两种布局

**时间**：2~3 天

LVGL v9 最重要的升级就是这两个布局引擎。**不要用绝对坐标手写定位**（v8 时代的老方法），能 Flex/Grid 解决的一律用布局引擎 —— 换分辨率时才不会重写一遍。

### Flex（弹性盒子）= 一维布局（单行 or 单列）

学习要点：
1. **4 个方向**：`ROW` / `COLUMN` / `ROW_REVERSE` / `COLUMN_REVERSE`
2. **3 个对齐参数**：`flex_align(main, cross, track)` —— 三个参数到底管什么，做实验搞懂
3. **子控件拉伸**：`lv_obj_set_flex_grow(child, 1)` 让子控件"吃掉剩余空间"（类似 CSS 的 flex-grow）
4. **子控件排序**：`lv_obj_move_to_index(child, N)` 动态调顺序

对应代码：你 `page_monitor_create` 顶部的 header（`LV_FLEX_FLOW_ROW` + `SPACE_BETWEEN`）就是 Flex 最佳入门样例。

### Grid（网格）= 二维布局（多行多列）

学习要点：
1. **描述数组**：`lv_coord_t col_dsc[] = {100, 200, LV_PCT(50), LV_GRID_TEMPLATE_LAST}`
   - 每列可以是像素、百分比、`LV_GRID_CONTENT`(内容宽度)、`LV_GRID_FR(N)`(剩余空间分 N 份)
2. **单元格占位**：`lv_obj_set_grid_cell(child, align, col, col_span, row, row_span)` —— 跨行跨列的写法
3. **行列间距**：`LV_STYLE_PAD_ROW` / `LV_STYLE_PAD_COLUMN`（不是 grid-gap）

对应代码：你 `grid` 里的 3 个仪表盘卡片（33% / 33% / 33%）就是最小 Grid 示例。

### 本阶段动手练习（必做）

**练习 1：重排仪表盘**
- 把原来 1 行 3 列的仪表盘卡片，改成「窄屏 3 行 1 列」「宽屏 2 行 2 列 + 最后一个占 2 列」两种布局切换（用 Switch 切换模拟"横竖屏切换"）
- 关键：只改 grid 的 dsc 数组，不要改 card 内部的任何代码

**练习 2：Flex-Grow 排三栏**
- 做一个 3 栏布局：左栏固定 150px，右栏固定 100px，中间栏吃掉所有剩余空间
- 在中间栏放一个 TextArea，观察改变窗口大小时中间栏是否自动伸缩

**通关标准**：
- 拿到一个 UI 设计稿截图，能**立刻说出**哪块用 Flex、哪块用 Grid、dsc 数组大概长什么样
- 上面两个练习完成

---

## 阶段 4：拆官方 Demos，找「设计模式」

**时间**：3~5 天

LVGL 自带 demos 不是给你看热闹的，是给你抄的。**每个 demo 对应一种典型产品 UI 范式**。

### 4 个必拆 Demo

在 lv_conf.h 里把对应 `LV_USE_DEMO_*` 打开，然后在 `my_ui_create()` 里先调 demo 看效果 → 再读源码 → 再把你喜欢的片段「抠出来」放到 Experiments Tab。

| Demo | 路径入口 | 对应产品场景 | 你要抄什么（嵌入式落地的设计模式） |
|---|---|---|---|
| **lv_demo_widgets** | `lvgl/demos/lv_demos.c` + `widgets/` 子目录 | 通用工业控制面板 | ① 每种 widget 的"标准状态样式套"直接抄；② 左右分栏（侧边导航 + 内容区）结构；③ 键盘/编码器输入的焦点管理（非常重要，工业屏大多非触屏） |
| **lv_demo_music** | `lvgl/demos/music/lv_demo_music.c` | 播放器 / 智能音箱 / 列表为主的应用 | ① 大 List 的滚动性能优化；② 侧边栏弹出/收起动画；③ TabBar（顶部标签）切换过渡；④ 封面图圆角遮罩 |
| **lv_demo_stress** | `lvgl/demos/stress/lv_demo_stress.c` | 内存 & 帧率压力测试基准 | ⭐⭐⭐ **嵌入式必抄**：① 运行时内存占用曲线；② FPS 实时显示；③ 连续创建删除 widget 测泄漏（`LV_USE_ASSERT_MEM_INTEGRITY` 打开） |
| **lv_demo_scroll** | `lvgl/demos/scroll/lv_demo_scroll.c` | 长列表 / 配置页滚动 | ① `LV_OBJ_FLAG_SCROLL_*` 几个 flag 的组合效果；② Scroll 到顶/底回调 + 弹性效果；③ 嵌套滚动（容器里嵌 List）；④ `lv_obj_scroll_to_view()` 把某个控件滚到可视区（做"定位到第 N 项"） |

### 每个 Demo 的拆解步骤（不要跳步）：

1. **运行观察**（1h）：直接 `lv_demo_xxx_create(lv_screen_active())` 跑起来，每个按钮都点一遍，截图 3~5 张不同状态
2. **分层读源码**（2h）：
   - 第一层：入口函数怎么分模块（对应哪些 UI 块）
   - 第二层：每个模块的布局结构（Flex？Grid？父是谁？子是谁？）
   - 第三层：挑一个 widget 读「样式注册 → 事件回调 → 状态切换」三件套
3. **抠代码**（1h）：把你最喜欢的那个模块，**尽量少改**地搬到你的 Experiments Tab。改崩了就对比原 demo 找差异。
4. **改实验**（1h）：给抠出来的模块动 3 个小手术（改颜色、改布局、换图标），观察 UI 怎么变。

**通关标准**：
- 4 个 demo 至少拆 2 个（推荐 stress 必拆 + widgets/music 二选一）
- Experiments Tab 里至少有一个从 demo 抠出来的可运行模块

---

## 阶段 5：模拟「嵌入式移植」，吃透 Porting 层

**时间**：5~7 天

这是 **你和纯前端 GUI 学习者的分水岭**。嵌入式项目最终要跑在真实 MCU + LCD + TP 上，SDL 模拟器只是个学习工具——这一步要做的是"思维实验 + 动手模拟"。

### 学习方法（4 个思考题 + 动手改代码）

#### 思考题 1：如果把 SDL 换成 SPI 接口的 2.4 寸 TFT-LCD（240×320，ILI9341），我要改哪些代码？

去读 porting/display.rst，对照 SDL 驱动的最小实现看差异：
- SDL 帮你封装的 `flush_cb` 里实际做了什么？
- ILI9341 的 `flush_cb` 应该写成什么样？（伪代码也行，但要体现「设置写区域 + SPI 送 GRAM」两步）
- 「全屏缓冲 / Partial 双小缓冲」两种策略下，`LV_DISP_RENDER_MODE_*` 和 `buf_size` 怎么选？

参考源码：`lvgl/src/draw/sdl/lv_draw_sdl.c` + `lvgl/src/display/lv_display.c`

#### 思考题 2：如果把 SDL 鼠标换成 I2C 触控芯片 FT5406，read_cb 怎么写？

读 porting/indev.rst，对照 `lv_sdl_mouse.c`：
- FT5406 datasheet 里「一次触控点数据 = 状态位 + X_H + X_L + Y_H + Y_L」的格式，怎么填到 `data->point` 和 `data->state`？
- 多点触控的话，LVGL 现在支持几个点？（查文档：v9 原生只支持单点，多点要上层做手势识别）

参考源码：`lvgl/src/drivers/sdl/lv_sdl_mouse.c`

#### 思考题 3：如果不用 SDL 的 main() 循环，而是跑在 RT-Thread 下，调度怎么搞？ ⭐⭐⭐

这是嵌入式落地**最核心**的问题，必须搞懂。读 porting/os.rst，然后动手改代码：

**改前**（当前 SDL 模拟器）：
```c
while(1) {
    uint32_t t = lv_timer_handler();
    lv_delay_ms(t);   // 单线程阻塞
}
```

**改后**（RT-Thread 多线程版，伪代码，能写多少写多少）：
```c
/* 1. 互斥锁：LVGL API 非线程安全，跨线程调必须加锁 */
static rt_mutex_t lvgl_lock;

/* 2. 二值信号量：flush_cb 完成后发，DMA 传输完成中断里释放 */
static rt_sem_t flush_done_sem;

/* 3. 心跳：systick 中断里喂 lv_tick_inc(1) —— LVGL 的时间基准 */
void rt_hw_systick_handler(void) { lv_tick_inc(1); ... }

/* 4. LVGL 专属线程（独立栈 8KB~16KB，优先级中等） */
static void lvgl_thread_entry(void *param) {
    while(1) {
        rt_mutex_take(lvgl_lock, RT_WAITING_FOREVER);
        uint32_t t = lv_timer_handler();
        rt_mutex_release(lvgl_lock);
        rt_thread_mdelay(t);
    }
}

/* 5. 传感器采集线程（另一个线程，想改 UI 必须拿锁） */
static void sensor_thread_entry(void *param) {
    while(1) {
        float temp = read_temp_sensor();
        rt_mutex_take(lvgl_lock, RT_WAITING_FOREVER);
        lv_bar_set_value(g_temp_bar, (int32_t)(temp*10), LV_ANIM_ON);   // 跨线程调用 LVGL API
        rt_mutex_release(lvgl_lock);
        rt_thread_mdelay(1000);
    }
}

/* 6. flush_cb：当用 LTDC / DMA2D 时，刷完要给 sem */
static void my_flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *buf) {
    /* 启动 LTDC / DMA2D / SPI DMA 传输... */
    dma2d_start_transfer(...);
    /* 不用等立刻返回，在 DMA 完成中断里释放 flush_done_sem */
}

/* DMA 传输完成中断 */
void DMA2D_IRQHandler(void) {
    lv_display_flush_ready(disp);   // 告诉 LVGL 我刷完了
    rt_sem_release(flush_done_sem); // 如果用阻塞模式的话
}
```

**动手练习**：即使不装 RT-Thread，也要在当前 PC 项目里「用 pthreads 模拟」上面 3 个核心 API 的**用法**（`pthread_mutex_t` 替代 `rt_mutex`，`usleep` 替代 `rt_thread_mdelay`）——感受"加锁前后会不会数据竞争崩溃"。

#### 思考题 4：Tick 时基从哪里来？

读 porting/tick.rst：
- PC SDL 下 `lv_tick_inc()` 是谁在喂？（SDL 的后台线程）
- RTOS 下必须用什么喂？（systick 中断 or 定时器中断，精度 1ms 足够）
- 如果 Tick 比 `lv_timer_handler()` 的调度间隔还慢，会出现什么症状？（动画卡顿、按键丢响应）

### 本阶段通关标准

- 能在白纸上写出「真实板子（STM32H7 + RGB-LTDC + FT5406 + RT-Thread）上 LVGL 初始化的 8 步代码框架伪代码」
- 能说清楚「为什么 RTOS 下必须加 Mutex、不加会出什么 bug、**死锁怎么发生**」
- 能说出 3 种帧缓冲策略分别适合多大 RAM 的 MCU

---

## 阶段 6（按需深入）：5 个进阶专题

做项目时遇到了再学，不用一次性啃完。按嵌入式项目落地的频率排序：

| 专题 | 推荐优先级 | 学习入口 | 你需要掌握什么 |
|---|---|---|---|
| **内存管理**（接 RT-Thread memheap） | ⭐⭐⭐⭐⭐ | `lv_conf.h` 的 `LV_USE_STDLIB_MALLOC` + `LV_MEM_CUSTOM` 相关注释 + `lvgl/src/misc/lv_fs.c` | ① 能不能把 LVGL 从 heap 换成自定义 memheap 分区？② `LV_MEM_SIZE` 该设多大（经验值：全屏像素数 × 色深 × 4 + 40KB 控件元数据） |
| **字体裁剪 & 自定义** | ⭐⭐⭐⭐⭐ | `lvgl/docs/overview/font.rst` + lv_font_conv 工具（Node.js 或在线版） | 嵌入式项目中文字库必须裁剪！① 用 lv_font_conv 生成只含你项目用到的 500 字的字库；② 对比压缩比：bpp=1/2/4/8 的 ROM 占用 vs 美观度；③ 字体混排（中文用 SIMSUN 14，英文数字用 Montserrat 14，如何自动切换） |
| **图片资源 + 解码** | ⭐⭐⭐⭐ | `lvgl/docs/libs/lodepng.rst` / `tjpgd.rst` / `rle.rst` | ① 3 种图片方案的取舍：C 数组直接编进 ROM（最省 RAM，启动最快）/ 解码 PNG/JPG（省 ROM，费 RAM 和 CPU）/ RLE 压缩（中间档）；② `lv_img_dsc_t` 结构体每个字段含义；③ 图片调色板 + Alpha 通道 |
| **自定义 Widget** | ⭐⭐⭐ | `lvgl/docs/overview/new_widget.rst` + 对照 `src/widgets/bar/lv_bar.c` 读源码 | 工业屏常要做「自定义仪表盘（gauge）」「自定义环形进度」——学会继承 lv_obj 基类、实现 constructor、draw_cb、event_cb 四件套 |
| **性能调优 & Profiler** | ⭐⭐⭐⭐ | `lvgl/docs/overview/profiler.rst` + `LV_USE_PROFILER` 宏 + Perfetto UI | ① 开 FPS 监控（stress demo 里抄）；② 用 Perfetto 火焰图看「哪个 draw 函数最吃 CPU」；③ 5 个常见优化手段：减少 alpha、禁用阴影、减少重绘面积、开 Partial 缓冲、用 Draw SDL/DMA2D 硬件加速 |

---

## 学习总进度检查清单（Checklist）

每个阶段完成后打勾，确保没漏：

- [ ] 阶段 0：UI 10 个概念 × main.c 映射完成
- [ ] 阶段 1：4 个底层概念 + Style×State 自动切换 写出来了
- [ ] 阶段 2：8 个核心 widget 各写一个 demo（Table 含配置功能）
- [ ] 阶段 3：横竖屏切换练习 + 三栏 Flex-Grow 练习完成
- [ ] 阶段 4：拆了 stress demo + 另 1 个，各抠出 1 个模块可用
- [ ] 阶段 5：写出 RT-Thread + LTDC + FT5406 移植的 8 步初始化伪代码
- [ ] 阶段 6（按需）：中文字体裁剪 OK；内存池方案定了；FPS 能测了

---

## 附：遇到问题时的 4 步排查法（嵌入式开发者习惯的思路）

1. **先看日志**：`LV_LOG_LEVEL_TRACE` 全开，对照时间戳看「崩溃前最后一个正常事件是什么」
2. **最小化复现**：把复杂 UI 代码一行行注释掉，直到找到"哪一加了就崩"的最小复现场景
3. **对照 demo 比差异**：官方 demo 能跑，你的不能跑 → 逐行对比 API 调用顺序，90% 的 bug 是「调用顺序错了 / 父对象传 nullptr / 样式状态位写错」
4. **怀疑内存就开断言**：`LV_USE_ASSERT_NULL` / `LV_USE_ASSERT_MALLOC` / `LV_USE_ASSERT_MEM_INTEGRITY` 三个全开，断言触发点就是崩溃的近因（比 core dump stack 好用得多）
