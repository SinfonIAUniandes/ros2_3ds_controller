#include "touch_ui.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logging/app_log.h"

static void draw_text(touch_ui *ui, float x, float y, float scale, u32 color, const char *str) {
    if (!ui || !ui->text_buf || !str) return;
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

static void draw_rect(float x, float y, float w, float h, u32 color) {
    C2D_DrawRectSolid(x, y, 0.5f, w, h, color);
}

static void draw_button(touch_ui *ui, float x, float y, float w, float h,
                        u32 bg_color, u32 border_color, const char *label, const char *sublabel) {
    draw_rect(x, y, w, h, border_color);
    draw_rect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, bg_color);

    if (sublabel && sublabel[0] != '\0') {
        draw_text(ui, x + 8.0f, y + 4.0f, 0.42f, ui->col_text, label);
        draw_text(ui, x + 8.0f, y + 18.0f, 0.32f, ui->col_subtext, sublabel);
    } else {
        draw_text(ui, x + 8.0f, y + (h - 14.0f) * 0.5f, 0.40f, ui->col_text, label);
    }
}

static void draw_badge(touch_ui *ui, float x, float y, float w, float h, const char *text, bool active, u32 active_color) {
    u32 bg = active ? active_color : C2D_Color32(35, 40, 50, 255);
    u32 fg = active ? C2D_Color32(255, 255, 255, 255) : C2D_Color32(110, 120, 135, 255);
    draw_rect(x, y, w, h, C2D_Color32(50, 58, 70, 255));
    draw_rect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, bg);
    draw_text(ui, x + (w - 12.0f) * 0.5f, y + (h - 12.0f) * 0.5f, 0.35f, fg, text);
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

    ui->text_buf = C2D_TextBufNew(4096);
    if (!ui->text_buf) {
        app_log_write(APP_LOG_ERROR, "Failed to allocate text buffer");
        return false;
    }

    ui->mode = UI_MODE_MAIN;
    ui->exit_requested = false;

    /* Theme colors */
    ui->col_bg          = C2D_Color32(13, 17, 23, 255);
    ui->col_surface     = C2D_Color32(22, 27, 34, 255);
    ui->col_header      = C2D_Color32(33, 38, 45, 255);
    ui->col_border      = C2D_Color32(48, 54, 61, 255);
    ui->col_accent      = C2D_Color32(31, 111, 235, 255);
    ui->col_btn         = C2D_Color32(33, 40, 52, 255);
    ui->col_btn_active  = C2D_Color32(46, 120, 242, 255);
    ui->col_text        = C2D_Color32(240, 246, 252, 255);
    ui->col_subtext     = C2D_Color32(139, 148, 158, 255);
    ui->col_danger      = C2D_Color32(218, 54, 51, 255);
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

