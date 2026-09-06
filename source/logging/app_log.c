#include "logging/app_log.h"

#include <3ds.h>

#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define APP_LOG_CAPACITY 32
#define APP_LOG_ROOT_DIRECTORY "sdmc:/3ds/ros2_3ds_controller/logs"
#define NINTENDO_EPOCH_OFFSET_SECONDS INT64_C(2208988800)

static struct {
    LightLock lock;
    app_log_record entries[APP_LOG_CAPACITY];
    u32 first;
    u32 count;
    bool has_error;
    FILE *file;
    char session_path[160];
    char day_directory[128];
    char error_directory[144];
    u32 error_count;
} g_log;

const char *app_log_level_name(app_log_level level) {
    switch (level) {
        case APP_LOG_DEBUG: return "DBG";
        case APP_LOG_INFO: return "INF";
        case APP_LOG_WARN: return "WRN";
        case APP_LOG_ERROR: return "ERR";
        case APP_LOG_DDS: return "DDS";
        default: return "UNK";
    }
}

static void create_log_directory(void) {
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/ros2_3ds_controller", 0777);
    mkdir(APP_LOG_ROOT_DIRECTORY, 0777);
}

static void day_directory_path(char *path, size_t size, time_t timestamp) {
    struct tm calendar_time;
    gmtime_r(&timestamp, &calendar_time);
    snprintf(path, size, APP_LOG_ROOT_DIRECTORY "/%04d%02d%02d",
             calendar_time.tm_year + 1900, calendar_time.tm_mon + 1, calendar_time.tm_mday);
}

static time_t unix_time_now(void) {
    return (time_t)(osGetTime() / 1000 - NINTENDO_EPOCH_OFFSET_SECONDS);
}

static void remove_directory_contents(const char *path) {
    DIR *directory = opendir(path);
    if (directory == NULL) {
        return;
    }

    struct dirent *entry;
    char child_path[512];
    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        snprintf(child_path, sizeof(child_path), "%s/%s", path, entry->d_name);
        remove(child_path);
    }
    closedir(directory);
    rmdir(path);
}

static void remove_previous_day(time_t now) {
    char previous_day[128];
    day_directory_path(previous_day, sizeof(previous_day), now - 24 * 60 * 60);

    char previous_errors[144];
    snprintf(previous_errors, sizeof(previous_errors), "%s/errors", previous_day);
    remove_directory_contents(previous_errors);
    remove_directory_contents(previous_day);
}

static void write_snapshot(const app_log_record *error) {
    char snapshot_path[192];
    snprintf(snapshot_path, sizeof(snapshot_path), "%s/error-%010llu-%03lu.log",
             g_log.error_directory, (unsigned long long)error->timestamp_ms,
             (unsigned long)g_log.error_count++);

    FILE *snapshot = fopen(snapshot_path, "w");
    if (snapshot == NULL) {
        return;
    }

    fprintf(snapshot, "ROS 2 3DS Controller error snapshot\n");
    fprintf(snapshot, "error: %s\n\nrecent events:\n", error->message);
    for (u32 offset = 0; offset < g_log.count; offset++) {
        const app_log_record *entry = &g_log.entries[(g_log.first + offset) % APP_LOG_CAPACITY];
        fprintf(snapshot, "[%010llu] %s %s\n", (unsigned long long)entry->timestamp_ms,
            app_log_level_name(entry->level), entry->message);
    }
    fclose(snapshot);
}

void app_log_init(void) {
    memset(&g_log, 0, sizeof(g_log));
    LightLock_Init(&g_log.lock);
    create_log_directory();
    time_t now = unix_time_now();
    remove_previous_day(now);
    day_directory_path(g_log.day_directory, sizeof(g_log.day_directory), now);
    mkdir(g_log.day_directory, 0777);
    snprintf(g_log.error_directory, sizeof(g_log.error_directory), "%s/errors", g_log.day_directory);
    mkdir(g_log.error_directory, 0777);

    struct tm calendar_time;
    gmtime_r(&now, &calendar_time);
    snprintf(g_log.session_path, sizeof(g_log.session_path), "%s/session-%02d%02d%02d-%03llu.log",
             g_log.day_directory, calendar_time.tm_hour, calendar_time.tm_min, calendar_time.tm_sec,
             (unsigned long long)(osGetTime() % 1000));
    g_log.file = fopen(g_log.session_path, "w");
    app_log_write(APP_LOG_INFO, "Logger initialized");
    app_log_write(APP_LOG_INFO, "Session log: %s", g_log.session_path);
    if (g_log.file == NULL) {
        app_log_write(APP_LOG_WARN, "SD log unavailable");
    }
}

void app_log_close(void) {
    LightLock_Lock(&g_log.lock);
    if (g_log.file != NULL) {
        fclose(g_log.file);
        g_log.file = NULL;
    }
    LightLock_Unlock(&g_log.lock);
}

void app_log_write(app_log_level level, const char *format, ...) {
    char message[APP_LOG_MESSAGE_SIZE];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    message[sizeof(message) - 1] = '\0';

    LightLock_Lock(&g_log.lock);
    u32 index = (g_log.first + g_log.count) % APP_LOG_CAPACITY;
    if (g_log.count == APP_LOG_CAPACITY) {
        g_log.first = (g_log.first + 1) % APP_LOG_CAPACITY;
    } else {
        g_log.count++;
    }

    app_log_record *entry = &g_log.entries[index];
    entry->timestamp_ms = osGetTime();
    entry->level = level;
    snprintf(entry->message, sizeof(entry->message), "%s", message);
    if (level == APP_LOG_ERROR) {
        g_log.has_error = true;
    }

    if (g_log.file != NULL) {
        fprintf(g_log.file, "[%010llu] %s %s\n", (unsigned long long)entry->timestamp_ms,
            app_log_level_name(level), entry->message);
        fflush(g_log.file);
    }
    if (level == APP_LOG_ERROR) {
        write_snapshot(entry);
    }
    LightLock_Unlock(&g_log.lock);
}

size_t app_log_copy_recent(app_log_record *records, size_t capacity) {
    if (records == NULL || capacity == 0) {
        return 0;
    }
    LightLock_Lock(&g_log.lock);
    size_t available = (size_t)g_log.count;
    size_t count = available < capacity ? available : capacity;
    u32 start = (g_log.first + g_log.count - count) % APP_LOG_CAPACITY;
    for (size_t offset = 0; offset < count; offset++) {
        records[offset] = g_log.entries[(start + offset) % APP_LOG_CAPACITY];
    }
    LightLock_Unlock(&g_log.lock);
    return count;
}

bool app_log_has_error(void) {
    LightLock_Lock(&g_log.lock);
    bool has_error = g_log.has_error;
    LightLock_Unlock(&g_log.lock);
    return has_error;
}

const char *app_log_file_path(void) {
    return g_log.session_path;
}
