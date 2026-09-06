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

#define SOC_BUFFER_SIZE 0x100000

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    gfxInitDefault();
    romfsInit();

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
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

    /* Main loop: Only terminates on touch screen [ EXIT ] or system home event */
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
            touch_ui_handle_touch(&ui, &dds, touch.px, touch.py);
        }

        /* Physical inputs (Circle Pad, C-Stick, D-Pad, Buttons) are strictly
           and exclusively published over the ROS 2 Joy topic. Never passed to UI. */
        uint64_t now = osGetTime();
        if (now - last_joy_publish_ms >= joy_interval_ms) {
            dds_controller_runtime_publish_joy(&dds, now, &circle, &cstick, kHeld, &touch, is_touching);
            last_joy_publish_ms = now;
        }

        /* Poll camera DDS reader for new video frames */
        dds_controller_runtime_poll_camera(&dds);

        /* Update gamepad visualizer state */
        touch_ui_update_inputs(&ui, dds.joy.last_axes[0], dds.joy.last_axes[1],
                               dds.joy.last_axes[2], dds.joy.last_axes[3], kHeld);

        /* Render frames on top and bottom screens */
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        touch_ui_render_top(&ui, &dds);
        touch_ui_render_bottom(&ui, &dds);
        C3D_FrameEnd(0);
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
