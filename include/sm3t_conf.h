#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>
#include <stdint.h>

typedef bool (*sm3t_ctx_handler_t)(void *, void *, int);

typedef enum : uint8_t {
    SM3T__MODE_TRANSPARENT,
    SM3t__MODE_REVERSE,
    SM3T__MODE_SOCKS5,
    SM3t__MODE_COUNT  // counter
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
} sm3t_log_fmt_t;

typedef enum : uint8_t {
    SM3T__LOG_STDOUT,
    SM3T__LOG_FILE,
    SM3T__LOG_SYSLOG
} sm3t_log_out_t;

typedef enum : uint8_t {
    SM3T__ORIG_DST_REQUIRED,
    SM3T__ORIG_DST_OPTIONAL,
    SM3T__ORIG_DST_IGNORED
} sm3t_orig_dst_policy_t;

typedef enum : uint8_t {
    SM3T__FORWARD_ORIG,
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
    sm3t_ctx_handler_t ctx_vtable[SM3t__MODE_COUNT];
    struct {
        char *name;
        char *user;
        char *group;
        char *chroot;
        bool daemonize;
        struct {
            sm3t_log_level_t level;
            sm3t_log_fmt_t format;
            sm3t_log_out_t out;
            char *file_path;
        } logging;
    } sys;

    struct {
        struct {
            char *address;
            uint16_t port;
        } listen;

        struct {
            sm3t_orig_dst_policy_t policy;
        } orig_dst;

        struct {
            struct {
                char **cidrs;
                size_t cidrs_count;
                uint16_t *ports;
                size_t ports_count;
            } src;

            struct {
                char **cidrs;
                size_t cidrs_count;
                uint16_t *ports;
                size_t ports_count;
            } dst;
        } allow;

        struct {
            struct {
                char **cidrs;
                size_t cidrs_count;
                uint16_t *ports;
                size_t ports_count;
            } src;

            struct {
                char **cidrs;
                size_t cidrs_count;
                uint16_t *ports;
                size_t ports_count;
            } dst;
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
        } forward;

        struct {
            struct {
                uint32_t max_total;
                uint32_t max_per_src;
            } limit;

            struct {
                uint32_t connect_ms;
                uint32_t idle_ms;
                uint32_t lifetime_ms;
            } timeout;

            struct {
                bool enable;
                uint8_t max;
                uint32_t backoff_ms;
            } retries;
        } conn;

        struct {
            struct {
                size_t read_size;
                size_t write_size;
                size_t max;
            } buffer;

            sm3t_backpressure_policy_t backpressure;
        } flow_control;

        struct {
            struct {
                bool enable;
                uint32_t idle_ms;
                uint32_t interval_ms;
                uint8_t count;
            } keepalive;
            bool nodelay;
            bool reset_on_violation;
        } transport;

        struct {
            bool enable;
            struct {
                bool per_src;
                bool per_dst;
            } metrics;

            struct {
                bool src_ip;
                bool src_port;
                bool dst_ip;
                bool dst_port;
                bool bytes_in;
                bool bytes_out;
                bool duration;
                bool err;
            } log;
        } telemetry;
    } tcp;
} sm3t_conf_t;

sm3t_conf_t *sm3t__parse_conf(char *path);
sm3t_conf_t *sm3t__reload_conf(char *path);
sm3t_conf_t *sm3t__dump_conf(char *path);

#endif  // !CONFIG_H
