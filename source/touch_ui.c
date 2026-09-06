#include "touch_ui.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logging/app_log.h"

static void draw_rect(float x, float y, float w, float h, u32 color) {
    C2D_DrawRectSolid(x, y, 0.5f, w, h, color);
}

static void draw_panel(touch_ui *ui, float x, float y, float w, float h) {
    draw_rect(x, y, w, h, ui->col_border);
    draw_rect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, ui->col_surface);
}

static void draw_text(touch_ui *ui, float x, float y, float scale, u32 color, const char *str) {
    if (!ui || !ui->text_buf || !str || str[0] == '\0') return;
    C2D_Text parsed;
    C2D_TextParse(&parsed, ui->text_buf, str);
    C2D_TextOptimize(&parsed);
    C2D_DrawText(&parsed, C2D_WithColor, x, y, 0.5f, scale, scale, color);
}

static void draw_textf(touch_ui *ui, float x, float y, float scale, u32 color, const char *fmt, ...) {
    char buf[192];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    draw_text(ui, x, y, scale, color, buf);
}

static void draw_button(touch_ui *ui, float x, float y, float w, float h,
                        u32 bg_color, u32 border_color, const char *label, const char *sublabel) {
    draw_rect(x, y, w, h, border_color);
    draw_rect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, bg_color);

    if (sublabel && sublabel[0] != '\0') {
        draw_text(ui, x + 8.0f, y + 3.0f, 0.40f, ui->col_text, label);
        draw_text(ui, x + 8.0f, y + 18.0f, 0.30f, ui->col_muted, sublabel);
    } else {
        float text_y = y + (h - 13.0f) * 0.5f;
        draw_text(ui, x + 8.0f, text_y, 0.38f, ui->col_text, label);
    }
}

static void draw_badge(touch_ui *ui, float x, float y, float w, float h,
                       const char *text, bool active, u32 active_color) {
    u32 bg = active ? active_color : C2D_Color32(36, 42, 54, 255);
    u32 border = active ? C2D_Color32(255, 255, 255, 255) : C2D_Color32(55, 65, 80, 255);
    u32 fg = active ? C2D_Color32(255, 255, 255, 255) : C2D_Color32(130, 145, 165, 255);

    draw_rect(x, y, w, h, border);
    draw_rect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, bg);

    size_t len = strlen(text);
    float text_scale = (len >= 3) ? 0.28f : 0.34f;
    float text_x = x + 3.0f;
    if (len == 1) text_x = x + (w - 7.0f) * 0.5f;
    else if (len == 2) text_x = x + (w - 14.0f) * 0.5f;
    else if (len >= 3) text_x = x + (w - 24.0f) * 0.5f;

    float text_y = y + (h - 11.0f) * 0.5f;
    draw_text(ui, text_x, text_y, text_scale, fg, text);
}

static void draw_stick_panel(touch_ui *ui, float x, float y, float w, float h,
                             const char *title, float stick_x, float stick_y, u32 dot_color) {
    draw_panel(ui, x, y, w, h);
    draw_text(ui, x + 12.0f, y + 6.0f, 0.36f, ui->col_muted, title);

    float box_size = 52.0f;
    float box_x = x + (w - box_size) * 0.5f;
    float box_y = y + 26.0f;
    float center_x = box_x + box_size * 0.5f;
    float center_y = box_y + box_size * 0.5f;

    draw_rect(box_x, box_y, box_size, box_size, C2D_Color32(24, 28, 38, 255));
    draw_rect(box_x, box_y, box_size, 1.0f, ui->col_border);
    draw_rect(box_x, box_y + box_size - 1.0f, box_size, 1.0f, ui->col_border);
    draw_rect(box_x, box_y, 1.0f, box_size, ui->col_border);
    draw_rect(box_x + box_size - 1.0f, box_y, 1.0f, box_size, ui->col_border);

    draw_rect(box_x + 2.0f, center_y, box_size - 4.0f, 1.0f, C2D_Color32(45, 54, 70, 255));
    draw_rect(center_x, box_y + 2.0f, 1.0f, box_size - 4.0f, C2D_Color32(45, 54, 70, 255));

    float dot_x = center_x - stick_x * 20.0f;
    float dot_y = center_y - stick_y * 20.0f;
    draw_rect(dot_x - 3.0f, dot_y - 3.0f, 6.0f, 6.0f, dot_color);

    draw_textf(ui, x + 12.0f, y + 84.0f, 0.34f, ui->col_text, "X: %+.2f", stick_x);
    draw_textf(ui, x + 12.0f, y + 100.0f, 0.34f, ui->col_text, "Y: %+.2f", stick_y);
}

