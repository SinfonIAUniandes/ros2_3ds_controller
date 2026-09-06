#ifndef ROS2_3DS_ROS2_NAMES_H
#define ROS2_3DS_ROS2_NAMES_H

#include <stdbool.h>
#include <stddef.h>

bool ros2_dds_name(char *output, size_t output_size, const char *prefix,
                   const char *ros_namespace, const char *resource);

#endif
