#ifndef ROS2_3DS_APP_LOG_H
#define ROS2_3DS_APP_LOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define APP_LOG_MESSAGE_SIZE 384

typedef enum {
    APP_LOG_DEBUG,
    APP_LOG_INFO,
    APP_LOG_WARN,
    APP_LOG_ERROR,
    APP_LOG_DDS
} app_log_level;

typedef struct {
    uint64_t timestamp_ms;
    app_log_level level;
    char message[APP_LOG_MESSAGE_SIZE];
} app_log_record;

void app_log_init(void);
void app_log_close(void);
void app_log_write(app_log_level level, const char *format, ...);
bool app_log_has_error(void);
const char *app_log_file_path(void);
const char *app_log_level_name(app_log_level level);
size_t app_log_copy_recent(app_log_record *records, size_t capacity);

#endif