bool touch_ui_init(touch_ui *ui) {
    if (!ui) return false;
    memset(ui, 0, sizeof(*ui));

    ui->top_screen = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    ui->bottom_screen = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    if (!ui->top_screen || !ui->bottom_screen) {
        app_log_write(APP_LOG_ERROR, "Failed to create Citro2D screen targets");
        return false;
    }

    ui->text_buf = C2D_TextBufNew(16384);
    if (!ui->text_buf) {
        app_log_write(APP_LOG_ERROR, "Failed to allocate text buffer");
        return false;
    }

    ui->mode = UI_MODE_MAIN;
    ui->exit_requested = false;

    /* Theme colors */
    ui->col_bg          = C2D_Color32(20, 24, 32, 255);
    ui->col_surface     = C2D_Color32(32, 38, 50, 255);
    ui->col_header      = C2D_Color32(28, 34, 46, 255);
    ui->col_border      = C2D_Color32(55, 65, 82, 255);
    ui->col_accent      = C2D_Color32(30, 136, 229, 255);
    ui->col_btn         = C2D_Color32(40, 48, 64, 255);
    ui->col_btn_active  = C2D_Color32(30, 136, 229, 255);
    ui->col_text        = C2D_Color32(255, 255, 255, 255);
    ui->col_muted       = C2D_Color32(140, 155, 175, 255);
    ui->col_danger      = C2D_Color32(220, 50, 50, 255);
    ui->col_success     = C2D_Color32(46, 160, 67, 255);

    ui->status_msg[0] = '\0';
    ui->status_expire_ms = 0;

    return true;
}

void touch_ui_free(touch_ui *ui) {
    if (!ui) return;
    if (ui->text_buf) {
        C2D_TextBufDelete(ui->text_buf);
        ui->text_buf = NULL;
    }
}

void touch_ui_set_status(touch_ui *ui, const char *msg) {
    if (!ui || !msg) return;
    snprintf(ui->status_msg, sizeof(ui->status_msg), "%s", msg);
    ui->status_expire_ms = osGetTime() + 3000;
}

void touch_ui_update_inputs(touch_ui *ui, float cpad_x, float cpad_y,
                            float cstick_x, float cstick_y, u32 keys_held) {
    if (!ui) return;
    ui->cpad_x = cpad_x;
    ui->cpad_y = cpad_y;
    ui->cstick_x = cstick_x;
    ui->cstick_y = cstick_y;
    ui->keys_held = keys_held;
}

static inline bool hit_test(u16 px, u16 py, float x, float y, float w, float h) {
    return (float)px >= x && (float)px < (x + w) && (float)py >= y && (float)py < (y + h);
}

