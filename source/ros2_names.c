#include "ros2_names.h"

#include <stdio.h>
#include <string.h>

bool ros2_dds_name(char *output, size_t output_size, const char *prefix,
                   const char *ros_namespace, const char *resource) {
    if (output == NULL || output_size == 0 || prefix == NULL || resource == NULL) return false;
    const char *name_space = ros_namespace != NULL && ros_namespace[0] != '\0'
        ? ros_namespace : "/";
    int length = strcmp(name_space, "/") == 0
        ? snprintf(output, output_size, "%s/%s", prefix, resource)
        : snprintf(output, output_size, "%s%s/%s", prefix, name_space, resource);
    return length >= 0 && (size_t)length < output_size;
}
