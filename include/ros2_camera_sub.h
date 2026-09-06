#ifndef ROS2_3DS_ROS2_CAMERA_SUB_H
#define ROS2_3DS_ROS2_CAMERA_SUB_H

#include <stdbool.h>
#include <stdint.h>
#include <3ds.h>
#include <citro2d.h>
#include <dds/dds.h>
#include <turbojpeg.h>

typedef struct {
    dds_entity_t topic;
    dds_entity_t reader;
    dds_return_t last_result;
    bool enabled;
    char dds_topic_name[160];
    char ros_topic_name[128];

    /* TurboJPEG & Frame Buffers */
    tjhandle tj;
    uint8_t *rgba_buffer;
    size_t rgba_buffer_size;

    /* GPU Texture */
    C3D_Tex tex;
    Tex3DS_SubTexture subtex;
    C2D_Image image;
    bool tex_allocated;
    uint32_t tex_w;
    uint32_t tex_h;

    /* Telemetry */
    uint32_t img_width;
    uint32_t img_height;
    uint64_t frames_received;
    float current_fps;
    uint64_t last_fps_time_ms;
    uint32_t fps_counter;
    bool has_frame;
} ros2_camera_sub;

void ros2_camera_sub_init(ros2_camera_sub *sub);
bool ros2_camera_sub_start(ros2_camera_sub *sub, dds_entity_t participant, const char *ros_camera_topic);
bool ros2_camera_sub_set_topic(ros2_camera_sub *sub, dds_entity_t participant, const char *new_topic);
bool ros2_camera_sub_poll(ros2_camera_sub *sub);
void ros2_camera_sub_draw(ros2_camera_sub *sub, float screen_w, float screen_h);
int32_t ros2_camera_sub_reader_matches(const ros2_camera_sub *sub);
void ros2_camera_sub_stop(ros2_camera_sub *sub);

#endif