ui_action touch_ui_handle_touch(touch_ui *ui, u16 px, u16 py) {
    if (!ui) return UI_ACTION_NONE;

    /* Top Navigation Tabs */
    if (py >= 2 && py <= 26) {
        if (hit_test(px, py, 4.0f, 2.0f, 102.0f, 24.0f)) {
            ui->mode = UI_MODE_MAIN;
            return UI_ACTION_TAB_CONTROLLER;
        }
        if (hit_test(px, py, 110.0f, 2.0f, 102.0f, 24.0f)) {
            ui->mode = UI_MODE_SETTINGS;
            return UI_ACTION_TAB_SETTINGS;
        }
        if (hit_test(px, py, 216.0f, 2.0f, 100.0f, 24.0f)) {
            ui->exit_requested = true;
            return UI_ACTION_EXIT;
        }
    }

    if (ui->mode == UI_MODE_MAIN) {
        /* Row 2 Actions */
        if (py >= 28 && py <= 52) {
            if (hit_test(px, py, 4.0f, 28.0f, 102.0f, 24.0f)) {
                return UI_ACTION_SEND_COMMAND;
            }
            if (hit_test(px, py, 110.0f, 28.0f, 102.0f, 24.0f)) {
                return UI_ACTION_TOGGLE_JOY;
            }
            if (hit_test(px, py, 216.0f, 28.0f, 100.0f, 24.0f)) {
                return UI_ACTION_TOGGLE_CAMERA;
            }
        }
    } else if (ui->mode == UI_MODE_SETTINGS) {
        if (hit_test(px, py, 6.0f, 58.0f, 308.0f, 36.0f)) {
            return UI_ACTION_EDIT_DOMAIN_ID;
        }
        if (hit_test(px, py, 6.0f, 98.0f, 308.0f, 36.0f)) {
            return UI_ACTION_EDIT_NAMESPACE;
        }
        if (hit_test(px, py, 6.0f, 138.0f, 308.0f, 36.0f)) {
            return UI_ACTION_EDIT_CAMERA_TOPIC;
        }
        if (hit_test(px, py, 6.0f, 178.0f, 308.0f, 36.0f)) {
            return UI_ACTION_SAVE_CONFIG;
        }
    }

    return UI_ACTION_NONE;
}

static void touch_ui_render_top_internal(touch_ui *ui, dds_controller_runtime *dds) {
    if (dds->camera.enabled && dds->camera.has_frame) {
        ros2_camera_sub_draw(&dds->camera, 400.0f, 240.0f);

        draw_rect(0, 218.0f, 400.0f, 22.0f, C2D_Color32(10, 14, 20, 220));
        draw_textf(ui, 8.0f, 222.0f, 0.36f, C2D_Color32(230, 240, 255, 255),
                   "Topic: %s | %lux%lu @ %.1f FPS", dds->config.camera_topic,
                   (unsigned long)dds->camera.img_width,
                   (unsigned long)dds->camera.img_height,
                   dds->camera.current_fps);
    } else {
        float box_x = 20.0f;
        float box_y = 20.0f;
        float box_w = 360.0f;
        float box_h = 200.0f;

        draw_panel(ui, box_x, box_y, box_w, box_h);
        draw_rect(box_x + 1.0f, box_y + 1.0f, box_w - 2.0f, 26.0f, ui->col_header);

        draw_text(ui, box_x + 12.0f, box_y + 6.0f, 0.44f, ui->col_text, "ROS 2 ROBOT CAMERA MONITOR");
        draw_text(ui, box_x + 14.0f, box_y + 38.0f, 0.36f, ui->col_muted, "Subscribed Topic:");
        draw_text(ui, box_x + 14.0f, box_y + 54.0f, 0.42f, ui->col_accent, dds->config.camera_topic);

        draw_textf(ui, box_x + 14.0f, box_y + 84.0f, 0.38f, ui->col_text,
                   "Domain ID: %lu   Namespace: %s", (unsigned long)dds->config.domain_id,
                   dds->config.ros_namespace);

        const char *status_str = dds->config.camera_enabled ? "Waiting for incoming CompressedImage..." : "Camera RX Disabled";
        u32 status_col = dds->config.camera_enabled ? ui->col_success : ui->col_danger;
        draw_textf(ui, box_x + 14.0f, box_y + 108.0f, 0.36f, status_col, "Status: %s", status_str);

        draw_text(ui, box_x + 14.0f, box_y + 146.0f, 0.34f, ui->col_muted,
                  "Use bottom screen [SETTINGS] to change topic or domain ID");
    }
}

