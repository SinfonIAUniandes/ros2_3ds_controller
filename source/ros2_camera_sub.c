#include "ros2_camera_sub.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logging/app_log.h"
#include "ros2_common.h"
#include "ros2_names.h"
#include "sensor_msgs_compressed_image.h"

#define MAX_SUPPORTED_WIDTH  640
#define MAX_SUPPORTED_HEIGHT 480
#define RGBA_PIXEL_SIZE 4

static uint8_t s_morton_table[8][8];
static bool s_morton_initialized = false;

static void init_morton_table(void) {
    if (s_morton_initialized) return;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            s_morton_table[y][x] = (uint8_t)(
                (x & 1) |
                ((y & 1) << 1) |
                ((x & 2) << 1) |
                ((y & 2) << 2) |
                ((x & 4) << 2) |
                ((y & 4) << 3)
            );
        }
    }
    s_morton_initialized = true;
}

static inline uint32_t next_power_of_2(uint32_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v < 64 ? 64 : v;
}

void ros2_camera_sub_init(ros2_camera_sub *sub) {
    if (!sub) return;
    init_morton_table();
    memset(sub, 0, sizeof(*sub));
    sub->topic = DDS_ENTITY_NIL;
    sub->reader = DDS_ENTITY_NIL;
    sub->last_result = DDS_RETCODE_OK;
    sub->enabled = true;
    sub->tj = tjInitDecompress();
    if (!sub->tj) {
        app_log_write(APP_LOG_ERROR, "Failed to initialize TurboJPEG decompressor");
    }

    sub->rgba_buffer_size = MAX_SUPPORTED_WIDTH * MAX_SUPPORTED_HEIGHT * RGBA_PIXEL_SIZE;
    sub->rgba_buffer = (uint8_t *)malloc(sub->rgba_buffer_size);
    if (!sub->rgba_buffer) {
        app_log_write(APP_LOG_ERROR, "Failed to allocate RGBA buffer for camera");
    }
}

static bool create_camera_reader(ros2_camera_sub *sub, dds_entity_t participant) {
    if (sub->ros_topic_name[0] == '\0') {
        snprintf(sub->ros_topic_name, sizeof(sub->ros_topic_name), "/camera/image_raw/compressed");
    }

    const char *raw_name = sub->ros_topic_name;
    while (*raw_name == '/') raw_name++;
    snprintf(sub->dds_topic_name, sizeof(sub->dds_topic_name), "rt/%s", raw_name);

    dds_qos_t *qos = NULL;
    if (!ros2_create_qos(&qos, 1, false, DDS_MSECS(200), false, NULL, false)) {
        sub->last_result = DDS_RETCODE_OUT_OF_RESOURCES;
        return false;
    }

    sub->topic = dds_create_topic(participant, &sensor_msgs_msg_dds__CompressedImage__desc,
                                  sub->dds_topic_name, NULL, NULL);
    if (sub->topic < 0) {
        sub->last_result = sub->topic;
        dds_delete_qos(qos);
        return false;
    }

    sub->reader = dds_create_reader(participant, sub->topic, qos, NULL);
    if (sub->reader < 0) {
        sub->last_result = sub->reader;
        dds_delete_qos(qos);
        if (sub->topic > DDS_ENTITY_NIL) {
            dds_delete(sub->topic);
            sub->topic = DDS_ENTITY_NIL;
        }
        return false;
    }

    dds_delete_qos(qos);
    sub->last_result = DDS_RETCODE_OK;
    app_log_write(APP_LOG_INFO, "Camera reader subscribed to %s", sub->dds_topic_name);
    return true;
}

bool ros2_camera_sub_start(ros2_camera_sub *sub, dds_entity_t participant, const char *ros_camera_topic) {
    if (!sub || participant <= DDS_ENTITY_NIL) return false;
    if (ros_camera_topic && ros_camera_topic[0] != '\0') {
        snprintf(sub->ros_topic_name, sizeof(sub->ros_topic_name), "%s", ros_camera_topic);
    }
    return create_camera_reader(sub, participant);
}

