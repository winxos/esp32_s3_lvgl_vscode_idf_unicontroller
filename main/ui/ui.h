// ui.h
#ifndef UI_H
#define UI_H

#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_LOG_MAX_LINES 40
#define UI_STATUS_MAX_ITEMS 6
#define UI_BUTTON_COUNT 4

typedef enum {
    UI_MSG_SET_TOP,
    UI_MSG_SET_STATUS_ITEM,
    UI_MSG_SET_BUTTON,
    UI_MSG_ADD_LOG,
    UI_MSG_SET_BOTTOM,
    UI_MSG_REFRESH_STATUS,
    UI_MSG_CLEAR_LOG,
} ui_msg_type_t;

typedef struct {
    ui_msg_type_t type;
    union {
        struct {
            char name[32];
            char version[32];
        } top;
        struct {
            int index;
            char key[32];
            char value[32];
            lv_color_t color;
        } status_item;
        struct {
            int index;
            char text[32];
            void (*callback)(void);
        } button;
        struct {
            char msg[128];
        } log;
        struct {
            char ip[32];
            uint32_t baudrate;
            char firmware_id[32];
        } bottom;
    } data;
} ui_msg_t;

typedef void (*ui_btn_callback_t)(void);

void ui_init(void);
void ui_set_top_firmware_info(const char* name, const char* version);
void ui_set_status_item(int index, const char* key, const char* value, lv_color_t color);
void ui_set_button(int index, const char* text, ui_btn_callback_t callback);
void ui_add_log(const char* msg);
void ui_clear_log(void);
void ui_set_bottom_info(const char* ip, uint32_t baudrate, const char* firmware_id);
void ui_refresh_status(void);
void ui_process_messages(void);

#ifdef __cplusplus
}
#endif

#endif // UI_H