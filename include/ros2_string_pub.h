#ifndef ROS2_3DS_ROS2_STRING_PUB_H
#define ROS2_3DS_ROS2_STRING_PUB_H

#include <stdbool.h>
#include <stdint.h>

#include <dds/dds.h>

typedef struct {
    dds_entity_t topic;
    dds_entity_t writer;
    dds_return_t last_result;
    uint64_t published_count;
    char last_sent[128];
    char ros_topic_name[128];
    char dds_topic_name[160];
} ros2_string_pub;

void ros2_string_pub_init(ros2_string_pub *pub);
bool ros2_string_pub_start(ros2_string_pub *pub, dds_entity_t participant, const char *ros_namespace);
bool ros2_string_pub_send(ros2_string_pub *pub, const char *text);
int32_t ros2_string_pub_writer_matches(const ros2_string_pub *pub);
void ros2_string_pub_stop(ros2_string_pub *pub);

#endif
