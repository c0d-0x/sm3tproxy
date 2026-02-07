#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>
#include <stdint.h>

typedef bool (*sm3t_ctx_handler_t)(void *, int);

typedef enum : uint8_t {
    SM3T__HTTP_PROXY,
    SM3T__HTTP_SERVER,
    SM3T__CUSTOM_SERVER,
    SM3T__TCP_PROXY,
    SM3t__MODE_COUNT
} sm3t_server_mode_t;

typedef enum : uint8_t {
    SM3T__LOG_ERROR,
    SM3T__LOG_WARN,
    SM3T__LOG_INFO,
    SM3T__LOG_DEBUG
} sm3t_log_level_t;

typedef enum : uint8_t {
    SM3T__LOG_STRUCTURED,
    SM3T__LOG_PLAIN
} sm3t_log_format_t;

typedef enum : uint8_t {
    SM3T__LOG_STDOUT,
    SM3T__LOG_FILE,
    SM3T__LOG_SYSLOG
} sm3t_log_output_t;

typedef enum : uint8_t {
    SM3T__ORIG_DEST_REQUIRED,
    SM3T__ORIG_DEST_OPTIONAL,
    SM3T__ORIG_DEST_IGNORED
} sm3t_orig_dest_policy_t;

typedef enum : uint8_t {
    SM3T__FORWARD_ORIG_DEST,
    SM3T__FORWARD_FIXED
} sm3t_forwarding_mode_t;

typedef enum : uint8_t {
    SM3T__FAILURE_DROP,
    SM3T__FAILURE_RESET,
    SM3T__FAILURE_BYPASS
} sm3t_failure_policy_t;

typedef enum : uint8_t {
    SM3T__BACKPRESSURE_STALL,
    SM3T__BACKPRESSURE_CLOSE
} sm3t_backpressure_policy_t;

typedef struct {
    // TODO: Very nested, more refactorn
    sm3t_server_mode_t mode;
    sm3t_ctx_handler_t ctx_handler_vtable[SM3t__MODE_COUNT];
    struct {
        char *name;
        char *version;
        char *user;
        char *group;
        char *chroot;
        bool daemonize;
        struct {
            sm3t_log_level_t level;
            sm3t_log_format_t format;
            sm3t_log_output_t output;
            bool log_address;
            char *file_path;
        } logging;
    } global;

    struct {
        struct {
            char *address;
            uint16_t port;
        } listen;

        struct {
            bool transparent;
            struct {
                sm3t_orig_dest_policy_t policy;
            } original_dest;
        } interception;

        struct {
            struct {
                char **cidrs;
                size_t cidrs_count;
                uint16_t *ports;
                size_t ports_count;
            } source;
            struct {
                char **cidrs;
                size_t cidrs_count;
                uint16_t *ports;
                size_t ports_count;
            } destination;
        } allow;

        struct {
            struct {
                char **cidrs;
                size_t cidrs_count;
                uint16_t *ports;
                size_t ports_count;
            } source;
            struct {
                char **cidrs;
                size_t cidrs_count;
                uint16_t *ports;
                size_t ports_count;
            } destination;
        } deny;

        struct {
            sm3t_forwarding_mode_t mode;
            struct {
                char *name;
                char *address;
                uint16_t port;
                uint32_t weight;
            } *upstreams;
            size_t upstreams_count;
            sm3t_failure_policy_t failure_policy;
        } forwarding;

        struct {
            struct {
                uint32_t max_total;
                uint32_t max_per_source;
            } limits;

            struct {
                uint32_t connect_ms;
                uint32_t idle_ms;
                uint32_t lifetime_ms;
            } timeouts;

            struct {
                bool enabled;
                uint8_t max_attempts;
                uint32_t backoff_ms;
            } retries;
        } connection;

        struct {
            struct {
                size_t read_size;
                size_t write_size;
                size_t max_buffered;
            } buffer;

            sm3t_backpressure_policy_t backpressure;
        } flow_control;

        struct {
            struct {
                bool enabled;
                uint32_t idle_ms;
                uint32_t interval_ms;
                uint8_t count;
            } keepalive;
            bool nodelay;
            bool reset_on_violation;
        } transport;

        struct {
            struct {
                bool enabled;
                bool per_source;
                bool per_destination;
            } metrics;

            struct {
                bool enabled;
                bool log_source_ip;
                bool log_source_port;
                bool log_destination_ip;
                bool log_destination_port;
                bool log_bytes_in;
                bool log_bytes_out;
                bool log_duration;
                bool log_error;
            } logging;
        } telemetry;
    } tcp_proxy;

    void *lua_hook;  // NOTE: I'm not really sure about this yet
} sm3t_conf_t;

sm3t_conf_t *sm3t__parse_conf(char *path);
sm3t_conf_t *sm3t__reload_conf(char *path);
sm3t_conf_t *sm3t__dump_conf(char *path);

#endif  // !CONFIG_H