static void touch_ui_render_bottom_internal(touch_ui *ui, dds_controller_runtime *dds) {
    /* Row 1: Top Navigation Tabs */
    u32 tab1_bg = (ui->mode == UI_MODE_MAIN) ? ui->col_accent : ui->col_surface;
    u32 tab2_bg = (ui->mode == UI_MODE_SETTINGS) ? ui->col_accent : ui->col_surface;
    draw_button(ui, 4.0f, 2.0f, 102.0f, 24.0f, tab1_bg, ui->col_border, "CONTROLS", NULL);
    draw_button(ui, 110.0f, 2.0f, 102.0f, 24.0f, tab2_bg, ui->col_border, "SETTINGS", NULL);
    draw_button(ui, 216.0f, 2.0f, 100.0f, 24.0f, ui->col_danger, ui->col_border, "EXIT APP", NULL);

    if (ui->mode == UI_MODE_MAIN) {
        /* Row 2: Action Buttons */
        draw_button(ui, 4.0f, 28.0f, 102.0f, 24.0f, ui->col_btn, ui->col_border, "SEND CMD", NULL);

        u32 joy_bg = dds->config.joy_enabled ? ui->col_success : ui->col_btn;
        draw_button(ui, 110.0f, 28.0f, 102.0f, 24.0f, joy_bg, ui->col_border,
                    dds->config.joy_enabled ? "JOY: ON" : "JOY: OFF", NULL);

        u32 cam_bg = dds->config.camera_enabled ? ui->col_btn_active : ui->col_btn;
        draw_button(ui, 216.0f, 28.0f, 100.0f, 24.0f, cam_bg, ui->col_border,
                    dds->config.camera_enabled ? "CAM: ON" : "CAM: OFF", NULL);

        /* Middle Area: 3 Columns */
        /* Column 1: Circle Pad */
        draw_stick_panel(ui, 4.0f, 54.0f, 102.0f, 126.0f, "CIRCLE PAD",
                         ui->cpad_x, ui->cpad_y, ui->col_accent);

        /* Column 2: Physical Buttons Panel */
        draw_panel(ui, 108.0f, 54.0f, 104.0f, 160.0f);
        draw_text(ui, 136.0f, 58.0f, 0.36f, ui->col_muted, "BUTTONS");

        u32 k = ui->keys_held;
        /* Shoulders */
        draw_badge(ui, 112.0f, 74.0f, 20.0f, 16.0f, "L", (k & KEY_L) != 0, ui->col_accent);
        draw_badge(ui, 135.0f, 74.0f, 22.0f, 16.0f, "ZL", (k & KEY_ZL) != 0, ui->col_accent);
        draw_badge(ui, 161.0f, 74.0f, 22.0f, 16.0f, "ZR", (k & KEY_ZR) != 0, ui->col_accent);
        draw_badge(ui, 186.0f, 74.0f, 20.0f, 16.0f, "R", (k & KEY_R) != 0, ui->col_accent);

        /* Face Buttons Diamond (Center: 160, 114) */
        draw_badge(ui, 151.0f, 96.0f, 18.0f, 16.0f, "X", (k & KEY_X) != 0, C2D_Color32(46, 120, 242, 255));
        draw_badge(ui, 131.0f, 112.0f, 18.0f, 16.0f, "Y", (k & KEY_Y) != 0, C2D_Color32(46, 160, 67, 255));
        draw_badge(ui, 171.0f, 112.0f, 18.0f, 16.0f, "A", (k & KEY_A) != 0, C2D_Color32(218, 54, 51, 255));
        draw_badge(ui, 151.0f, 128.0f, 18.0f, 16.0f, "B", (k & KEY_B) != 0, C2D_Color32(220, 180, 20, 255));

        /* D-Pad Cross (Center: 160, 160) */
        draw_badge(ui, 152.0f, 146.0f, 16.0f, 14.0f, "^", (k & KEY_DUP) != 0, ui->col_accent);
        draw_badge(ui, 134.0f, 158.0f, 16.0f, 14.0f, "<", (k & KEY_DLEFT) != 0, ui->col_accent);
        draw_badge(ui, 170.0f, 158.0f, 16.0f, 14.0f, ">", (k & KEY_DRIGHT) != 0, ui->col_accent);
        draw_badge(ui, 152.0f, 170.0f, 16.0f, 14.0f, "v", (k & KEY_DDOWN) != 0, ui->col_accent);

        /* Select / Start */
        draw_badge(ui, 116.0f, 190.0f, 40.0f, 18.0f, "SEL", (k & KEY_SELECT) != 0, ui->col_accent);
        draw_badge(ui, 164.0f, 190.0f, 40.0f, 18.0f, "STA", (k & KEY_START) != 0, ui->col_accent);

        /* Column 3: C-Stick */
        draw_stick_panel(ui, 214.0f, 54.0f, 102.0f, 126.0f, "C-STICK",
                         ui->cstick_x, ui->cstick_y, ui->col_btn_active);

    } else if (ui->mode == UI_MODE_SETTINGS) {
        char dom_label[64];
        snprintf(dom_label, sizeof(dom_label), "Domain ID: %lu", (unsigned long)dds->config.domain_id);
        draw_button(ui, 6.0f, 58.0f, 308.0f, 36.0f, ui->col_surface, ui->col_border, dom_label, "Tap to edit (0 - 232)");

        char ns_label[128];
        snprintf(ns_label, sizeof(ns_label), "Namespace: %.50s", dds->config.ros_namespace);
        draw_button(ui, 6.0f, 98.0f, 308.0f, 36.0f, ui->col_surface, ui->col_border, ns_label, "Tap to edit ROS 2 namespace");

        char cam_label[160];
        snprintf(cam_label, sizeof(cam_label), "Cam Topic: %.32s...", dds->config.camera_topic);
        draw_button(ui, 6.0f, 138.0f, 308.0f, 36.0f, ui->col_surface, ui->col_border, cam_label, "Tap to edit camera topic");

        draw_button(ui, 6.0f, 178.0f, 308.0f, 36.0f, ui->col_btn, ui->col_border, "SAVE CONFIG TO SD", "Write to /3ds/ros2_3ds_controller/config.ini");
    }

    /* Bottom Telemetry Bar */
    draw_rect(0, 218.0f, 320.0f, 22.0f, ui->col_header);
    if (ui->status_msg[0] != '\0' && osGetTime() < ui->status_expire_ms) {
        draw_text(ui, 8.0f, 222.0f, 0.36f, ui->col_success, ui->status_msg);
    } else {
        draw_textf(ui, 8.0f, 222.0f, 0.32f, ui->col_muted,
                   "TX: %lu | RX: %lu (%.1f fps) | DOM: %lu",
                   (unsigned long)dds->joy.published_count,
                   (unsigned long)dds->camera.frames_received,
                   dds->camera.current_fps,
                   (unsigned long)dds->config.domain_id);
    }
}

void touch_ui_render(touch_ui *ui, dds_controller_runtime *dds) {
    if (!ui || !dds) return;

    /* 1. Clear text buffer at frame start */
    C2D_TextBufClear(ui->text_buf);

    /* 2. Begin GPU frame */
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    /* 3. Explicitly clear both target renderbuffers */
    C2D_TargetClear(ui->top_screen, ui->col_bg);
    C2D_TargetClear(ui->bottom_screen, ui->col_bg);

    /* 4. Render top screen scene */
    C2D_SceneBegin(ui->top_screen);
    touch_ui_render_top_internal(ui, dds);

    /* 5. Render bottom screen scene */
    C2D_SceneBegin(ui->bottom_screen);
    touch_ui_render_bottom_internal(ui, dds);

    /* 6. End frame & flush */
    C3D_FrameEnd(0);
}
