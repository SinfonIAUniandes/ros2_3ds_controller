#include "ros2_joy_pub.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "logging/app_log.h"
#include "ros2_common.h"
#include "ros2_names.h"
#include "sensor_msgs_joy.h"

#define NINTENDO_EPOCH_OFFSET_SECONDS UINT64_C(2208988800)
#define CPAD_MAX 156.0f

static float normalize_stick(int16_t val, float max_val, float deadzone_ratio) {
    float norm = (float)val / max_val;
    if (fabsf(norm) < deadzone_ratio) {
        return 0.0f;
    }
    if (norm > 1.0f) norm = 1.0f;
    if (norm < -1.0f) norm = -1.0f;
    return norm;
}

void ros2_joy_pub_init(ros2_joy_pub *joy, float deadzone) {
    if (!joy) return;
    memset(joy, 0, sizeof(*joy));
    joy->topic = DDS_ENTITY_NIL;
    joy->writer = DDS_ENTITY_NIL;
    joy->last_result = DDS_RETCODE_OK;
    joy->enabled = true;
    joy->reliable = true;
    joy->published_count = 0;
    joy->deadzone = (deadzone >= 0.0f && deadzone <= 0.5f) ? deadzone : 0.08f;
    joy->ros_topic_name[0] = '\0';
    joy->dds_topic_name[0] = '\0';
}

bool ros2_joy_pub_start(ros2_joy_pub *joy, dds_entity_t participant, const char *ros_topic_or_ns, bool reliable) {
    if (!joy || participant <= DDS_ENTITY_NIL) return false;

    joy->reliable = reliable;

    /* If ros_topic_or_ns is specified as a full topic (e.g. ends with joy or has multiple slashes) */
    if (ros_topic_or_ns && ros_topic_or_ns[0] != '\0') {
        if (strstr(ros_topic_or_ns, "joy") != NULL) {
            snprintf(joy->ros_topic_name, sizeof(joy->ros_topic_name), "%s", ros_topic_or_ns);
            const char *raw = joy->ros_topic_name;
            while (*raw == '/') raw++;
            snprintf(joy->dds_topic_name, sizeof(joy->dds_topic_name), "rt/%s", raw);
        } else {
            /* Treat as namespace */
            if (!ros2_dds_name(joy->dds_topic_name, sizeof(joy->dds_topic_name), "rt",
                               ros_topic_or_ns, "joy")) {
                snprintf(joy->dds_topic_name, sizeof(joy->dds_topic_name), "rt/nintendo_3ds/joy");
            }
            if (strcmp(ros_topic_or_ns, "/") == 0) {
                snprintf(joy->ros_topic_name, sizeof(joy->ros_topic_name), "/joy");
            } else {
                snprintf(joy->ros_topic_name, sizeof(joy->ros_topic_name), "%s/joy", ros_topic_or_ns);
            }
        }
    } else {
        snprintf(joy->ros_topic_name, sizeof(joy->ros_topic_name), "/nintendo_3ds/joy");
        snprintf(joy->dds_topic_name, sizeof(joy->dds_topic_name), "rt/nintendo_3ds/joy");
    }

    dds_qos_t *qos = NULL;
    if (!ros2_create_qos(&qos, 10, reliable, DDS_MSECS(50), false, NULL, false)) {
        joy->last_result = DDS_RETCODE_OUT_OF_RESOURCES;
        return false;
    }

    joy->topic = dds_create_topic(participant, &sensor_msgs_msg_dds__Joy__desc,
                                  joy->dds_topic_name, NULL, NULL);
    if (joy->topic < 0) {
        joy->last_result = joy->topic;
        dds_delete_qos(qos);
        return false;
    }

    joy->writer = dds_create_writer(participant, joy->topic, qos, NULL);
    if (joy->writer < 0) {
        joy->last_result = joy->writer;
        dds_delete_qos(qos);
        ros2_joy_pub_stop(joy);
        return false;
    }

    dds_delete_qos(qos);
    joy->last_result = DDS_RETCODE_OK;
    joy->enabled = true;
    app_log_write(APP_LOG_INFO, "Joy publisher started on %s (%s, QoS: %s)",
                  joy->dds_topic_name, joy->ros_topic_name,
                  reliable ? "Reliable" : "BestEffort");
    return true;
}

