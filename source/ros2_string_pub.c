#include "ros2_string_pub.h"

#include <stdio.h>
#include <string.h>

#include "logging/app_log.h"
#include "ros2_common.h"
#include "ros2_names.h"
#include "std_msgs_string.h"

void ros2_string_pub_init(ros2_string_pub *pub) {
    if (!pub) return;
    pub->topic = DDS_ENTITY_NIL;
    pub->writer = DDS_ENTITY_NIL;
    pub->last_result = DDS_RETCODE_OK;
    pub->published_count = 0;
    pub->last_sent[0] = '\0';
    pub->ros_topic_name[0] = '\0';
    pub->dds_topic_name[0] = '\0';
}

bool ros2_string_pub_start(ros2_string_pub *pub, dds_entity_t participant, const char *ros_namespace) {
    if (!pub || participant <= DDS_ENTITY_NIL) return false;

    if (!ros2_dds_name(pub->dds_topic_name, sizeof(pub->dds_topic_name), "rt",
                       ros_namespace, "command")) {
        snprintf(pub->dds_topic_name, sizeof(pub->dds_topic_name), "rt/nintendo_3ds/command");
    }

    if (ros_namespace != NULL && strcmp(ros_namespace, "/") == 0) {
        snprintf(pub->ros_topic_name, sizeof(pub->ros_topic_name), "/command");
    } else {
        snprintf(pub->ros_topic_name, sizeof(pub->ros_topic_name), "%s/command",
                 ros_namespace != NULL && ros_namespace[0] != '\0' ? ros_namespace : "/nintendo_3ds");
    }

    dds_qos_t *qos = NULL;
    if (!ros2_create_qos(&qos, 10, true, 0, false, NULL, false)) {
        pub->last_result = DDS_RETCODE_OUT_OF_RESOURCES;
        return false;
    }
    dds_qset_durability(qos, DDS_DURABILITY_TRANSIENT_LOCAL);

    pub->topic = dds_create_topic(participant, &std_msgs_msg_dds__String__desc,
                                  pub->dds_topic_name, NULL, NULL);
    if (pub->topic < 0) {
        pub->last_result = pub->topic;
        dds_delete_qos(qos);
        return false;
    }

    pub->writer = dds_create_writer(participant, pub->topic, qos, NULL);
    if (pub->writer < 0) {
        pub->last_result = pub->writer;
        dds_delete_qos(qos);
        ros2_string_pub_stop(pub);
        return false;
    }

    dds_delete_qos(qos);
    pub->last_result = DDS_RETCODE_OK;
    app_log_write(APP_LOG_INFO, "String command publisher started on %s", pub->dds_topic_name);
    return true;
}

bool ros2_string_pub_send(ros2_string_pub *pub, const char *text) {
    if (!pub || pub->writer <= DDS_ENTITY_NIL || !text) return false;

    std_msgs_msg_dds__String_ sample;
    sample.data = (char *)text;

    pub->last_result = dds_write(pub->writer, &sample);
    if (pub->last_result != DDS_RETCODE_OK) {
        app_log_write(APP_LOG_ERROR, "Failed to publish command string: %d", pub->last_result);
        return false;
    }

    pub->published_count++;
    snprintf(pub->last_sent, sizeof(pub->last_sent), "%s", text);
    app_log_write(APP_LOG_INFO, "Published command: %s", text);
    return true;
}

int32_t ros2_string_pub_writer_matches(const ros2_string_pub *pub) {
    if (!pub || pub->writer <= DDS_ENTITY_NIL) return 0;
    dds_publication_matched_status_t status = { 0 };
    dds_return_t ret = dds_get_publication_matched_status(pub->writer, &status);
    return (ret == DDS_RETCODE_OK) ? status.current_count : 0;
}

void ros2_string_pub_stop(ros2_string_pub *pub) {
    if (!pub) return;
    if (pub->writer > DDS_ENTITY_NIL) {
        dds_delete(pub->writer);
        pub->writer = DDS_ENTITY_NIL;
    }
    if (pub->topic > DDS_ENTITY_NIL) {
        dds_delete(pub->topic);
        pub->topic = DDS_ENTITY_NIL;
    }
}
