#include <3ds.h>
#include <citro2d.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "controller_config.h"
#include "dds_controller_runtime.h"
#include "logging/app_log.h"
#include "touch_ui.h"

#define SOC_BUFFER_SIZE 0x400000

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    gfxInitDefault();
    romfsInit();

    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
        romfsExit();
        gfxExit();
        return 1;
    }
    if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) {
        C3D_Fini();
        romfsExit();
        gfxExit();
        return 1;
    }
    C2D_Prepare();

    app_log_init();
    app_log_write(APP_LOG_INFO, "=== ROS 2 3DS Controller Starting ===");

    /* Initialize Circle Pad Pro / New 3DS C-Stick */
    bool is_n3ds = false;
    if (R_SUCCEEDED(APT_CheckNew3DS(&is_n3ds)) && is_n3ds) {
        if (R_SUCCEEDED(irrstInit())) {
            app_log_write(APP_LOG_INFO, "New 3DS C-Stick initialized");
        }
    } else {
        if (R_SUCCEEDED(irrstInit())) {
            app_log_write(APP_LOG_INFO, "Circle Pad Pro (IRRST) initialized");
        }
    }

    /* Initialize BSD Sockets for Cyclone DDS */
    void *soc_buffer = memalign(0x1000, SOC_BUFFER_SIZE);
    if (soc_buffer) {
        int res = socInit((u32 *)soc_buffer, SOC_BUFFER_SIZE);
        app_log_write(res == 0 ? APP_LOG_INFO : APP_LOG_ERROR, "socInit result=0x%08X", (unsigned int)res);
    } else {
        app_log_write(APP_LOG_ERROR, "Failed to allocate memory for socInit");
    }

    /* Load configuration from RomFS and SD card override */
    controller_config config;
    controller_config_load(&config);

    /* Initialize touch UI subsystem */
    touch_ui ui;
    if (!touch_ui_init(&ui)) {
        app_log_write(APP_LOG_ERROR, "Failed to initialize touch UI");
        return 1;
    }

    /* Initialize DDS Controller runtime */
    dds_controller_runtime dds;
    dds_controller_runtime_init(&dds);
    if (!dds_controller_runtime_start(&dds, &config)) {
        app_log_write(APP_LOG_ERROR, "Failed to start DDS controller runtime");
    }

    uint64_t last_joy_publish_ms = 0;
    const uint64_t joy_interval_ms = (config.joy_publish_hz > 0) ? (1000 / config.joy_publish_hz) : 33;

    /* Main loop: Only terminates on touch screen [ EXIT APP ] or system home event */
    while (aptMainLoop() && !ui.exit_requested) {
        hidScanInput();

        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();

        circlePosition circle = { 0, 0 };
        hidCircleRead(&circle);

        circlePosition cstick = { 0, 0 };
        irrstCstickRead(&cstick);

        touchPosition touch = { 0, 0 };
        hidTouchRead(&touch);
        bool is_touching = (kHeld & KEY_TOUCH) != 0;

        /* Touch input is strictly and exclusively routed to UI management */
        if (kDown & KEY_TOUCH) {
            ui_action action = touch_ui_handle_touch(&ui, touch.px, touch.py);
            switch (action) {
                case UI_ACTION_SEND_COMMAND: {
                    SwkbdState swkbd;
                    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, 128);
                    swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Cancel", false);
                    swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "Send", true);
                    swkbdSetHintText(&swkbd, "Enter command / text message");
                    char text[128] = "";
                    if (swkbdInputText(&swkbd, text, sizeof(text)) == SWKBD_BUTTON_RIGHT && text[0] != '\0') {
                        dds_controller_runtime_publish_string(&dds, text);
                        touch_ui_set_status(&ui, "Command String Published!");
                    }
                    break;
                }
                case UI_ACTION_TOGGLE_JOY:
                    dds.config.joy_enabled = !dds.config.joy_enabled;
                    dds.joy.enabled = dds.config.joy_enabled;
                    controller_config_save(&dds.config);
                    touch_ui_set_status(&ui, dds.config.joy_enabled ? "Joy Streaming: ON" : "Joy Streaming: OFF");
                    break;
                case UI_ACTION_TOGGLE_CAMERA:
                    dds.config.camera_enabled = !dds.config.camera_enabled;
                    dds.camera.enabled = dds.config.camera_enabled;
                    controller_config_save(&dds.config);
                    touch_ui_set_status(&ui, dds.config.camera_enabled ? "Camera RX: ON" : "Camera RX: OFF");
                    break;
                case UI_ACTION_EDIT_DOMAIN_ID: {
                    char val[16];
                    snprintf(val, sizeof(val), "%lu", (unsigned long)dds.config.domain_id);
                    SwkbdState swkbd;
                    swkbdInit(&swkbd, SWKBD_TYPE_NUMPAD, 2, 3);
                    swkbdSetInitialText(&swkbd, val);
                    swkbdSetHintText(&swkbd, "Domain ID (0-232)");
                    swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Cancel", false);
                    swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "Apply", true);
                    if (swkbdInputText(&swkbd, val, sizeof(val)) == SWKBD_BUTTON_RIGHT && val[0] != '\0') {
                        long id = strtol(val, NULL, 10);
                        if (id >= 0 && id <= 232) {
                            dds_controller_runtime_set_domain_id(&dds, (uint32_t)id);
                            touch_ui_set_status(&ui, "Domain ID Updated");
                        }
                    }
                    break;
                }
                case UI_ACTION_EDIT_NAMESPACE: {
                    char val[64];
                    snprintf(val, sizeof(val), "%s", dds.config.ros_namespace);
                    SwkbdState swkbd;
                    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, sizeof(val) - 1);
                    swkbdSetInitialText(&swkbd, val);
                    swkbdSetHintText(&swkbd, "Namespace (e.g. /my_robot)");
                    swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Cancel", false);
                    swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "Apply", true);
                    if (swkbdInputText(&swkbd, val, sizeof(val)) == SWKBD_BUTTON_RIGHT && val[0] != '\0') {
                        dds_controller_runtime_set_namespace(&dds, val);
                        touch_ui_set_status(&ui, "Namespace Updated");
                    }
                    break;
                }
                case UI_ACTION_EDIT_JOY_TOPIC: {
                    char val[128];
                    snprintf(val, sizeof(val), "%s", dds.config.joy_topic);
                    SwkbdState swkbd;
                    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, sizeof(val) - 1);
                    swkbdSetInitialText(&swkbd, val);
                    swkbdSetHintText(&swkbd, "Joy Topic (e.g. /joy)");
                    swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Cancel", false);
                    swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "Apply", true);
                    if (swkbdInputText(&swkbd, val, sizeof(val)) == SWKBD_BUTTON_RIGHT && val[0] != '\0') {
                        dds_controller_runtime_set_joy_topic(&dds, val);
                        touch_ui_set_status(&ui, "Joy Topic Updated");
                    }
                    break;
                }
                case UI_ACTION_TOGGLE_JOY_QOS:
                    dds_controller_runtime_set_joy_reliable(&dds, !dds.config.joy_reliable);
                    touch_ui_set_status(&ui, dds.config.joy_reliable ? "Joy QoS: Reliable" : "Joy QoS: BestEffort");
                    break;
                case UI_ACTION_EDIT_CAMERA_TOPIC: {
                    char val[128];
                    snprintf(val, sizeof(val), "%s", dds.config.camera_topic);
                    SwkbdState swkbd;
                    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, sizeof(val) - 1);
                    swkbdSetInitialText(&swkbd, val);
                    swkbdSetHintText(&swkbd, "Camera Topic");
                    swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Cancel", false);
                    swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "Apply", true);
                    if (swkbdInputText(&swkbd, val, sizeof(val)) == SWKBD_BUTTON_RIGHT && val[0] != '\0') {
                        dds_controller_runtime_set_camera_topic(&dds, val);
                        touch_ui_set_status(&ui, "Camera Topic Re-subscribed");
                    }
                    break;
                }
                case UI_ACTION_SAVE_CONFIG:
                    if (controller_config_save(&dds.config)) {
                        touch_ui_set_status(&ui, "Config Saved to SD Card");
                    } else {
                        touch_ui_set_status(&ui, "Error Saving to SD Card");
                    }
                    break;
                case UI_ACTION_EXIT:
                    ui.exit_requested = true;
                    break;
                default:
                    break;
            }
        }

        /* Physical inputs (Circle Pad, C-Stick, D-Pad, Buttons) are strictly
           and exclusively published over the ROS 2 Joy topic. Never passed to UI. */
        uint64_t now = osGetTime();
        if (now - last_joy_publish_ms >= joy_interval_ms) {
            dds_controller_runtime_publish_joy(&dds, now, &circle, &cstick, kHeld, &touch, is_touching);
            last_joy_publish_ms = now;
        }

        /* Periodically refresh ROS 2 discovery graph (ros_discovery_info) every 5 seconds */
        static uint64_t next_graph_refresh_ms = 0;
        if (dds.running && now >= next_graph_refresh_ms) {
            dds_controller_runtime_refresh_graph(&dds);
            next_graph_refresh_ms = now + 5000;
        }

        /* Poll camera DDS reader for new video frames */
        dds_controller_runtime_poll_camera(&dds);

        /* Update gamepad visualizer state */
        touch_ui_update_inputs(&ui, dds.joy.last_axes[0], dds.joy.last_axes[1],
                               dds.joy.last_axes[2], dds.joy.last_axes[3], kHeld);

        /* Render frames on top and bottom screens cleanly */
        touch_ui_render(&ui, &dds);
    }

    app_log_write(APP_LOG_INFO, "Shutting down application cleanly...");

    dds_controller_runtime_stop(&dds);
    touch_ui_free(&ui);

    irrstExit();
    socExit();
    if (soc_buffer) {
        free(soc_buffer);
    }

    app_log_close();

    C2D_Fini();
    C3D_Fini();
    romfsExit();
    gfxExit();

    return 0;
}
