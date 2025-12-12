// ui.c
#include "ui.h"
#include "lvgl.h"
#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "ui";   

// === 全局UI对象引用 ===
static lv_obj_t *top_bar;
static lv_obj_t *status_container;
static lv_obj_t *button_container;
static lv_obj_t *log_textarea;
static lv_obj_t *bottom_bar;

// === 按钮回调存储 ===
static ui_btn_callback_t g_button_callbacks[UI_BUTTON_COUNT] = {NULL};

// === 日志缓冲区 ===
static char g_log_buffer[UI_LOG_MAX_LINES][128];
static uint8_t g_log_index = 0;
static uint8_t g_log_count = 0;

// === 状态项缓存 ===
typedef struct {
    char key[16];
    char value[32];
    lv_color_t color;
    bool valid;
} status_item_t;

static status_item_t g_status_items[UI_STATUS_MAX_ITEMS] = {0};

// === 按钮点击事件回调 ===
static void button_event_handler(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    uintptr_t id = (uintptr_t)lv_obj_get_user_data(btn);
    if (id < UI_BUTTON_COUNT && g_button_callbacks[id]) {
        g_button_callbacks[id]();
    }
}

// === 初始化顶部状态栏 ===
static void init_top_bar(void) {
    top_bar = lv_obj_create(lv_scr_act());
    lv_obj_set_size(top_bar, LV_PCT(100), 30);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_color(top_bar, lv_color_white(), 0);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_set_style_pad_all(top_bar, 0, 0); //

    lv_obj_t *label = lv_label_create(top_bar);
    lv_label_set_text(label, "Firmware: - | Ver: -");
    lv_obj_center(label);
}