void touch_ui_handle_touch(touch_ui *ui, dds_controller_runtime *dds, u16 px, u16 py) {
    if (!ui || !dds) return;

    if (ui->mode == UI_MODE_MAIN) {
        /* Row 1 Action Buttons */
        if (py >= 26 && py <= 54) {
            /* [ ⌨ Cmd ] Button */
            if (px >= 6 && px <= 102) {
                SwkbdState swkbd;
                swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, -1);
                swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Cancel", false);
                swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "Send", true);
                swkbdSetHintText(&swkbd, "Enter command / text message");
                char text[128] = "";
                SwkbdButton btn = swkbdInputText(&swkbd, text, sizeof(text));
                if (btn == SWKBD_BUTTON_CONFIRM && text[0] != '\0') {
                    dds_controller_runtime_publish_string(dds, text);
                    touch_ui_set_status(ui, "Command String Sent!");
                }
                return;
            }
            /* [ ⚙ Config ] Button */
            else if (px >= 108 && px <= 204) {
                ui->mode = UI_MODE_SETTINGS;
                return;
            }
            /* [ EXIT ] Button */
            else if (px >= 210 && px <= 314) {
                ui->exit_requested = true;
                return;
            }
        }
        /* Row 2 Toggles */
        else if (py >= 58 && py <= 84) {
            /* [ Joy: ON/OFF ] */
            if (px >= 6 && px <= 156) {
                dds->config.joy_enabled = !dds->config.joy_enabled;
                dds->joy.enabled = dds->config.joy_enabled;
                controller_config_save(&dds->config);
                touch_ui_set_status(ui, dds->config.joy_enabled ? "Joy Streaming: ON" : "Joy Streaming: OFF");
                return;
            }
            /* [ Cam: ON/OFF ] */
            else if (px >= 164 && px <= 314) {
                dds->config.camera_enabled = !dds->config.camera_enabled;
                dds->camera.enabled = dds->config.camera_enabled;
                controller_config_save(&dds->config);
                touch_ui_set_status(ui, dds->config.camera_enabled ? "Camera RX: ON" : "Camera RX: OFF");
                return;
            }
        }
    } else if (ui->mode == UI_MODE_SETTINGS) {
        /* Option 1: Domain ID */
        if (py >= 30 && py <= 62 && px >= 8 && px <= 312) {
            SwkbdState swkbd;
            swkbdInit(&swkbd, SWKBD_TYPE_NUMPAD, 2, 4);
            swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Cancel", false);
            swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "Apply", true);
            char init_str[16];
            snprintf(init_str, sizeof(init_str), "%lu", (unsigned long)dds->config.domain_id);
            swkbdSetInitialText(&swkbd, init_str);
            swkbdSetHintText(&swkbd, "Enter ROS Domain ID (0 - 232)");
            char text[16] = "";
            SwkbdButton btn = swkbdInputText(&swkbd, text, sizeof(text));
            if (btn == SWKBD_BUTTON_CONFIRM && text[0] != '\0') {
                long d = strtol(text, NULL, 10);
                if (d >= 0 && d <= 232) {
                    dds_controller_runtime_set_domain_id(dds, (uint32_t)d);
                    touch_ui_set_status(ui, "Domain ID Updated & Restarted");
                }
            }
            return;
        }
        /* Option 2: Namespace */
        else if (py >= 66 && py <= 98 && px >= 8 && px <= 312) {
            SwkbdState swkbd;
            swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, 60);
            swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Cancel", false);
            swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "Apply", true);
            swkbdSetInitialText(&swkbd, dds->config.ros_namespace);
            swkbdSetHintText(&swkbd, "Enter ROS namespace (e.g. /my_robot)");
            char text[64] = "";
            SwkbdButton btn = swkbdInputText(&swkbd, text, sizeof(text));
            if (btn == SWKBD_BUTTON_CONFIRM && text[0] != '\0') {
                dds_controller_runtime_set_namespace(dds, text);
                touch_ui_set_status(ui, "Namespace Updated");
            }
            return;
        }
        /* Option 3: Camera Topic */
        else if (py >= 102 && py <= 134 && px >= 8 && px <= 312) {
            SwkbdState swkbd;
            swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, 120);
            swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Cancel", false);
            swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "Apply", true);
            swkbdSetInitialText(&swkbd, dds->config.camera_topic);
            swkbdSetHintText(&swkbd, "Enter Camera Topic");
            char text[128] = "";
            SwkbdButton btn = swkbdInputText(&swkbd, text, sizeof(text));
            if (btn == SWKBD_BUTTON_CONFIRM && text[0] != '\0') {
                dds_controller_runtime_set_camera_topic(dds, text);
                touch_ui_set_status(ui, "Camera Topic Re-subscribed");
            }
            return;
        }
        /* Option 4: Save Config to SD */
        else if (py >= 138 && py <= 170 && px >= 8 && px <= 312) {
            if (controller_config_save(&dds->config)) {
                touch_ui_set_status(ui, "Config Saved to SD Card");
            } else {
                touch_ui_set_status(ui, "Error Saving to SD Card");
            }
            return;
        }
        /* Option 5: Back to Controller */
        else if (py >= 182 && py <= 226 && px >= 16 && px <= 304) {
            ui->mode = UI_MODE_MAIN;
            return;
        }
    }
}

