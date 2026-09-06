#include "dds_controller_runtime.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

#include "logging/app_log.h"

#define DDS_3DS_GENERAL_CONFIG \
    "<General><MaxMessageSize>1456 B</MaxMessageSize>" \
    "<MaxRexmitMessageSize>1456 B</MaxRexmitMessageSize>" \
    "<FragmentSize>1344 B</FragmentSize></General>" \
    "<Internal>" \
    "<SocketReceiveBufferSize min=\"64kB\" max=\"256kB\"/>" \
    "<SocketSendBufferSize min=\"32kB\" max=\"64kB\"/>" \
    "<DeliveryQueueMaxSamples>8</DeliveryQueueMaxSamples>" \
    "<DefragUnreliableMaxSamples>4</DefragUnreliableMaxSamples>" \
    "</Internal>"

void dds_controller_runtime_init(dds_controller_runtime *rt) {
    if (!rt) return;
    memset(rt, 0, sizeof(*rt));
    rt->domain = DDS_ENTITY_NIL;
    rt->participant = DDS_ENTITY_NIL;
    rt->last_result = DDS_RETCODE_OK;
    rt->running = false;

    controller_config_init_defaults(&rt->config);
    ros2_joy_pub_init(&rt->joy, rt->config.joy_deadzone);
    ros2_string_pub_init(&rt->string_cmd);
    ros2_camera_sub_init(&rt->camera);
    ros2_graph_init(&rt->graph);
}

bool dds_controller_runtime_start(dds_controller_runtime *rt, const controller_config *cfg) {
    if (!rt) return false;
    if (rt->running) return true;

    if (cfg) {
        rt->config = *cfg;
    }

    const char *domain_config = "<CycloneDDS><Domain Id=\"any\">" DDS_3DS_GENERAL_CONFIG
                                "</Domain></CycloneDDS>";
    char peer_domain_config[1024];

    const char *peer_ip = rt->config.peer_ip;
    const char *broadcast_ip = rt->config.broadcast_ip;

    if ((peer_ip && peer_ip[0] != '\0') || (broadcast_ip && broadcast_ip[0] != '\0')) {
        const bool explicit_peer = (peer_ip && peer_ip[0] != '\0');
        const char *address = explicit_peer ? peer_ip : broadcast_ip;
        struct in_addr parsed;
        if (inet_pton(AF_INET, address, &parsed) == 1) {
            char peer_locator[96];
            snprintf(peer_locator, sizeof(peer_locator), "%.64s:7400", address);
            if (explicit_peer) {
                snprintf(peer_domain_config, sizeof(peer_domain_config),
                    "<CycloneDDS><Domain Id=\"any\">" DDS_3DS_GENERAL_CONFIG
                    "<Discovery><ParticipantIndex>auto</ParticipantIndex>"
                    "<MaxAutoParticipantIndex>9</MaxAutoParticipantIndex>"
                    "<Peers><Peer Address=\"%s\" PruneDelay=\"inf\"/></Peers></Discovery>"
                    "</Domain></CycloneDDS>", peer_locator);
            } else {
                snprintf(peer_domain_config, sizeof(peer_domain_config),
                    "<CycloneDDS><Domain Id=\"any\">" DDS_3DS_GENERAL_CONFIG
                    "<Discovery><ParticipantIndex>auto</ParticipantIndex>"
                    "<MaxAutoParticipantIndex>9</MaxAutoParticipantIndex>"
                    "<Peers><Peer Address=\"%s\" PruneDelay=\"inf\"/>"
                    "<Peer Address=\"%.64s\" PruneDelay=\"inf\"/></Peers></Discovery>"
                    "</Domain></CycloneDDS>", peer_locator, address);
            }
            domain_config = peer_domain_config;
        }
    }

    rt->domain = dds_create_domain(rt->config.domain_id, domain_config);
    if (rt->domain < 0) {
        rt->last_result = rt->domain;
        rt->domain = DDS_ENTITY_NIL;
        app_log_write(APP_LOG_ERROR, "dds_create_domain failed: %d", rt->last_result);
        return false;
    }

    rt->participant = dds_create_participant(rt->config.domain_id, NULL, NULL);
    if (rt->participant < 0) {
        rt->last_result = rt->participant;
        rt->participant = DDS_ENTITY_NIL;
        dds_delete(rt->domain);
        rt->domain = DDS_ENTITY_NIL;
        app_log_write(APP_LOG_ERROR, "dds_create_participant failed: %d", rt->last_result);
        return false;
    }

    ros2_joy_pub_init(&rt->joy, rt->config.joy_deadzone);
    if (rt->config.joy_enabled) {
        const char *joy_topic = (rt->config.joy_topic[0] != '\0')
            ? rt->config.joy_topic : rt->config.ros_namespace;
        (void)ros2_joy_pub_start(&rt->joy, rt->participant, joy_topic, rt->config.joy_reliable);
    }

    (void)ros2_string_pub_start(&rt->string_cmd, rt->participant, rt->config.ros_namespace);

    if (rt->config.camera_enabled) {
        (void)ros2_camera_sub_start(&rt->camera, rt->participant, rt->config.camera_topic);
    }

    ros2_graph_set_namespace(&rt->graph, rt->config.ros_namespace);
    if (ros2_graph_start(&rt->graph, rt->participant)) {
        (void)dds_controller_runtime_refresh_graph(rt);
    }

    rt->running = true;
    app_log_write(APP_LOG_INFO, "DDS runtime started (Domain ID: %lu, NS: %s, Joy: %s)",
                  (unsigned long)rt->config.domain_id, rt->config.ros_namespace, rt->config.joy_topic);
    return true;
}

