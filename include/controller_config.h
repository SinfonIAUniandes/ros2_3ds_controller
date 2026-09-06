#ifndef ROS2_3DS_CONTROLLER_CONFIG_H
#define ROS2_3DS_CONTROLLER_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#define CONTROLLER_CONFIG_SD_PATH "sdmc:/3ds/ros2_3ds_controller/config.ini"
#define CONTROLLER_CONFIG_ROMFS_PATH "romfs:/config.ini"

typedef struct {
    uint32_t domain_id;
    char ros_namespace[64];
    char camera_topic[128];
    uint32_t joy_publish_hz;
    float joy_deadzone;
    char peer_ip[64];
    char broadcast_ip[64];
    bool joy_enabled;
    bool camera_enabled;
} controller_config;

void controller_config_init_defaults(controller_config *cfg);
bool controller_config_load(controller_config *cfg);
bool controller_config_save(const controller_config *cfg);
void controller_config_set_namespace(controller_config *cfg, const char *ns);
void controller_config_set_camera_topic(controller_config *cfg, const char *topic);

#endif
