#include "ros2_common.h"

#include <stdio.h>
#include <string.h>

bool ros2_create_qos(dds_qos_t **qos, int history_depth, bool reliable, int64_t lease_ns,
                     bool ignore_local, const char *service_id,
                     bool disable_writer_data_lifecycle) {
    if (qos == NULL) {
        return false;
    }

    *qos = dds_create_qos();
    if (*qos == NULL) {
        return false;
    }

    dds_qset_history(*qos, DDS_HISTORY_KEEP_LAST, history_depth > 0 ? history_depth : 1);
    dds_qset_reliability(*qos, reliable ? DDS_RELIABILITY_RELIABLE : DDS_RELIABILITY_BEST_EFFORT,
                         lease_ns > 0 ? DDS_MSECS(lease_ns / 1000000LL) : DDS_INFINITY);
    dds_qset_durability(*qos, DDS_DURABILITY_VOLATILE);
    if (disable_writer_data_lifecycle) {
        dds_qset_writer_data_lifecycle(*qos, false);
    }
    if (ignore_local) {
        dds_qset_ignorelocal(*qos, DDS_IGNORELOCAL_PARTICIPANT);
    }
    if (service_id != NULL && service_id[0] != '\0') {
        char user_data[96];
        int length = snprintf(user_data, sizeof(user_data), "%sros2_3ds=1;", service_id);
        if (length < 0 || (size_t)length >= sizeof(user_data)) {
            dds_delete_qos(*qos);
            *qos = NULL;
            return false;
        }
        dds_qset_userdata(*qos, user_data, (size_t)length);
    } else {
        static const char user_data[] = "ros2_3ds=1;";
        dds_qset_userdata(*qos, user_data, sizeof(user_data) - 1u);
    }
    return true;
}

void ros2_destroy_qos(dds_qos_t *qos) {
    if (qos != NULL) {
        dds_delete_qos(qos);
    }
}

bool ros2_topic_create_endpoints(ros2_topic_endpoint *endpoint, dds_entity_t participant,
                                 int history_depth, bool reliable, int64_t lease_ns,
                                 bool ignore_local) {
    if (endpoint == NULL || participant <= DDS_ENTITY_NIL || endpoint->type == NULL) {
        return false;
    }

    dds_qos_t *writer_qos = NULL;
    dds_qos_t *reader_qos = NULL;
    if (!ros2_create_qos(&writer_qos, history_depth, reliable, lease_ns, ignore_local, NULL, false) ||
        !ros2_create_qos(&reader_qos, history_depth, reliable, lease_ns, ignore_local, NULL, false)) {
        ros2_destroy_qos(writer_qos);
        ros2_destroy_qos(reader_qos);
        endpoint->last_result = DDS_RETCODE_OUT_OF_RESOURCES;
        return false;
    }

    endpoint->topic = dds_create_topic(participant, endpoint->type, endpoint->name, NULL, NULL);
    if (endpoint->topic < 0) {
        endpoint->last_result = endpoint->topic;
        endpoint->topic = DDS_ENTITY_NIL;
        ros2_destroy_qos(writer_qos);
        ros2_destroy_qos(reader_qos);
        return false;
    }

    if (endpoint->writer_enabled) {
        endpoint->writer = dds_create_writer(participant, endpoint->topic, writer_qos, NULL);
        if (endpoint->writer < 0) {
            endpoint->last_result = endpoint->writer;
            endpoint->writer = DDS_ENTITY_NIL;
            ros2_destroy_qos(writer_qos);
            ros2_destroy_qos(reader_qos);
            ros2_topic_cleanup(endpoint);
            return false;
        }
    }

    if (endpoint->reader_enabled) {
        endpoint->reader = dds_create_reader(participant, endpoint->topic, reader_qos, NULL);
        if (endpoint->reader < 0) {
            endpoint->last_result = endpoint->reader;
            endpoint->reader = DDS_ENTITY_NIL;
            ros2_destroy_qos(writer_qos);
            ros2_destroy_qos(reader_qos);
            ros2_topic_cleanup(endpoint);
            return false;
        }
    }

    ros2_destroy_qos(writer_qos);
    ros2_destroy_qos(reader_qos);
    endpoint->last_result = DDS_RETCODE_OK;
    return true;
}

void ros2_topic_cleanup(ros2_topic_endpoint *endpoint) {
    if (endpoint == NULL) return;
    if (endpoint->writer >= 0) {
        dds_delete(endpoint->writer);
        endpoint->writer = DDS_ENTITY_NIL;
    }
    if (endpoint->reader >= 0) {
        dds_delete(endpoint->reader);
        endpoint->reader = DDS_ENTITY_NIL;
    }
    if (endpoint->topic >= 0) {
        dds_delete(endpoint->topic);
        endpoint->topic = DDS_ENTITY_NIL;
    }
}
