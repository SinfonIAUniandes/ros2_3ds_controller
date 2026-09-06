#ifndef ROS2_3DS_TOUCH_UI_H
#define ROS2_3DS_TOUCH_UI_H

#include <stdbool.h>
#include <stdint.h>
#include <3ds.h>
#include <citro2d.h>

#include "dds_controller_runtime.h"

typedef enum {
    UI_MODE_MAIN = 0,
    UI_MODE_SETTINGS
} touch_ui_mode;

typedef struct {
    C3D_RenderTarget *top_screen;
    C3D_RenderTarget *bottom_screen;
    C2D_TextBuf text_buf;
    touch_ui_mode mode;
    bool exit_requested;

    /* Theme colors */
    u32 col_bg;
    u32 col_surface;
    u32 col_header;
    u32 col_border;
    u32 col_accent;
    u32 col_btn;
    u32 col_btn_active;
    u32 col_text;
    u32 col_subtext;
    u32 col_danger;
    u32 col_success;

    /* Cached input state for live visualization */
    float cpad_x;
    float cpad_y;
    float cstick_x;
    float cstick_y;
    u32 keys_held;

    /* Temporary feedback toast */
    char status_msg[64];
    uint64_t status_expire_ms;
} touch_ui;

bool touch_ui_init(touch_ui *ui);
void touch_ui_free(touch_ui *ui);

void touch_ui_update_inputs(touch_ui *ui, float cpad_x, float cpad_y,
                            float cstick_x, float cstick_y, u32 keys_held);

void touch_ui_handle_touch(touch_ui *ui, dds_controller_runtime *dds, u16 px, u16 py);

void touch_ui_render_top(touch_ui *ui, dds_controller_runtime *dds);
void touch_ui_render_bottom(touch_ui *ui, dds_controller_runtime *dds);

void touch_ui_set_status(touch_ui *ui, const char *msg);

#endif
