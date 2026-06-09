#ifndef SLORA_H
#define SLORA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct slora_node_handle slora_node_handle_t;

enum {
    SLORA_ACTION_TX_BROADCAST = 0,
    SLORA_ACTION_TX_DATA      = 1,
    SLORA_ACTION_TUNE_DATA    = 2,
    SLORA_ACTION_SCHEDULE     = 3,
    SLORA_ACTION_CANCEL       = 4,
    SLORA_ACTION_DELIVER      = 5,
};

typedef struct {
    uint8_t tag;
    union {
        struct {
            const uint8_t* bytes;
            size_t bytes_len;
        } tx_broadcast;
        struct {
            const uint8_t* bytes;
            size_t bytes_len;
            uint32_t freq_hz;
            uint32_t bw_hz;
        } tx_data;
        struct {
            uint32_t freq_hz;
            uint32_t bw_hz;
        } tune_data;
        struct {
            uint32_t token;
            uint64_t delay_ns;
        } schedule;
        struct {
            uint32_t token;
        } cancel;
        struct {
            uint8_t from[16];
            const uint8_t* payload;
            size_t payload_len;
        } deliver;
    } payload;
} slora_action_t;

slora_node_handle_t* slora_node_init(const uint8_t seed[32]);
void slora_node_deinit(slora_node_handle_t* node);

void slora_node_addr(slora_node_handle_t* node, uint8_t out[16]);

uint32_t slora_node_link_count(slora_node_handle_t* node);
uint32_t slora_node_neighbor_count(slora_node_handle_t* node);
uint32_t slora_node_route_count(slora_node_handle_t* node);
uint32_t slora_node_pending_tx_total(slora_node_handle_t* node);
uint32_t slora_node_forwarded_count(slora_node_handle_t* node);
uint32_t slora_node_drop_no_route(slora_node_handle_t* node);
uint32_t slora_node_drop_no_link(slora_node_handle_t* node);
uint32_t slora_node_drop_link_down(slora_node_handle_t* node);
uint32_t slora_node_drop_queue_full(slora_node_handle_t* node);
void slora_node_add_target_peer(slora_node_handle_t* node, const uint8_t peer_addr[16]);
void slora_node_set_announce_interval(slora_node_handle_t* node, uint64_t interval_ns, uint64_t jitter_ns);

void slora_node_start(slora_node_handle_t* node, uint64_t now_ns);

void slora_node_on_broadcast_rx(slora_node_handle_t* node,
                                const uint8_t* bytes, size_t bytes_len,
                                uint64_t now_ns);

void slora_node_on_data_rx(slora_node_handle_t* node,
                           const uint8_t* bytes, size_t bytes_len,
                           uint32_t freq_hz, uint32_t bw_hz,
                           uint64_t now_ns);

void slora_node_on_wakeup(slora_node_handle_t* node, uint32_t token, uint64_t now_ns);

void slora_node_send_payload(slora_node_handle_t* node,
                             const uint8_t dest[16],
                             const uint8_t* payload, size_t payload_len,
                             uint64_t now_ns);

void slora_node_send_echo(slora_node_handle_t* node,
                          const uint8_t dest[16],
                          const uint8_t* payload, size_t payload_len,
                          uint64_t now_ns);

bool slora_node_next_action(slora_node_handle_t* node, slora_action_t* out);

#ifdef __cplusplus
}
#endif

#endif
