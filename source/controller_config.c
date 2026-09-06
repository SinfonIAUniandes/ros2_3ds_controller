#include "controller_config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "logging/app_log.h"

static void trim(char *s) {
    if (!s) return;
    char *p = s;
    while (isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

void controller_config_init_defaults(controller_config *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->domain_id = 0;
    snprintf(cfg->ros_namespace, sizeof(cfg->ros_namespace), "/nintendo_3ds");
    snprintf(cfg->camera_topic, sizeof(cfg->camera_topic), "/camera/image_raw/compressed");
    cfg->joy_publish_hz = 30;
    cfg->joy_deadzone = 0.08f;
    cfg->peer_ip[0] = '\0';
    cfg->broadcast_ip[0] = '\0';
    cfg->joy_enabled = true;
    cfg->camera_enabled = true;
}

void controller_config_set_namespace(controller_config *cfg, const char *ns) {
    if (!cfg) return;
    if (!ns || ns[0] == '\0') {
        snprintf(cfg->ros_namespace, sizeof(cfg->ros_namespace), "/");
        return;
    }
    char temp[60];
    snprintf(temp, sizeof(temp), "%s", ns);
    trim(temp);
    if (temp[0] != '/') {
        snprintf(cfg->ros_namespace, sizeof(cfg->ros_namespace), "/%.58s", temp);
    } else {
        snprintf(cfg->ros_namespace, sizeof(cfg->ros_namespace), "%.62s", temp);
    }
    size_t len = strlen(cfg->ros_namespace);
    while (len > 1 && cfg->ros_namespace[len - 1] == '/') {
        cfg->ros_namespace[--len] = '\0';
    }
}

void controller_config_set_camera_topic(controller_config *cfg, const char *topic) {
    if (!cfg) return;
    if (!topic || topic[0] == '\0') {
        snprintf(cfg->camera_topic, sizeof(cfg->camera_topic), "/camera/image_raw/compressed");
        return;
    }
    char temp[120];
    snprintf(temp, sizeof(temp), "%s", topic);
    trim(temp);
    if (temp[0] != '/') {
        snprintf(cfg->camera_topic, sizeof(cfg->camera_topic), "/%.120s", temp);
    } else {
        snprintf(cfg->camera_topic, sizeof(cfg->camera_topic), "%.124s", temp);
    }
    size_t len = strlen(cfg->camera_topic);
    while (len > 1 && cfg->camera_topic[len - 1] == '/') {
        cfg->camera_topic[--len] = '\0';
    }
}

static bool parse_config_file(controller_config *cfg, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return false;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        trim(line);
        if (line[0] == '\0' || line[0] == '#') continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        trim(key);
        trim(val);

        if (strcmp(key, "domain_id") == 0) {
            long d = strtol(val, NULL, 10);
            if (d >= 0 && d <= 232) cfg->domain_id = (uint32_t)d;
        } else if (strcmp(key, "ros_namespace") == 0) {
            controller_config_set_namespace(cfg, val);
        } else if (strcmp(key, "camera_topic") == 0) {
            controller_config_set_camera_topic(cfg, val);
        } else if (strcmp(key, "joy_publish_hz") == 0) {
            long hz = strtol(val, NULL, 10);
            if (hz >= 1 && hz <= 100) cfg->joy_publish_hz = (uint32_t)hz;
        } else if (strcmp(key, "joy_deadzone") == 0) {
            float dz = strtof(val, NULL);
            if (dz >= 0.0f && dz <= 0.5f) cfg->joy_deadzone = dz;
        } else if (strcmp(key, "peer_ip") == 0) {
            snprintf(cfg->peer_ip, sizeof(cfg->peer_ip), "%s", val);
        } else if (strcmp(key, "broadcast_ip") == 0) {
            snprintf(cfg->broadcast_ip, sizeof(cfg->broadcast_ip), "%s", val);
        } else if (strcmp(key, "joy_enabled") == 0) {
            cfg->joy_enabled = (strtol(val, NULL, 10) != 0);
        } else if (strcmp(key, "camera_enabled") == 0) {
            cfg->camera_enabled = (strtol(val, NULL, 10) != 0);
        }
    }
    fclose(f);
    return true;
}

bool controller_config_load(controller_config *cfg) {
    controller_config_init_defaults(cfg);
    (void)parse_config_file(cfg, CONTROLLER_CONFIG_ROMFS_PATH);
    bool sd_loaded = parse_config_file(cfg, CONTROLLER_CONFIG_SD_PATH);
    app_log_write(APP_LOG_INFO, sd_loaded ? "Loaded config from SD" : "Loaded default/RomFS config");
    return true;
}

bool controller_config_save(const controller_config *cfg) {
    if (!cfg) return false;
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/ros2_3ds_controller", 0777);

    FILE *f = fopen(CONTROLLER_CONFIG_SD_PATH, "w");
    if (!f) {
        app_log_write(APP_LOG_ERROR, "Failed to open %s for writing", CONTROLLER_CONFIG_SD_PATH);
        return false;
    }

    fprintf(f, "# ROS 2 3DS Controller Configuration\n");
    fprintf(f, "# Auto-generated and updated from touch screen\n\n");
    fprintf(f, "domain_id=%lu\n", (unsigned long)cfg->domain_id);
    fprintf(f, "ros_namespace=%s\n", cfg->ros_namespace);
    fprintf(f, "camera_topic=%s\n", cfg->camera_topic);
    fprintf(f, "joy_publish_hz=%lu\n", (unsigned long)cfg->joy_publish_hz);
    fprintf(f, "joy_deadzone=%.3f\n", cfg->joy_deadzone);
    fprintf(f, "peer_ip=%s\n", cfg->peer_ip);
    fprintf(f, "broadcast_ip=%s\n", cfg->broadcast_ip);
    fprintf(f, "joy_enabled=%d\n", cfg->joy_enabled ? 1 : 0);
    fprintf(f, "camera_enabled=%d\n", cfg->camera_enabled ? 1 : 0);

    fclose(f);
    app_log_write(APP_LOG_INFO, "Saved config to %s", CONTROLLER_CONFIG_SD_PATH);
    return true;
}