bool ros2_joy_pub_publish(ros2_joy_pub *joy, uint64_t timestamp_ms,
                          const circlePosition *circle,
                          const circlePosition *cstick,
                          u32 keys_held,
                          const touchPosition *touch,
                          bool is_touching) {
    if (!joy || !joy->enabled || joy->writer <= DDS_ENTITY_NIL) {
        return false;
    }

    float axes[ROS2_JOY_NUM_AXES] = { 0 };
    int32_t buttons[ROS2_JOY_NUM_BUTTONS] = { 0 };

    /* Circle Pad:
       ROS Joy standard: Left is +1.0, Right is -1.0; Up is +1.0, Down is -1.0 */
    if (circle != NULL) {
        axes[0] = -normalize_stick(circle->dx, CPAD_MAX, joy->deadzone);
        axes[1] = normalize_stick(circle->dy, CPAD_MAX, joy->deadzone);
    }

    /* C-Stick (New 3DS / Circle Pad Pro) */
    if (cstick != NULL) {
        axes[2] = -normalize_stick(cstick->dx, CPAD_MAX, joy->deadzone);
        axes[3] = normalize_stick(cstick->dy, CPAD_MAX, joy->deadzone);
    }

    /* D-Pad axes */
    float dpad_x = 0.0f;
    if (keys_held & KEY_DLEFT) dpad_x += 1.0f;
    if (keys_held & KEY_DRIGHT) dpad_x -= 1.0f;
    axes[4] = dpad_x;

    float dpad_y = 0.0f;
    if (keys_held & KEY_DUP) dpad_y += 1.0f;
    if (keys_held & KEY_DDOWN) dpad_y -= 1.0f;
    axes[5] = dpad_y;

    /* Touch screen as extra analog axes (320x240) */
    if (is_touching && touch != NULL) {
        axes[6] = ((float)touch->px / 159.5f) - 1.0f;
        axes[7] = 1.0f - ((float)touch->py / 119.5f);
        if (axes[6] < -1.0f) axes[6] = -1.0f;
        if (axes[6] > 1.0f) axes[6] = 1.0f;
        if (axes[7] < -1.0f) axes[7] = -1.0f;
        if (axes[7] > 1.0f) axes[7] = 1.0f;
    }

    /* 15 Buttons */
    buttons[0]  = (keys_held & KEY_A) ? 1 : 0;
    buttons[1]  = (keys_held & KEY_B) ? 1 : 0;
    buttons[2]  = (keys_held & KEY_X) ? 1 : 0;
    buttons[3]  = (keys_held & KEY_Y) ? 1 : 0;
    buttons[4]  = (keys_held & KEY_L) ? 1 : 0;
    buttons[5]  = (keys_held & KEY_R) ? 1 : 0;
    buttons[6]  = (keys_held & KEY_ZL) ? 1 : 0;
    buttons[7]  = (keys_held & KEY_ZR) ? 1 : 0;
    buttons[8]  = (keys_held & KEY_SELECT) ? 1 : 0;
    buttons[9]  = (keys_held & KEY_START) ? 1 : 0;
    buttons[10] = (is_touching || (keys_held & KEY_TOUCH)) ? 1 : 0;
    buttons[11] = (keys_held & KEY_DUP) ? 1 : 0;
    buttons[12] = (keys_held & KEY_DDOWN) ? 1 : 0;
    buttons[13] = (keys_held & KEY_DLEFT) ? 1 : 0;
    buttons[14] = (keys_held & KEY_DRIGHT) ? 1 : 0;

    memcpy(joy->last_axes, axes, sizeof(axes));
    memcpy(joy->last_buttons, buttons, sizeof(buttons));

    uint64_t unix_seconds = timestamp_ms / 1000;
    if (unix_seconds >= NINTENDO_EPOCH_OFFSET_SECONDS) {
        unix_seconds -= NINTENDO_EPOCH_OFFSET_SECONDS;
    } else {
        unix_seconds = 0;
    }

    sensor_msgs_msg_dds__Joy_ sample;
    memset(&sample, 0, sizeof(sample));
    sample.header.stamp.sec = (int32_t)unix_seconds;
    sample.header.stamp.nanosec = (uint32_t)((timestamp_ms % 1000) * UINT64_C(1000000));
    sample.header.frame_id = (char *)ROS2_JOY_FRAME_ID;

    sample.axes._maximum = ROS2_JOY_NUM_AXES;
    sample.axes._length = ROS2_JOY_NUM_AXES;
    sample.axes._buffer = axes;
    sample.axes._release = false;

    sample.buttons._maximum = ROS2_JOY_NUM_BUTTONS;
    sample.buttons._length = ROS2_JOY_NUM_BUTTONS;
    sample.buttons._buffer = buttons;
    sample.buttons._release = false;

    joy->last_result = dds_write(joy->writer, &sample);
    if (joy->last_result == DDS_RETCODE_OK) {
        joy->published_count++;
        return true;
    }

    static uint64_t last_joy_err_ms = 0;
    if (timestamp_ms - last_joy_err_ms >= 2000) {
        app_log_write(APP_LOG_ERROR, "Joy dds_write failed: %d", joy->last_result);
        last_joy_err_ms = timestamp_ms;
    }
    return false;
}

int32_t ros2_joy_pub_writer_matches(const ros2_joy_pub *joy) {
    if (!joy || joy->writer <= DDS_ENTITY_NIL) return 0;
    dds_publication_matched_status_t status = { 0 };
    dds_return_t ret = dds_get_publication_matched_status(joy->writer, &status);
    return (ret == DDS_RETCODE_OK) ? status.current_count : 0;
}

void ros2_joy_pub_stop(ros2_joy_pub *joy) {
    if (!joy) return;
    if (joy->writer > DDS_ENTITY_NIL) {
        dds_delete(joy->writer);
        joy->writer = DDS_ENTITY_NIL;
    }
    if (joy->topic > DDS_ENTITY_NIL) {
        dds_delete(joy->topic);
        joy->topic = DDS_ENTITY_NIL;
    }
    joy->enabled = false;
}

bool ros2_joy_pub_set_topic(ros2_joy_pub *joy, dds_entity_t participant, const char *new_topic, bool reliable) {
    if (!joy || !new_topic || new_topic[0] == '\0') return false;

    ros2_joy_pub_stop(joy);
    joy->published_count = 0;

    if (participant > DDS_ENTITY_NIL) {
        return ros2_joy_pub_start(joy, participant, new_topic, reliable);
    }
    return true;
}