bool ros2_camera_sub_set_topic(ros2_camera_sub *sub, dds_entity_t participant, const char *new_topic) {
    if (!sub || !new_topic || new_topic[0] == '\0') return false;

    if (sub->reader > DDS_ENTITY_NIL) {
        dds_delete(sub->reader);
        sub->reader = DDS_ENTITY_NIL;
    }
    if (sub->topic > DDS_ENTITY_NIL) {
        dds_delete(sub->topic);
        sub->topic = DDS_ENTITY_NIL;
    }

    snprintf(sub->ros_topic_name, sizeof(sub->ros_topic_name), "%s", new_topic);
    sub->has_frame = false;
    sub->frames_received = 0;
    sub->fps_counter = 0;
    sub->current_fps = 0.0f;

    if (participant > DDS_ENTITY_NIL) {
        return create_camera_reader(sub, participant);
    }
    return true;
}

static void update_texture_from_rgba(ros2_camera_sub *sub, uint32_t width, uint32_t height) {
    uint32_t w_pow2 = next_power_of_2(width);
    uint32_t h_pow2 = next_power_of_2(height);

    if (w_pow2 > 512) w_pow2 = 512;
    if (h_pow2 > 512) h_pow2 = 512;

    if (!sub->tex_allocated || sub->tex_w != w_pow2 || sub->tex_h != h_pow2) {
        if (sub->tex_allocated) {
            C3D_TexDelete(&sub->tex);
            sub->tex_allocated = false;
        }
        if (!C3D_TexInit(&sub->tex, (u16)w_pow2, (u16)h_pow2, GPU_RGBA8)) {
            app_log_write(APP_LOG_ERROR, "C3D_TexInit failed for %lux%lu", (unsigned long)w_pow2, (unsigned long)h_pow2);
            return;
        }
        C3D_TexSetFilter(&sub->tex, GPU_LINEAR, GPU_LINEAR);
        C3D_TexSetWrap(&sub->tex, GPU_CLAMP_TO_BORDER, GPU_CLAMP_TO_BORDER);
        sub->tex.border = 0x00000000;
        sub->tex_allocated = true;
        sub->tex_w = w_pow2;
        sub->tex_h = h_pow2;
    }

    uint32_t *src_pixels = (uint32_t *)sub->rgba_buffer;
    uint32_t *dst_pixels = (uint32_t *)sub->tex.data;
    uint32_t blocks_x = w_pow2 >> 3;

    for (uint32_t y = 0; y < height; y++) {
        uint32_t by = y >> 3;
        uint32_t ry = y & 7;
        uint32_t row_block_base = (by * blocks_x) << 6;
        const uint8_t *morton_row = s_morton_table[ry];
        const uint32_t *src_row = &src_pixels[y * width];

        for (uint32_t x = 0; x < width; x++) {
            uint32_t bx = x >> 3;
            uint32_t rx = x & 7;
            uint32_t dst_idx = (row_block_base + (bx << 6)) + morton_row[rx];
            dst_pixels[dst_idx] = src_row[x];
        }
    }

    C3D_TexFlush(&sub->tex);

    sub->subtex.width = (u16)width;
    sub->subtex.height = (u16)height;
    sub->subtex.left = 0.0f;
    sub->subtex.top = 1.0f;
    sub->subtex.right = (float)width / (float)w_pow2;
    sub->subtex.bottom = 1.0f - ((float)height / (float)h_pow2);

    sub->image.tex = &sub->tex;
    sub->image.subtex = &sub->subtex;
    sub->img_width = width;
    sub->img_height = height;
    sub->has_frame = true;
}

bool ros2_camera_sub_poll(ros2_camera_sub *sub) {
    if (!sub || !sub->enabled || sub->reader <= DDS_ENTITY_NIL || !sub->tj || !sub->rgba_buffer) {
        return false;
    }

    sensor_msgs_msg_dds__CompressedImage_ *sample = NULL;
    dds_sample_info_t info;
    void *samples[1] = { NULL };

    dds_return_t ret = dds_take(sub->reader, samples, &info, 1, 1);
    if (ret > 0 && info.valid_data && samples[0] != NULL) {
        sample = (sensor_msgs_msg_dds__CompressedImage_ *)samples[0];

        if (sample->data._length > 0 && sample->data._buffer != NULL) {
            int width = 0, height = 0, jpeg_subsamp = 0, jpeg_colorspace = 0;
            if (tjDecompressHeader3(sub->tj, (unsigned char *)sample->data._buffer,
                                    sample->data._length, &width, &height,
                                    &jpeg_subsamp, &jpeg_colorspace) == 0) {
                if (width > 0 && height > 0 &&
                    width <= MAX_SUPPORTED_WIDTH && height <= MAX_SUPPORTED_HEIGHT) {
                    if (tjDecompress2(sub->tj, (unsigned char *)sample->data._buffer,
                                      sample->data._length, sub->rgba_buffer,
                                      width, 0, height, TJPF_RGBA, TJFLAG_FASTDCT) == 0) {
                        update_texture_from_rgba(sub, (uint32_t)width, (uint32_t)height);
                        sub->frames_received++;
                        sub->fps_counter++;

                        uint64_t now = osGetTime();
                        if (now - sub->last_fps_time_ms >= 1000) {
                            sub->current_fps = (float)sub->fps_counter * 1000.0f / (float)(now - sub->last_fps_time_ms);
                            sub->fps_counter = 0;
                            sub->last_fps_time_ms = now;
                        }
                    }
                }
            }
        }

        dds_return_loan(sub->reader, samples, 1);
        return true;
    }

    return false;
}