void touch_ui_render_top(touch_ui *ui, dds_controller_runtime *dds) {
    if (!ui || !dds) return;

    C2D_SceneBegin(ui->top_screen);

    /* Render video or Standby box */
    ros2_camera_sub_draw(&dds->camera, 400.0f, 240.0f);

    if (dds->camera.enabled && dds->camera.has_frame) {
        /* Live Video OSD */
        draw_textf(ui, 12.0f, 10.0f, 0.38f, C2D_Color32(230, 240, 255, 255),
                   "%lux%lu  %.1f FPS", (unsigned long)dds->camera.img_width,
                   (unsigned long)dds->camera.img_height, dds->camera.current_fps);
        draw_text(ui, 12.0f, 222.0f, 0.34f, C2D_Color32(200, 210, 225, 255),
                  dds->config.camera_topic);
    } else {
        /* Standby Box Content */
        float box_x = (400.0f - 340.0f) * 0.5f;
        float box_y = (240.0f - 140.0f) * 0.5f;

        draw_text(ui, box_x + 12.0f, box_y + 6.0f, 0.44f, ui->col_text, "ROS 2 CAMERA RECEIVER");
        draw_text(ui, box_x + 12.0f, box_y + 36.0f, 0.38f, ui->col_subtext, "Subscribed Topic:");
        draw_text(ui, box_x + 12.0f, box_y + 52.0f, 0.40f, ui->col_accent, dds->config.camera_topic);

        draw_textf(ui, box_x + 12.0f, box_y + 78.0f, 0.36f, ui->col_text,
                   "Domain ID: %lu  |  Status: %s", (unsigned long)dds->config.domain_id,
                   dds->config.camera_enabled ? "Waiting for video..." : "Camera RX Disabled");

        draw_text(ui, box_x + 12.0f, box_y + 104.0f, 0.34f, ui->col_subtext,
                  "Configurable via bottom touch screen [ ⚙ Config ]");
    }
}