void dds_controller_runtime_stop(dds_controller_runtime *rt) {
    if (!rt) return;

    if (rt->running) {
        ros2_graph_stop(&rt->graph);
        ros2_camera_sub_stop(&rt->camera);
        ros2_string_pub_stop(&rt->string_cmd);
        ros2_joy_pub_stop(&rt->joy);
    }

    if (rt->participant > DDS_ENTITY_NIL) {
        dds_delete(rt->participant);
        rt->participant = DDS_ENTITY_NIL;
    }

    if (rt->domain > DDS_ENTITY_NIL) {
        dds_delete(rt->domain);
        rt->domain = DDS_ENTITY_NIL;
    }

    rt->running = false;
    app_log_write(APP_LOG_INFO, "DDS runtime stopped");
}

bool dds_controller_runtime_restart(dds_controller_runtime *rt, const controller_config *cfg) {
    dds_controller_runtime_stop(rt);
    return dds_controller_runtime_start(rt, cfg);
}

bool dds_controller_runtime_set_domain_id(dds_controller_runtime *rt, uint32_t new_domain_id) {
    if (!rt || new_domain_id > 232) return false;
    if (rt->config.domain_id == new_domain_id && rt->running) return true;

    rt->config.domain_id = new_domain_id;
    controller_config_save(&rt->config);
    return dds_controller_runtime_restart(rt, &rt->config);
}

bool dds_controller_runtime_set_namespace(dds_controller_runtime *rt, const char *new_namespace) {
    if (!rt || !new_namespace) return false;

    controller_config_set_namespace(&rt->config, new_namespace);
    controller_config_save(&rt->config);
    return dds_controller_runtime_restart(rt, &rt->config);
}

bool dds_controller_runtime_set_joy_topic(dds_controller_runtime *rt, const char *new_joy_topic) {
    if (!rt || !new_joy_topic) return false;

    controller_config_set_joy_topic(&rt->config, new_joy_topic);
    controller_config_save(&rt->config);
    return dds_controller_runtime_restart(rt, &rt->config);
}

bool dds_controller_runtime_set_joy_reliable(dds_controller_runtime *rt, bool reliable) {
    if (!rt) return false;
    if (rt->config.joy_reliable == reliable && rt->running) return true;

    rt->config.joy_reliable = reliable;
    controller_config_save(&rt->config);
    return dds_controller_runtime_restart(rt, &rt->config);
}

bool dds_controller_runtime_set_camera_topic(dds_controller_runtime *rt, const char *new_camera_topic) {
    if (!rt || !new_camera_topic) return false;

    controller_config_set_camera_topic(&rt->config, new_camera_topic);
    controller_config_save(&rt->config);

    if (rt->running && rt->participant > DDS_ENTITY_NIL) {
        bool ok = ros2_camera_sub_set_topic(&rt->camera, rt->participant, rt->config.camera_topic);
        (void)dds_controller_runtime_refresh_graph(rt);
        return ok;
    }
    return true;
}

int32_t dds_controller_runtime_joy_matches(const dds_controller_runtime *rt) {
    return rt ? ros2_joy_pub_writer_matches(&rt->joy) : 0;
}

int32_t dds_controller_runtime_string_matches(const dds_controller_runtime *rt) {
    return rt ? ros2_string_pub_writer_matches(&rt->string_cmd) : 0;
}

int32_t dds_controller_runtime_camera_matches(const dds_controller_runtime *rt) {
    return rt ? ros2_camera_sub_reader_matches(&rt->camera) : 0;
}

bool dds_controller_runtime_publish_joy(dds_controller_runtime *rt, uint64_t timestamp_ms,
                                        const circlePosition *circle,
                                        const circlePosition *cstick,
                                        u32 keys_held,
                                        const touchPosition *touch,
                                        bool is_touching) {
    if (!rt || !rt->running) return false;
    return ros2_joy_pub_publish(&rt->joy, timestamp_ms, circle, cstick, keys_held, touch, is_touching);
}

bool dds_controller_runtime_publish_string(dds_controller_runtime *rt, const char *text) {
    if (!rt || !rt->running) return false;
    return ros2_string_pub_send(&rt->string_cmd, text);
}

bool dds_controller_runtime_poll_camera(dds_controller_runtime *rt) {
    if (!rt || !rt->running) return false;
    return ros2_camera_sub_poll(&rt->camera);
}

bool dds_controller_runtime_refresh_graph(dds_controller_runtime *rt) {
    if (!rt || !rt->running || rt->participant <= DDS_ENTITY_NIL) return false;
    return ros2_graph_publish(&rt->graph, rt->participant,
                              rt->joy.writer,
                              rt->string_cmd.writer,
                              rt->camera.reader);
}
