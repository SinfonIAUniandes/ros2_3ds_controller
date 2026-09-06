#ifndef ROS2_3DS_ROS2_JOY_PUB_H
#define ROS2_3DS_ROS2_JOY_PUB_H

#include <stdbool.h>
#include <stdint.h>
#include <3ds.h>
#include <dds/dds.h>

#define ROS2_JOY_NUM_AXES 8
#define ROS2_JOY_NUM_BUTTONS 15
#define ROS2_JOY_FRAME_ID "3ds_controller"

typedef struct {
    dds_entity_t topic;
    dds_entity_t writer;
    dds_return_t last_result;
    bool enabled;
    bool reliable;
    uint64_t published_count;
    float last_axes[ROS2_JOY_NUM_AXES];
    int32_t last_buttons[ROS2_JOY_NUM_BUTTONS];
    char ros_topic_name[128];
    char dds_topic_name[160];
    float deadzone;
} ros2_joy_pub;

void ros2_joy_pub_init(ros2_joy_pub *joy, float deadzone);
bool ros2_joy_pub_start(ros2_joy_pub *joy, dds_entity_t participant, const char *ros_topic_or_ns, bool reliable);
bool ros2_joy_pub_publish(ros2_joy_pub *joy, uint64_t timestamp_ms,
                          const circlePosition *circle,
                          const circlePosition *cstick,
                          u32 keys_held,
                          const touchPosition *touch,
                          bool is_touching);
int32_t ros2_joy_pub_writer_matches(const ros2_joy_pub *joy);
void ros2_joy_pub_stop(ros2_joy_pub *joy);

#endif
