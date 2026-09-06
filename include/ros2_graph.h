#ifndef ROS2_3DS_ROS2_GRAPH_H
#define ROS2_3DS_ROS2_GRAPH_H

#include <stdbool.h>
#include <stdint.h>

#include <dds/dds.h>

#define ROS2_GRAPH_TOPIC "ros_discovery_info"
#define ROS2_CONTROLLER_NODE_NAME "ros2_3ds_controller"

typedef struct {
    dds_entity_t topic;
    dds_entity_t writer;
    dds_return_t last_result;
    uint64_t published;
    char node_namespace[257];
} ros2_graph;

void ros2_graph_init(ros2_graph *graph);
void ros2_graph_set_namespace(ros2_graph *graph, const char *node_namespace);
bool ros2_graph_start(ros2_graph *graph, dds_entity_t participant);
bool ros2_graph_publish(ros2_graph *graph, dds_entity_t participant,
                        dds_entity_t joy_writer,
                        dds_entity_t string_writer,
                        dds_entity_t camera_reader);
int32_t ros2_graph_writer_matches(ros2_graph *graph);
void ros2_graph_stop(ros2_graph *graph);

#endif