// === 初始化主状态区 ===
static void init_status_area(void) {
    status_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(status_container, LV_PCT(100), 80);
    lv_obj_set_style_border_width(status_container, 0, 0);
    lv_obj_set_style_bg_color(status_container, lv_color_hex(0x1e1e1e), 0);
    lv_obj_align_to(status_container, top_bar, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    // 设置为 Flex 布局：行换行 + 均匀分布
    lv_obj_set_flex_flow(status_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(status_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < UI_STATUS_MAX_ITEMS; i++) {
        lv_obj_t *item = lv_obj_create(status_container);
        lv_obj_set_size(item, LV_PCT(15), 60); // 每个 item 占 30% 宽度
        lv_obj_set_style_border_width(item, 0, 0);
        lv_obj_set_style_bg_color(item, lv_color_hex(0x2a2a2a), 0);
        lv_obj_set_style_pad_all(item, 5, 0);

        lv_obj_t *key_label = lv_label_create(item);
        lv_label_set_text(key_label, "Key");
        lv_obj_set_style_text_color(key_label, lv_color_white(), 0);
        lv_obj_align(key_label, LV_ALIGN_TOP_LEFT, 0, 0);

        lv_obj_t *value_label = lv_label_create(item);
        lv_label_set_text(value_label, "Value");
        lv_obj_set_style_text_color(value_label, lv_color_hex(0x00FF00), 0);
        lv_obj_align(value_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

        lv_obj_set_user_data(item, (void*)(uintptr_t)i);
    }
}

// === 初始化按钮区 ===
static void init_button_area(void) {
    button_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(button_container, LV_PCT(100), 80);
    lv_obj_set_flex_flow(button_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    // 关键：清除内边距，避免意外溢出
    lv_obj_set_style_pad_all(button_container, 0, 0);
    lv_obj_set_style_border_width(button_container, 0, 0);
    lv_obj_set_style_bg_color(button_container, lv_color_hex(0x2a2a2a), 0);
    lv_obj_align_to(button_container, status_container, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    for (int i = 0; i < UI_BUTTON_COUNT; i++) {
        lv_obj_t *btn = lv_btn_create(button_container);
        lv_obj_set_size(btn, 180, 40);
        lv_obj_set_style_height(btn, 70, 0);
        lv_obj_set_user_data(btn, (void*)(uintptr_t)i);

        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, "N/A");
        lv_obj_center(label);

        lv_obj_add_event_cb(btn, button_event_handler, LV_EVENT_CLICKED, NULL);
    }
}

// === 初始化日志区 ===
static void init_log_area(void) {
    lv_obj_t *log_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(log_container, LV_PCT(100), 240);
    lv_obj_set_style_border_width(log_container, 0, 0);
    lv_obj_set_style_bg_color(log_container, lv_color_hex(0x0d0d0d), 0);
    lv_obj_align_to(log_container, button_container, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    log_textarea = lv_textarea_create(log_container);
    lv_textarea_set_text(log_textarea, "");
    lv_obj_set_size(log_textarea, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(log_textarea, lv_color_black(), 0);
    lv_obj_set_style_text_color(log_textarea, lv_color_hex(0x00FF00), 0);
    lv_obj_set_scrollbar_mode(log_textarea, LV_SCROLLBAR_MODE_AUTO); // 自动滚动条
    lv_textarea_set_one_line(log_textarea, false); // 多行
}

// === 初始化底部状态栏 ===
static void init_bottom_bar(void) {
    bottom_bar = lv_obj_create(lv_scr_act());
    lv_obj_set_size(bottom_bar, LV_PCT(100), 40);
    lv_obj_set_style_border_width(bottom_bar, 0, 0);
    lv_obj_set_style_bg_color(bottom_bar, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_color(bottom_bar, lv_color_white(), 0);
    lv_obj_align(bottom_bar, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t *label = lv_label_create(bottom_bar);
    lv_label_set_text(label, "IP: - | Baud: - | FW: -");
    lv_obj_center(label);
}

// === 线程内绘制 ===
void _ui_refresh_status(void) {
    for (int i = 0; i < UI_STATUS_MAX_ITEMS; i++) {
        lv_obj_t *item = lv_obj_get_child(status_container, i);
        if (g_status_items[i].valid) {
            lv_obj_t *key_label = lv_obj_get_child(item, 0);
            lv_obj_t *value_label = lv_obj_get_child(item, 1);
            lv_label_set_text(key_label, g_status_items[i].key);
            lv_label_set_text(value_label, g_status_items[i].value);
            lv_obj_set_style_text_color(value_label, g_status_items[i].color, 0);
        } else {
            lv_obj_t *key_label = lv_obj_get_child(item, 0);
            lv_obj_t *value_label = lv_obj_get_child(item, 1);
            lv_label_set_text(key_label, "");
            lv_label_set_text(value_label, "");
        }
    }
}

void _ui_set_top_firmware_info(const char* name, const char* version) {
    static char buf[128];
    snprintf(buf, sizeof(buf), "Firmware: %s | Ver: %s", name ? name : "-", version ? version : "-");
    lv_obj_t *label = lv_obj_get_child(top_bar, 0);
    lv_label_set_text(label, buf);
}

void _ui_set_status_item(int index, const char* key, const char* value, lv_color_t color) {
    if (index < 0 || index >= UI_STATUS_MAX_ITEMS) return;
    strncpy(g_status_items[index].key, key ? key : "", sizeof(g_status_items[index].key) - 1);
    strncpy(g_status_items[index].value, value ? value : "", sizeof(g_status_items[index].value) - 1);
    g_status_items[index].color = color;
    g_status_items[index].valid = true;
    ui_refresh_status();
}

void _ui_set_button(int index, const char* text, ui_btn_callback_t callback) {
    if (index < 0 || index >= UI_BUTTON_COUNT) return;
    g_button_callbacks[index] = callback;
    lv_obj_t *btn = lv_obj_get_child(button_container, index);
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    lv_label_set_text(label, text ? text : "N/A");
}

// === 真正执行日志写入和显示刷新（仅在 LVGL 任务中调用！）===
static char g_log_display_buf[UI_LOG_MAX_LINES * 128];
void _ui_add_log_from_lvgl(const char* formatted_msg) {
    if (!formatted_msg) return;

    // 写入环形缓冲区
    strncpy(g_log_buffer[g_log_index], formatted_msg, sizeof(g_log_buffer[0]) - 1);
    g_log_buffer[g_log_index][sizeof(g_log_buffer[0]) - 1] = '\0'; // 确保终止

    g_log_index = (g_log_index + 1) % UI_LOG_MAX_LINES;
    if (g_log_count < UI_LOG_MAX_LINES) g_log_count++;

    // 安全地更新 UI
    
    g_log_display_buf[0] = '\0';
    size_t pos = 0;

    for (int i = 0; i < g_log_count; i++) {
        int idx = (g_log_index - g_log_count + i + UI_LOG_MAX_LINES) % UI_LOG_MAX_LINES;
        size_t line_len = strlen(g_log_buffer[idx]);
        if (pos + line_len + 1 >= sizeof(g_log_display_buf)) break;

        memcpy(g_log_display_buf + pos, g_log_buffer[idx], line_len);
        pos += line_len;
        g_log_display_buf[pos++] = '\n';
    }
    g_log_display_buf[pos] = '\0';

    lv_textarea_set_text(log_textarea, g_log_display_buf);
    lv_textarea_set_cursor_pos(log_textarea, LV_TEXTAREA_CURSOR_LAST);
}

void _ui_set_bottom_info(const char* ip, uint32_t baudrate, const char* firmware_id) {
    static char buf[128];
    snprintf(buf, sizeof(buf), "IP: %s | Baud: %lu | FW: %s",
             ip ? ip : "-", (unsigned long)baudrate, firmware_id ? firmware_id : "-");
    lv_obj_t *label = lv_obj_get_child(bottom_bar, 0);
    lv_label_set_text(label, buf);
}
static void _ui_clear_log(void) {
    // 清空环形缓冲区状态
    g_log_index = 0;
    g_log_count = 0;
    // 可选：显式清零内存（非必需，但更干净）
    memset(g_log_buffer, 0, sizeof(g_log_buffer));

    // 清空文本框
    lv_textarea_set_text(log_textarea, "");
}
// 声明内部刷新函数（仅在 LVGL 任务中调用）
static void _ui_apply_msg(const ui_msg_t* msg) {
    switch (msg->type) {
        case UI_MSG_SET_TOP:
            // 假设你有 top_label_name, top_label_version
            _ui_set_top_firmware_info(msg->data.top.name,msg->data.top.version);
            break;

        case UI_MSG_SET_STATUS_ITEM:
            _ui_set_status_item(msg->data.status_item.index,msg->data.status_item.key,msg->data.status_item.value,msg->data.status_item.color);
            break;

        case UI_MSG_SET_BUTTON:
            _ui_set_button(msg->data.button.index,msg->data.button.text,msg->data.button.callback);
            break;

        case UI_MSG_ADD_LOG:
            _ui_add_log_from_lvgl(msg->data.log.msg); // 你已有的 log 控件更新函数
            break;

        case UI_MSG_SET_BOTTOM:
            _ui_set_bottom_info(msg->data.bottom.ip,(unsigned long)msg->data.bottom.baudrate,msg->data.bottom.firmware_id);
            break;

        case UI_MSG_REFRESH_STATUS:
            _ui_refresh_status();
            break;
        case UI_MSG_CLEAR_LOG:
            _ui_clear_log();
            break;
    }
}

#define UI_MSG_QUEUE_SIZE 20
static QueueHandle_t ui_msg_queue = NULL;

// === 新增：在 lvgl_port_task 主循环中定期调用 ===
void ui_process_messages(void) {
    if (!ui_msg_queue) return;
    ui_msg_t msg;
    while (xQueueReceive(ui_msg_queue, &msg, 0) == pdTRUE) {
        _ui_apply_msg(&msg);
    }
}

// === 主初始化函数（加入预制数据）===
void ui_init(void) {
    ESP_LOGD(TAG, "ui_init");
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    init_top_bar();
    init_status_area();
    init_button_area();
    init_log_area();
    init_bottom_bar();

    if (ui_msg_queue == NULL) {
        ui_msg_queue = xQueueCreate(UI_MSG_QUEUE_SIZE, sizeof(ui_msg_t));
        configASSERT(ui_msg_queue);
    }
}

// api
void ui_set_top_firmware_info(const char* name, const char* version) {
    if (!ui_msg_queue) return;
    ui_msg_t msg = {0};
    msg.type = UI_MSG_SET_TOP;
    if (name) strncpy(msg.data.top.name, name, sizeof(msg.data.top.name) - 1);
    if (version) strncpy(msg.data.top.version, version, sizeof(msg.data.top.version) - 1);
    xQueueSend(ui_msg_queue, &msg, 0);
}

void ui_set_status_item(int index, const char* key, const char* value, lv_color_t color) {
    if (!ui_msg_queue || index < 0) return;
    ui_msg_t msg = {0};
    msg.type = UI_MSG_SET_STATUS_ITEM;
    msg.data.status_item.index = index;
    if (key) strncpy(msg.data.status_item.key, key, sizeof(msg.data.status_item.key) - 1);
    if (value) strncpy(msg.data.status_item.value, value, sizeof(msg.data.status_item.value) - 1);
    msg.data.status_item.color = color;
    xQueueSend(ui_msg_queue, &msg, 0);
}

void ui_set_button(int index, const char* text, ui_btn_callback_t callback) {
    if (!ui_msg_queue || index < 0) return;
    ui_msg_t msg = {0};
    msg.type = UI_MSG_SET_BUTTON;
    msg.data.button.index = index;
    if (text) strncpy(msg.data.button.text, text, sizeof(msg.data.button.text) - 1);
    msg.data.button.callback = callback;
    xQueueSend(ui_msg_queue, &msg, 0);
}

void ui_add_log(const char* msg) {
    if (!ui_msg_queue || !msg) return;
    ui_msg_t m = {0};
    m.type = UI_MSG_ADD_LOG;
    // 获取当前 LVGL tick（单位：毫秒）
    uint32_t tick_ms = lv_tick_get();
    uint32_t total_sec = tick_ms / 1000;
    uint32_t hr = total_sec / 3600;
    uint32_t min = (total_sec % 3600) / 60;
    uint32_t sec = total_sec % 60;

    // 格式化带时间戳的日志消息，确保不越界
    snprintf(m.data.log.msg, sizeof(m.data.log.msg),
             "[%02d:%02d:%02d.%03d] %s", (int)hr, (int)min, (int)sec,(int)tick_ms%1000, msg);
    xQueueSend(ui_msg_queue, &m, 0);
}

void ui_set_bottom_info(const char* ip, uint32_t baudrate, const char* firmware_id) {
    if (!ui_msg_queue) return;
    ui_msg_t msg = {0};
    msg.type = UI_MSG_SET_BOTTOM;
    if (ip) strncpy(msg.data.bottom.ip, ip, sizeof(msg.data.bottom.ip) - 1);
    msg.data.bottom.baudrate = baudrate;
    if (firmware_id) strncpy(msg.data.bottom.firmware_id, firmware_id, sizeof(msg.data.bottom.firmware_id) - 1);
    xQueueSend(ui_msg_queue, &msg, 0);
}

void ui_refresh_status(void) {
    if (!ui_msg_queue) return;
    ui_msg_t msg = {0};
    msg.type = UI_MSG_REFRESH_STATUS;
    xQueueSend(ui_msg_queue, &msg, 0);
}

void ui_clear_log(void) {
    if (!ui_msg_queue) return;
    ui_msg_t msg = {0};
    msg.type = UI_MSG_CLEAR_LOG;
    xQueueSend(ui_msg_queue, &msg, 0);
}