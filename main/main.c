/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "waveshare_rgb_lcd_port.h"
#include "ui.h"

void key1_pressed(void)
{
    ui_add_log("pressed");
}
void key4_pressed(void)
{
    ui_clear_log();
}
void app_main()
{
    waveshare_esp32_s3_rgb_lcd_init(); // Initialize the Waveshare ESP32-S3 RGB LCD 
    // wavesahre_rgb_lcd_bl_on();  //Turn on the screen backlight 
    // wavesahre_rgb_lcd_bl_off(); //Turn off the screen backlight 

    vTaskDelay(pdMS_TO_TICKS(1000));

    ui_set_top_firmware_info("UniController", "v1.0.0");
    ui_set_bottom_info("192.168.1.100", 115200, "FW-2025");

    // 主状态区三列示例
    ui_set_status_item(0, "Temp", "25°C", lv_color_hex(0x00FF00));
    ui_set_status_item(1, "Pressure", "101kPa", lv_color_hex(0xFFFF00));
    ui_set_status_item(2, "Mode", "Auto", lv_color_hex(0x00FFFF));
    ui_set_status_item(3, "Flow", "5L/min", lv_color_hex(0xFF00FF));
    ui_set_status_item(4, "Error", "None", lv_color_hex(0xFFFFFF));
    ui_set_status_item(5, "Uptime", "00:05:30", lv_color_hex(0x00FF00));

    // 按钮
    ui_set_button(0, "Start", key1_pressed);
    ui_set_button(1, "Stop", key1_pressed);
    ui_set_button(2, "Debug", key1_pressed);
    ui_set_button(3, "Clear", key4_pressed);

    // 日志
    ui_add_log("System booting...");
    ui_add_log("LVGL initialized.");
    ui_add_log("Network connected.");
    ui_add_log("Device ready.");
    while(1)
    {
        ui_add_log("tick.");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
