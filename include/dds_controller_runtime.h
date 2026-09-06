#ifndef ROS2_3DS_DDS_CONTROLLER_RUNTIME_H
#define ROS2_3DS_DDS_CONTROLLER_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>
#include <3ds.h>
#include <dds/dds.h>

#include "controller_config.h"
#include "ros2_joy_pub.h"
#include "ros2_string_pub.h"
#include "ros2_camera_sub.h"
#include "ros2_graph.h"

typedef struct {
    dds_entity_t domain;
    dds_entity_t participant;
    dds_return_t last_result;
    bool running;

    controller_config config;

    ros2_joy_pub joy;
    ros2_string_pub string_cmd;
    ros2_camera_sub camera;
    ros2_graph graph;
} dds_controller_runtime;

void dds_controller_runtime_init(dds_controller_runtime *rt);
bool dds_controller_runtime_start(dds_controller_runtime *rt, const controller_config *cfg);
void dds_controller_runtime_stop(dds_controller_runtime *rt);
bool dds_controller_runtime_restart(dds_controller_runtime *rt, const controller_config *cfg);

bool dds_controller_runtime_set_domain_id(dds_controller_runtime *rt, uint32_t new_domain_id);
bool dds_controller_runtime_set_namespace(dds_controller_runtime *rt, const char *new_namespace);
bool dds_controller_runtime_set_camera_topic(dds_controller_runtime *rt, const char *new_camera_topic);

bool dds_controller_runtime_publish_joy(dds_controller_runtime *rt, uint64_t timestamp_ms,
                                        const circlePosition *circle,
                                        const circlePosition *cstick,
                                        u32 keys_held,
                                        const touchPosition *touch,
                                        bool is_touching);
bool dds_controller_runtime_publish_string(dds_controller_runtime *rt, const char *text);
bool dds_controller_runtime_poll_camera(dds_controller_runtime *rt);
bool dds_controller_runtime_refresh_graph(dds_controller_runtime *rt);

#endif