void ros2_camera_sub_draw(ros2_camera_sub *sub, float screen_w, float screen_h) {
    if (!sub) return;

    if (sub->enabled && sub->has_frame && sub->tex_allocated) {
        float scale_x = screen_w / (float)sub->img_width;
        float scale_y = screen_h / (float)sub->img_height;
        float scale = (scale_x < scale_y) ? scale_x : scale_y;

        float draw_w = (float)sub->img_width * scale;
        float draw_h = (float)sub->img_height * scale;
        float draw_x = (screen_w - draw_w) * 0.5f;
        float draw_y = (screen_h - draw_h) * 0.5f;

        /* Clear pillarbox / letterbox bars with black */
        C2D_DrawRectSolid(0, 0, 0.4f, screen_w, screen_h, C2D_Color32(0, 0, 0, 255));
        /* Pillarbox / letterbox bars in black */
        C2D_DrawRectSolid(0, 0, 0.5f, screen_w, screen_h, C2D_Color32(0, 0, 0, 255));

        /* Draw hardware-accelerated video frame */
        C2D_DrawImageAt(sub->image, draw_x, draw_y, 0.5f, NULL, scale, scale);

        /* Render sleek semi-transparent OSD badge */
        C2D_DrawRectSolid(draw_x + 8, draw_y + 8, 0.6f, 150, 20, C2D_Color32(10, 15, 20, 190));
    } else {
        /* Standby HUD */
        C2D_DrawRectSolid(0, 0, 0.5f, screen_w, screen_h, C2D_Color32(22, 27, 34, 255));

        /* Standby box */
        float box_w = 340.0f;
        float box_h = 140.0f;
        float box_x = (screen_w - box_w) * 0.5f;
        float box_y = (screen_h - box_h) * 0.5f;

        C2D_DrawRectSolid(box_x, box_y, 0.6f, box_w, box_h, C2D_Color32(33, 38, 45, 255));
        C2D_DrawRectSolid(box_x + 2, box_y + 2, 0.7f, box_w - 4, box_h - 4, C2D_Color32(22, 27, 34, 255));

        /* Header bar in standby box */
        C2D_DrawRectSolid(box_x + 2, box_y + 2, 0.8f, box_w - 4, 26, C2D_Color32(48, 54, 61, 255));
    }
}

int32_t ros2_camera_sub_reader_matches(const ros2_camera_sub *sub) {
    if (!sub || sub->reader <= DDS_ENTITY_NIL) return 0;
    dds_subscription_matched_status_t status = { 0 };
    dds_return_t ret = dds_get_subscription_matched_status(sub->reader, &status);
    return (ret == DDS_RETCODE_OK) ? status.current_count : 0;
}

void ros2_camera_sub_stop(ros2_camera_sub *sub) {
    if (!sub) return;

    if (sub->reader > DDS_ENTITY_NIL) {
        dds_delete(sub->reader);
        sub->reader = DDS_ENTITY_NIL;
    }
    if (sub->topic > DDS_ENTITY_NIL) {
        dds_delete(sub->topic);
        sub->topic = DDS_ENTITY_NIL;
    }
    if (sub->tex_allocated) {
        C3D_TexDelete(&sub->tex);
        sub->tex_allocated = false;
    }
    if (sub->tj) {
        tjDestroy(sub->tj);
        sub->tj = NULL;
    }
    if (sub->rgba_buffer) {
        free(sub->rgba_buffer);
        sub->rgba_buffer = NULL;
    }
    sub->has_frame = false;
    sub->enabled = false;
}
