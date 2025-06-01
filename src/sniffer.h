extern void log_packet(uint8_t *psdu, size_t length, uint8_t lqi, int8_t rssi, uint64_t timestamp);

typedef void (*sniffer_init_fn_t)();
typedef void (*sniffer_start_fn_t)();
typedef void (*sniffer_stop_fn_t)();
typedef uint32_t (*sniffer_get_channel_fn_t)();
typedef uint32_t (*sniffer_get_channel_min_fn_t)();
typedef uint32_t (*sniffer_get_channel_max_fn_t)();
typedef void (*sniffer_set_channel_fn_t)(uint32_t channel);

typedef struct {
    sniffer_init_fn_t init;
    sniffer_start_fn_t start;
    sniffer_stop_fn_t stop;
    sniffer_get_channel_fn_t get_channel;
    sniffer_set_channel_fn_t set_channel;
    sniffer_get_channel_min_fn_t get_channel_min;
    sniffer_get_channel_max_fn_t get_channel_max;
} sniffer_t;

extern sniffer_t sniffer_zephyr;
