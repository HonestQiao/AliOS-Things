/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#ifndef BZ_TRANSPORT_H
#define BZ_TRANSPORT_H

#include <stdint.h>
#include "common.h"
#include "breeze_hal_os.h"
#include "bzopt.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum {
    TX_NOTIFICATION,
    TX_INDICATION,
};

typedef struct {
    uint8_t *p_data;
    uint16_t length;
    uint8_t cmd;
    uint8_t num_frames;
} ali_transport_xfer_evt_t;

typedef struct {
    union {
        ali_transport_xfer_evt_t rxtx;
    } data;
} ali_transport_event_t;

typedef uint32_t (*transport_tx_func_t)(uint8_t *p_data, uint16_t length);

#define TX_BUFF_LEN (BZ_MAX_SUPPORTED_MTU - 3)
#define RX_BUFF_LEN BZ_MAX_PAYLOAD_SIZE
typedef struct transport_s {
    struct {
        uint8_t buff[TX_BUFF_LEN];
        uint8_t *data;
        uint16_t len;
        uint16_t bytes_sent;
        uint8_t encrypted;
        uint8_t msg_id;
        uint8_t cmd;
        uint8_t total_frame;
        uint8_t frame_seq;
        uint8_t zeroes_padded;
        uint16_t pkt_req;
        uint16_t pkt_cfm;
        os_timer_t timer;
        transport_tx_func_t active_func;
    } tx;
    struct {
        uint8_t buff[RX_BUFF_LEN];
        uint16_t buff_size;
        uint16_t bytes_received;
        uint8_t msg_id;
        uint8_t cmd;
        uint8_t total_frame;
        uint8_t frame_seq;
        os_timer_t timer;
    } rx;
    uint16_t max_pkt_size;
    void *p_key;
    uint16_t timeout;
    void *p_aes_ctx;
} transport_t;

ret_code_t transport_init(transport_t *p_transport, ali_init_t const *p_init);
void transport_reset(transport_t *p_transport);
ret_code_t transport_tx(transport_t *p_transport, uint8_t tx_type, uint8_t cmd,
                        uint8_t const *const p_data, uint16_t length);
void transport_txdone(transport_t *p_transport, uint16_t pkt_sent);
void transport_rx(transport_t *p_transport, uint8_t *p_data, uint16_t length);
uint32_t transport_update_key(transport_t *p_transport, uint8_t *p_key);

#ifdef __cplusplus
}
#endif

#endif // BZ_TRANSPORT_H
