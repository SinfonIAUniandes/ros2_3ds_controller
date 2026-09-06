#include "ros2_graph.h"

#include "ros_graph.h"

#include <stdio.h>
#include <string.h>

void ros2_graph_init(ros2_graph *graph) {
    if (!graph) return;
    graph->topic = DDS_ENTITY_NIL;
    graph->writer = DDS_ENTITY_NIL;
    graph->last_result = DDS_RETCODE_OK;
    graph->published = 0;
    snprintf(graph->node_namespace, sizeof(graph->node_namespace), "/");
}

void ros2_graph_set_namespace(ros2_graph *graph, const char *node_namespace) {
    if (!graph) return;
    snprintf(graph->node_namespace, sizeof(graph->node_namespace), "%s",
             node_namespace != NULL && node_namespace[0] != '\0' ? node_namespace : "/");
}

bool ros2_graph_start(ros2_graph *graph, dds_entity_t participant) {
    if (!graph || participant <= DDS_ENTITY_NIL) return false;

    dds_qos_t *qos = dds_create_qos();
    if (qos == NULL) {
        graph->last_result = DDS_RETCODE_OUT_OF_RESOURCES;
        return false;
    }

    dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 1);
    dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));
    dds_qset_durability(qos, DDS_DURABILITY_TRANSIENT_LOCAL);
    dds_qset_userdata(qos, "ros2_3ds=1;", sizeof("ros2_3ds=1;") - 1u);

    graph->topic = dds_create_topic(participant,
                                    &rmw_dds_common_msg_dds__ParticipantEntitiesInfo__desc,
                                    ROS2_GRAPH_TOPIC, NULL, NULL);
    if (graph->topic < 0) {
        graph->last_result = graph->topic;
        dds_delete_qos(qos);
        return false;
    }

    graph->writer = dds_create_writer(participant, graph->topic, qos, NULL);
    if (graph->writer < 0) {
        graph->last_result = graph->writer;
        dds_delete_qos(qos);
        ros2_graph_stop(graph);
        return false;
    }

    dds_delete_qos(qos);
    graph->last_result = DDS_RETCODE_OK;
    return true;
}

bool ros2_graph_publish(ros2_graph *graph, dds_entity_t participant,
                        dds_entity_t joy_writer,
                        dds_entity_t string_writer,
                        dds_entity_t camera_reader) {
    if (!graph || graph->writer <= DDS_ENTITY_NIL || participant <= DDS_ENTITY_NIL) {
        return false;
    }

    dds_guid_t participant_guid;
    dds_guid_t joy_guid;
    dds_guid_t string_guid;
    dds_guid_t camera_guid;

    graph->last_result = dds_get_guid(participant, &participant_guid);
    if (graph->last_result != DDS_RETCODE_OK) return false;

    bool has_joy = joy_writer > DDS_ENTITY_NIL;
    if (has_joy) {
        graph->last_result = dds_get_guid(joy_writer, &joy_guid);
        if (graph->last_result != DDS_RETCODE_OK) return false;
    }

    bool has_string = string_writer > DDS_ENTITY_NIL;
    if (has_string) {
        graph->last_result = dds_get_guid(string_writer, &string_guid);
        if (graph->last_result != DDS_RETCODE_OK) return false;
    }

    bool has_cam = camera_reader > DDS_ENTITY_NIL;
    if (has_cam) {
        graph->last_result = dds_get_guid(camera_reader, &camera_guid);
        if (graph->last_result != DDS_RETCODE_OK) return false;
    }

    rmw_dds_common_msg_dds__Gid_ writer_gids[2];
    rmw_dds_common_msg_dds__Gid_ reader_gids[1];
    uint32_t writer_count = 0;
    uint32_t reader_count = 0;

    if (has_joy) {
        memcpy(writer_gids[writer_count++].data, joy_guid.v, sizeof(writer_gids[0].data));
    }
    if (has_string) {
        memcpy(writer_gids[writer_count++].data, string_guid.v, sizeof(writer_gids[0].data));
    }
    if (has_cam) {
        memcpy(reader_gids[reader_count++].data, camera_guid.v, sizeof(reader_gids[0].data));
    }

    rmw_dds_common_msg_dds__NodeEntitiesInfo_ node = { 0 };
    rmw_dds_common_msg_dds__ParticipantEntitiesInfo_ sample = { 0 };
    memcpy(sample.gid.data, participant_guid.v, sizeof(sample.gid.data));

    snprintf(node.node_namespace, sizeof(node.node_namespace), "%s", graph->node_namespace);
    snprintf(node.node_name, sizeof(node.node_name), "%s", ROS2_CONTROLLER_NODE_NAME);

    node.reader_gid_seq._maximum = reader_count;
    node.reader_gid_seq._length = reader_count;
    node.reader_gid_seq._buffer = reader_count > 0 ? reader_gids : NULL;
    node.reader_gid_seq._release = false;

    node.writer_gid_seq._maximum = writer_count;
    node.writer_gid_seq._length = writer_count;
    node.writer_gid_seq._buffer = writer_count > 0 ? writer_gids : NULL;
    node.writer_gid_seq._release = false;

    sample.node_entities_info_seq._maximum = 1;
    sample.node_entities_info_seq._length = 1;
    sample.node_entities_info_seq._buffer = &node;
    sample.node_entities_info_seq._release = false;

    graph->last_result = dds_write(graph->writer, &sample);
    if (graph->last_result != DDS_RETCODE_OK) {
        return false;
    }
    graph->published++;
    return true;
}

int32_t ros2_graph_writer_matches(ros2_graph *graph) {
    if (!graph || graph->writer <= DDS_ENTITY_NIL) return 0;
    dds_publication_matched_status_t status = { 0 };
    graph->last_result = dds_get_publication_matched_status(graph->writer, &status);
    if (graph->last_result != DDS_RETCODE_OK) {
        return graph->last_result;
    }
    return status.current_count;
}

void ros2_graph_stop(ros2_graph *graph) {
    if (!graph) return;
    if (graph->topic > DDS_ENTITY_NIL) {
        dds_delete(graph->topic);
    }
    graph->topic = DDS_ENTITY_NIL;
    graph->writer = DDS_ENTITY_NIL;
}