void touch_ui_render_bottom(touch_ui *ui, dds_controller_runtime *dds) {
    if (!ui || !dds) return;

    C2D_SceneBegin(ui->bottom_screen);
    C2D_TextBufClear(ui->text_buf);

    /* Fill background */
    draw_rect(0, 0, 320.0f, 240.0f, ui->col_bg);

    if (ui->mode == UI_MODE_MAIN) {
        /* Top Header Bar */
        draw_rect(0, 0, 320.0f, 22.0f, ui->col_header);
        draw_text(ui, 6.0f, 4.0f, 0.42f, ui->col_text, "ROS 2 CONTROLLER");
        draw_textf(ui, 190.0f, 5.0f, 0.34f, ui->col_subtext, "DOM: %lu | NS: %s",
                   (unsigned long)dds->config.domain_id, dds->config.ros_namespace);

        /* Action Buttons Row 1 */
        draw_button(ui, 6.0f, 26.0f, 96.0f, 28.0f, ui->col_btn, ui->col_border, "⌨ Send Cmd", NULL);
        draw_button(ui, 108.0f, 26.0f, 96.0f, 28.0f, ui->col_btn, ui->col_border, "⚙ Config", NULL);
        draw_button(ui, 210.0f, 26.0f, 104.0f, 28.0f, ui->col_danger, ui->col_border, "[ EXIT ]", NULL);

        /* Action Buttons Row 2 */
        u32 joy_col = dds->config.joy_enabled ? ui->col_success : ui->col_btn;
        u32 cam_col = dds->config.camera_enabled ? ui->col_btn_active : ui->col_btn;
        draw_button(ui, 6.0f, 58.0f, 150.0f, 26.0f, joy_col, ui->col_border,
                    dds->config.joy_enabled ? "Joy: STREAMING" : "Joy: DISABLED", NULL);
        draw_button(ui, 164.0f, 58.0f, 150.0f, 26.0f, cam_col, ui->col_border,
                    dds->config.camera_enabled ? "Camera RX: ACTIVE" : "Camera RX: OFF", NULL);

        /* Live Gamepad Visualizer Panel */
        float panel_y = 88.0f;
        float panel_h = 128.0f;
        draw_rect(6.0f, panel_y, 308.0f, panel_h, ui->col_surface);
        draw_rect(6.0f, panel_y, 308.0f, 1.0f, ui->col_border);

        /* Left: Circle Pad Visualizer */
        float cpad_cx = 54.0f;
        float cpad_cy = 152.0f;
        draw_rect(cpad_cx - 28.0f, cpad_cy - 28.0f, 56.0f, 56.0f, C2D_Color32(30, 35, 45, 255));
        draw_rect(cpad_cx - 1.0f, cpad_cy - 28.0f, 2.0f, 56.0f, C2D_Color32(45, 52, 65, 255));
        draw_rect(cpad_cx - 28.0f, cpad_cy - 1.0f, 56.0f, 2.0f, C2D_Color32(45, 52, 65, 255));
        float cpad_px = cpad_cx - ui->cpad_x * 24.0f; /* X: left is positive in Joy */
        float cpad_py = cpad_cy - ui->cpad_y * 24.0f; /* Y: up is positive in Joy */
        draw_rect(cpad_px - 4.0f, cpad_py - 4.0f, 8.0f, 8.0f, ui->col_accent);
        draw_text(ui, 24.0f, 100.0f, 0.34f, ui->col_subtext, "CIRCLE PAD");
        draw_textf(ui, 18.0f, 190.0f, 0.30f, ui->col_text, "X:%+.2f Y:%+.2f", ui->cpad_x, ui->cpad_y);

        /* Center: Button Badges */
        u32 k = ui->keys_held;
        /* Shoulders */
        draw_badge(ui, 114.0f, 96.0f, 20.0f, 16.0f, "L", (k & KEY_L) != 0, ui->col_btn_active);
        draw_badge(ui, 138.0f, 96.0f, 24.0f, 16.0f, "ZL", (k & KEY_ZL) != 0, ui->col_btn_active);
        draw_badge(ui, 168.0f, 96.0f, 24.0f, 16.0f, "ZR", (k & KEY_ZR) != 0, ui->col_btn_active);
        draw_badge(ui, 196.0f, 96.0f, 20.0f, 16.0f, "R", (k & KEY_R) != 0, ui->col_btn_active);

        /* Face Buttons (X top, Y left, A right, B bottom) */
        draw_badge(ui, 172.0f, 120.0f, 18.0f, 18.0f, "X", (k & KEY_X) != 0, C2D_Color32(46, 120, 242, 255));
        draw_badge(ui, 150.0f, 138.0f, 18.0f, 18.0f, "Y", (k & KEY_Y) != 0, C2D_Color32(46, 160, 67, 255));
        draw_badge(ui, 194.0f, 138.0f, 18.0f, 18.0f, "A", (k & KEY_A) != 0, C2D_Color32(218, 54, 51, 255));
        draw_badge(ui, 172.0f, 156.0f, 18.0f, 18.0f, "B", (k & KEY_B) != 0, C2D_Color32(220, 180, 20, 255));

        /* D-Pad */
        draw_badge(ui, 122.0f, 120.0f, 16.0f, 16.0f, "^", (k & KEY_DUP) != 0, ui->col_btn_active);
        draw_badge(ui, 104.0f, 138.0f, 16.0f, 16.0f, "<", (k & KEY_DLEFT) != 0, ui->col_btn_active);
        draw_badge(ui, 140.0f, 138.0f, 16.0f, 16.0f, ">", (k & KEY_DRIGHT) != 0, ui->col_btn_active);
        draw_badge(ui, 122.0f, 156.0f, 16.0f, 16.0f, "v", (k & KEY_DDOWN) != 0, ui->col_btn_active);

        /* Select / Start */
        draw_badge(ui, 116.0f, 186.0f, 36.0f, 16.0f, "SELECT", (k & KEY_SELECT) != 0, ui->col_btn_active);
        draw_badge(ui, 166.0f, 186.0f, 36.0f, 16.0f, "START", (k & KEY_START) != 0, ui->col_btn_active);

        /* Right: C-Stick Visualizer */
        float cstick_cx = 266.0f;
        float cstick_cy = 152.0f;
        draw_rect(cstick_cx - 28.0f, cstick_cy - 28.0f, 56.0f, 56.0f, C2D_Color32(30, 35, 45, 255));
        draw_rect(cstick_cx - 1.0f, cstick_cy - 28.0f, 2.0f, 56.0f, C2D_Color32(45, 52, 65, 255));
        draw_rect(cstick_cx - 28.0f, cstick_cy - 1.0f, 56.0f, 2.0f, C2D_Color32(45, 52, 65, 255));
        float cstick_px = cstick_cx - ui->cstick_x * 24.0f;
        float cstick_py = cstick_cy - ui->cstick_y * 24.0f;
        draw_rect(cstick_px - 4.0f, cstick_py - 4.0f, 8.0f, 8.0f, ui->col_btn_active);
        draw_text(ui, 244.0f, 100.0f, 0.34f, ui->col_subtext, "C-STICK");
        draw_textf(ui, 230.0f, 190.0f, 0.30f, ui->col_text, "X:%+.2f Y:%+.2f", ui->cstick_x, ui->cstick_y);

        /* Bottom Telemetry Bar or Status Msg */
        draw_rect(0, 220.0f, 320.0f, 20.0f, ui->col_header);
        if (ui->status_msg[0] != '\0' && osGetTime() < ui->status_expire_ms) {
            draw_text(ui, 8.0f, 224.0f, 0.35f, ui->col_success, ui->status_msg);
        } else {
            draw_textf(ui, 8.0f, 224.0f, 0.33f, ui->col_subtext,
                       "Joy TX: %lu msg  |  Cam RX: %lu frames (%.1ffps)",
                       (unsigned long)dds->joy.published_count,
                       (unsigned long)dds->camera.frames_received,
                       dds->camera.current_fps);
        }
    } else if (ui->mode == UI_MODE_SETTINGS) {
        /* Header */
        draw_rect(0, 0, 320.0f, 24.0f, ui->col_header);
        draw_text(ui, 8.0f, 4.0f, 0.44f, ui->col_text, "⚙ CONTROLLER SETTINGS");

        /* Options */
        char dom_label[64];
        snprintf(dom_label, sizeof(dom_label), "Domain ID: %lu", (unsigned long)dds->config.domain_id);
        draw_button(ui, 8.0f, 28.0f, 304.0f, 32.0f, ui->col_surface, ui->col_border, dom_label, "Tap to change (0 - 232)");

        char ns_label[128];
        snprintf(ns_label, sizeof(ns_label), "Namespace: %.50s", dds->config.ros_namespace);
        draw_button(ui, 8.0f, 64.0f, 304.0f, 32.0f, ui->col_surface, ui->col_border, ns_label, "Tap to change ROS namespace");

        char cam_label[160];
        snprintf(cam_label, sizeof(cam_label), "Cam Topic: %.30s...", dds->config.camera_topic);
        draw_button(ui, 8.0f, 100.0f, 304.0f, 32.0f, ui->col_surface, ui->col_border, cam_label, "Tap to change camera topic");

        draw_button(ui, 8.0f, 136.0f, 304.0f, 32.0f, ui->col_btn, ui->col_border, "💾 Save Configuration to SD", "Persists changes across launches");

        /* Return button */
        draw_button(ui, 16.0f, 180.0f, 288.0f, 38.0f, ui->col_accent, ui->col_border, "◀  BACK TO CONTROLLER", NULL);

        /* Status msg */
        if (ui->status_msg[0] != '\0' && osGetTime() < ui->status_expire_ms) {
            draw_text(ui, 8.0f, 222.0f, 0.35f, ui->col_success, ui->status_msg);
        }
    }
}
