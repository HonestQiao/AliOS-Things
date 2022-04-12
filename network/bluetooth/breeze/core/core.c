/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#include "ali_core.h"
#include "ble_ais.h"
#include "ali_transport.h"
#include "ali_auth.h"
#include "ali_ext.h"
#include "ali_common.h"
#include "breeze_export.h"
#include <string.h>
#include <breeze_hal_ble.h>
#ifdef CONFIG_AIS_SECURE_ADV
#include "sha256.h"
#include <aos/kv.h>
#endif

#define MODULE_INITIALIZED (p_ali->is_initialized)

#define FMSK_BLUETOOTH_VER_Pos 0
#define FMSK_OTA_Pos 2
#define FMSK_SECURITY_Pos 3
#define FMSK_SECRET_TYPE_Pos 4
#define FMSK_SIGNED_ADV_Pos 5

#define MAC_ASCII_LEN 6
#define MAX_SECRET_LEN ALI_AUTH_SECRET_LEN_MAX

#define RX_BUFF_LEN ALI_TRANSPORT_MAX_RX_DATA_LEN

#define OTA_RX_BUFF_LEN   (256)

#define SET_U16_LE(data, val)                                    \
    {                                                            \
        *(uint8_t *)(data)       = (uint8_t)(val & 0xFF);        \
        *((uint8_t *)(data) + 1) = (uint8_t)((val >> 8) & 0xFF); \
    }

#define SET_U32_LE(data, val)                                     \
    {                                                             \
        *(uint8_t *)(data)       = (uint8_t)(val & 0xFF);         \
        *((uint8_t *)(data) + 1) = (uint8_t)((val >> 8) & 0xFF);  \
        *((uint8_t *)(data) + 2) = (uint8_t)((val >> 16) & 0xFF); \
        *((uint8_t *)(data) + 3) = (uint8_t)((val >> 24) & 0xFF); \
    }

extern struct bt_conn *g_conn;
ali_t *g_ali;

#ifdef CONFIG_AIS_SECURE_ADV
#define AIS_SEQ_KV_KEY      "ais_adv_seq"
#define AIS_SEQ_UPDATE_FREQ (1 * 60 * 60) /* in second uint */
static uint32_t g_seq = 0;
#endif

static uint8_t const m_valid_rx_cmd[10] /**< Valid command for Rx. */
  = { ALI_CMD_CTRL,           ALI_CMD_QUERY,
      ALI_CMD_EXT_DOWN,       ALI_CMD_AUTH_REQ,
      ALI_CMD_AUTH_CFM,       ALI_CMD_FW_VERSION_REQ,
      ALI_CMD_FW_UPGRADE_REQ, ALI_CMD_FW_GET_INIT_FW_SIZE,
      ALI_CMD_FW_XFER_FINISH, ALI_CMD_FW_DATA };


static uint8_t const m_valid_tx_cmd[10] /**< Valid command for Tx. */
  = { ALI_CMD_STATUS,           ALI_CMD_REPLY,
      ALI_CMD_EXT_UP,           ALI_CMD_AUTH_RAND,
      ALI_CMD_AUTH_RSP,         ALI_CMD_AUTH_KEY,
      ALI_CMD_FW_VERSION_RSP,   ALI_CMD_FW_UPGRADE_RSP,
      ALI_CMD_FW_BYTES_RECEIVED,ALI_CMD_FW_CHECK_RESULT,
      ALI_CMD_FW_UPDATE_PROCESS,ALI_CMD_ERROR};


/**@brief Function to check whether the received command is a valid command. */
static bool is_valid_rx_command(uint8_t cmd)
{
    for (uint8_t i = 0; i < sizeof(m_valid_rx_cmd); i++) {
        if (cmd == m_valid_rx_cmd[i]) {
            return true;
        }
    }

    return false;
}

/**@brief Function to check whether the tx command is a valid command. */
static bool is_valid_tx_command(uint8_t cmd)
{
    for (uint8_t i = 0; i < sizeof(m_valid_tx_cmd); i++) {
        if (cmd == m_valid_tx_cmd[i]) {
            return true;
        }
    }

    return false;
}

/**@brief Notify error to higher layer. */
static void notify_error(ali_t *p_ali, uint32_t src, uint32_t err_code)
{
    ali_event_t evt;

    /* send event to higher layer. */
    evt.type                = ALI_EVT_ERROR;
    evt.data.error.source   = src;
    evt.data.error.err_code = err_code;
    p_ali->event_handler(p_ali->p_evt_context, &evt);
}


/**@brief Notify event without data to higher layer.
 * @note  Only for ALI_EVT_CONNECTED, ALI_EVT_DISCONNECTED and
 * ALI_EVT_AUTHENTICATED. */
void notify_evt_no_data(ali_t *p_ali, ali_evt_type_t evt_type)
{
    ali_event_t evt;

    /* send event to higher layer. */
    evt.type = evt_type;
    p_ali->event_handler(p_ali->p_evt_context, &evt);
}


/**@brief Notify received data to higher layer. */
static void notify_ctrl_data(ali_t *p_ali, uint8_t *data, uint16_t len)
{
    ali_event_t evt;

    /* send event to higher layer. */
    evt.type                = ALI_EVT_CTRL_RECEIVED;
    evt.data.rx_data.p_data = data;
    evt.data.rx_data.length = len;
    p_ali->event_handler(p_ali->p_evt_context, &evt);
}


/**@brief Notify received data to higher layer. */
static void notify_query_data(ali_t *p_ali, uint8_t *data, uint16_t len)
{
    ali_event_t evt;

    /* send event to higher layer. */
    evt.type                = ALI_EVT_QUERY_RECEIVED;
    evt.data.rx_data.p_data = data;
    evt.data.rx_data.length = len;
    p_ali->event_handler(p_ali->p_evt_context, &evt);
}


/**@brief Notify received data to higher layer. */
static void notify_apinfo(ali_t *p_ali, uint8_t *data, uint16_t len)
{
    ali_event_t evt;

    /* send event to higher layer. */
    evt.type                = ALI_EVT_APINFO;
    evt.data.rx_data.p_data = data;
    evt.data.rx_data.length = len;
    p_ali->event_handler(p_ali->p_evt_context, &evt);
}


breeze_otainfo_t g_ota_info;
/**@brief Notify received ota cmd to higher layer. */
static void notify_ota_command(ali_t *p_ali, uint8_t cmd, uint8_t num_frame, uint8_t *data, uint16_t len)
{
    ali_event_t evt;

    if ((cmd & ALI_CMD_TYPE_MASK) != ALI_CMD_TYPE_FW_UPGRADE) {
        return;
    }

    g_ota_info.type = OTA_CMD;
    g_ota_info.cmd_evt.m_cmd.cmd = cmd;
    g_ota_info.cmd_evt.m_cmd.frame = num_frame;
    memcpy(g_ota_info.cmd_evt.m_cmd.data, data, len);
    g_ota_info.cmd_evt.m_cmd.len = len;
    /* send event to higher layer. */
    evt.type                = ALI_EVT_OTA_CMD;
    evt.data.rx_data.p_data = &g_ota_info;
    evt.data.rx_data.length = sizeof(breeze_otainfo_t);
    p_ali->event_handler(p_ali->p_evt_context, &evt);
}

/**@brief Notify received ota evt to higher layer. */
static void notify_ota_event(ali_t *p_ali, uint8_t ota_evt, uint8_t sub_evt)
{
    ali_event_t evt;
    if(ota_evt == ALI_OTA_ON_TX_DONE){
         uint8_t cmd = sub_evt;
         if((cmd != ALI_CMD_FW_CHECK_RESULT) \
			 || (cmd != ALI_CMD_ERROR)\
			 || (cmd != ALI_CMD_FW_BYTES_RECEIVED)){
	     return;
	 }
    }
    g_ota_info.type      =  OTA_EVT;
    g_ota_info.cmd_evt.m_evt.evt =  ota_evt;
    g_ota_info.cmd_evt.m_evt.d   =  sub_evt;
 
    /* send event to higher layer. */
    evt.type                = ALI_EVT_OTA_CMD;
    evt.data.rx_data.p_data = &g_ota_info;
    evt.data.rx_data.length = sizeof(breeze_otainfo_t);
    p_ali->event_handler(p_ali->p_evt_context, &evt);
}

/**@brief Endian swapping. */
static void endian_swap(uint8_t *buff, uint8_t *data, uint16_t len)
{
    VERIFY_PARAM_NOT_NULL_VOID(buff);
    VERIFY_PARAM_NOT_NULL_VOID(data);

    for (uint8_t i = 0; i < len; i++) {
        buff[i] = data[len - i - 1];
    }
}


/**@brief Build manufacturer specific advertising data. */
static void create_manuf_spec_adv_data(ali_t *p_ali, uint32_t model_id,
                                       uint8_t *mac_bin, bool enable_auth,
                                       bool enable_ota, bool secret_per_device)
{
    uint16_t i;
    uint8_t  fmsk = 0;

    // Company ID (CID)
    SET_U16_LE(p_ali->manuf_spec_adv_data, ALI_COMPANY_ID);
    i = sizeof(uint16_t);

    // Protocol ID (PID)
    p_ali->manuf_spec_adv_data[i++] = ALI_PROTOCOL_ID;

    // Function mask (FMSK)
    fmsk = BZ_BLUETOOTH_VER << FMSK_BLUETOOTH_VER_Pos;
    if (enable_auth) {
        fmsk |= 1 << FMSK_SECURITY_Pos;
    }

    if (enable_ota) {
        fmsk |= 1 << FMSK_OTA_Pos;
    }
#ifndef CONFIG_MODEL_SECURITY
    if (secret_per_device) {
        fmsk |= 1 << FMSK_SECRET_TYPE_Pos;
    }
#endif
#ifdef CONFIG_AIS_SECURE_ADV
    fmsk |= 1 << FMSK_SIGNED_ADV_Pos;
#endif
    p_ali->manuf_spec_adv_data[i++] = fmsk;

    // Model ID (MID)
    SET_U32_LE(p_ali->manuf_spec_adv_data + i, model_id);
    i += sizeof(uint32_t);

    for (int j = 5; j >= 0; i++, j--) {
        p_ali->manuf_spec_adv_data[i] = mac_bin[j];
    }
    p_ali->manuf_spec_adv_data_len = i;
}


/**@brief Function to send command by notification. */
static uint32_t tx_func_notify(ali_t *p_ali, uint8_t cmd, uint8_t *p_data,
                               uint16_t length)
{
    return ali_transport_send(&p_ali->transport, ALI_TRANSPORT_TX_TYPE_NOTIFY,
                              cmd, p_data, length);
}


/**@brief Function to send command by indication. */
static uint32_t tx_func_indicate(ali_t *p_ali, uint8_t cmd, uint8_t *p_data,
                                 uint16_t length)
{
    return ali_transport_send(&p_ali->transport, ALI_TRANSPORT_TX_TYPE_INDICATE,
                              cmd, p_data, length);
}


/**@brief Authentication module: event handler function. */
static void auth_event_handler(os_event_t *evt, void *priv)
{
    uint32_t err_code;
    ali_t *p_ali = (ali_t *)priv;
    ali_auth_event_t *p_event= (ali_auth_event_t *)evt->value;

    if (evt->type != OS_EV_AUTH) return;

    switch (evt->code) {
        case OS_EV_CODE_AUTH_DONE:
	    notify_ota_event(p_ali, ALI_OTA_ON_AUTH_EVT, (uint8_t)p_event->data.auth_done.result);
            ali_ext_on_auth(&p_ali->ext, p_event->data.auth_done.result);
            /* Disconnect if authentication failed. */
            if (!p_event->data.auth_done.result &&
                p_ali->conn_handle != BLE_CONN_HANDLE_INVALID) {
                ble_disconnect(AIS_BT_REASON_REMOTE_USER_TERM_CONN);
            } else {
                notify_evt_no_data(p_ali, ALI_EVT_AUTHENTICATED);
            }
            break;

        case OS_EV_CODE_AUTH_KEY_UPDATE:
            err_code = ali_transport_set_key(&p_ali->transport,
                                             p_event->data.new_key.p_sec_key);
            if (err_code != BREEZE_SUCCESS) {
                notify_error(p_ali, ALI_ERROR_SRC_CORE_CANNOT_SET_KEY,
                             err_code);
            }
            break;

        case OS_EV_CODE_AUTH_ERROR:
            if (p_event->data.error.err_code == BREEZE_ERROR_TIMEOUT) {
                ble_disconnect(AIS_BT_REASON_REMOTE_USER_TERM_CONN);
            }
            notify_error(p_ali, p_event->data.error.source,
                         p_event->data.error.err_code);
            break;

        default:
            break;
    }
}


/**@brief Try parsing message and check integrity by the format. */
static bool try_parse(uint8_t *data, uint16_t len)
{
#if CHECK_PAYLOAD_FORMAT // On-site: removed chapter 8 from specification v1.0.4
    uint16_t offset = 0, next_offset;
    uint8_t  attr_len;

    /* Parse message according to Ali-SDK specification v1.0.4 ch. 8.1 */
    do {
        if ((attr_len = data[offset + 2]) == 0) {
            return false; // zero length attribute
        }

        next_offset = offset + 3 + attr_len;
        if (len == next_offset) {
            break; // correct payload
        } else if (len < next_offset) {
            return false;
        } else {
            offset = next_offset;
        }
    } while (1);
#endif
    return true;
}


/**@brief Transport layer: event handler function. */
static void transport_event_handler(os_event_t *evt, void *priv)
{
    bool     send_err = false;
    uint32_t err_code;
    ali_t *p_ali = (ali_t *)priv;
    ali_transport_event_t *p_event= (ali_transport_event_t *)evt->value;
    if (evt->type != OS_EV_TRANS) return;

    switch (evt->code) {
        case OS_EV_CODE_TRANS_TX_DONE:
	    if (!is_valid_tx_command(p_event->data.rxtx.cmd)) {
                send_err = true;
                break;
            }
	    if(p_event->data.rxtx.cmd == ALI_CMD_REPLY || p_event->data.rxtx.cmd == ALI_CMD_STATUS){
	          notify_evt_no_data(p_ali, ALI_EVT_TX_DONE);
	    }
	    notify_ota_event(p_ali, ALI_OTA_ON_TX_DONE, p_event->data.rxtx.cmd);
            break;

        case OS_EV_CODE_TRANS_RX_DONE:
            if (!is_valid_rx_command(p_event->data.rxtx.cmd)) {
                send_err = true;
                break;
            }
	    
	    uint8_t cmd = p_event->data.rxtx.cmd;
	    uint8_t *p_data = p_event->data.rxtx.p_data;
	    uint8_t length = p_event->data.rxtx.length;
	    if(length > OTA_RX_BUFF_LEN){
                notify_error(p_ali, ALI_ERROR_SRC_TRANSPORT_RX_BUFF_SIZE, BREEZE_ERROR_DATA_SIZE);
	    }
	    /*handle 0x00 or 0x02 cmd*/
	    if(length != 0 && (cmd == ALI_CMD_CTRL || cmd == ALI_CMD_QUERY)){
	        if(try_parse(p_data, length)){
                    if(cmd == ALI_CMD_QUERY){
	                notify_query_data(p_ali, p_data, length);
		    } else {
	                notify_ctrl_data(p_ali, p_data, length);
		    }
		} else{
		    notify_error(p_ali,ALI_ERROR_SRC_GAP_CMD_RECEIVED, BREEZE_ERROR_INVALID_DATA);
		}
	    }
            ali_auth_on_command(&p_ali->auth, p_event->data.rxtx.cmd,
                                p_event->data.rxtx.p_data,
                                p_event->data.rxtx.length);
            notify_ota_command(p_ali, p_event->data.rxtx.cmd, \
                           p_event->data.rxtx.num_frames, \
                           p_event->data.rxtx.p_data,\
                           p_event->data.rxtx.length);
            ali_ext_on_command(&p_ali->ext, p_event->data.rxtx.cmd,
                               p_event->data.rxtx.p_data,
                               p_event->data.rxtx.length);
            break;

        case OS_EV_CODE_TRANS_TX_TIMEOUT:
            notify_error(p_ali, ALI_ERROR_SRC_TRANSPORT_TX_TIMER,
                         BREEZE_ERROR_TIMEOUT);
            break;

        case OS_EV_CODE_TRANS_RX_TIMEOUT:
            notify_error(p_ali, ALI_ERROR_SRC_TRANSPORT_RX_TIMER,
                         BREEZE_ERROR_TIMEOUT);
            break;

        case OS_EV_CODE_TRANS_ERROR:
            notify_error(p_ali, p_event->data.error.source,
                         p_event->data.error.err_code);

            /* Trigger resend if error is NOT caused by the module itself. */
            if (p_event->data.error.err_code != BREEZE_ERROR_INTERNAL) {
                send_err = true;

                if (p_event->data.error.source ==
                    ALI_ERROR_SRC_TRANSPORT_FW_DATA_DISC) {
	            notify_ota_event(p_ali, ALI_OTA_ON_DISCONTINUE_ERR, 0);
                }
            }
            break;

        default:
            break;
    }

    if (send_err) {
        // Send error to central
        err_code =
          ali_transport_send(&p_ali->transport, ALI_TRANSPORT_TX_TYPE_NOTIFY,
                             ALI_CMD_ERROR, NULL, 0);
        if (err_code != BREEZE_SUCCESS) {
            notify_error(p_ali, ALI_ERROR_SRC_CORE_SEND_ERR, err_code);
            return;
        }
    }
}


/**@brief Extend module: event handler function. */
static void ext_event_handler(os_event_t *evt, void *priv)
{
    ali_t *p_ali = (ali_t *)priv;
    ali_ext_event_t *p_event= (ali_ext_event_t *)evt->value;
    if (evt->type != OS_EV_EXT) return;
    switch (evt->type) {
        case OS_EV_CODE_EXT_ERROR:
            notify_error(p_ali, p_event->data.error.source,
                         p_event->data.error.err_code);
            break;

        case OS_EV_CODE_EXT_APIINFO:
            notify_apinfo(p_ali, p_event->data.rx_data.p_data,
                          p_event->data.rx_data.length);
            break;

        default:
            break;
    }
}

static void ble_ais_event_handler(ali_t *p_ali, ble_ais_event_t *p_event)
{
    switch (p_event->type) {
        case BLE_AIS_EVT_RX_DATA:
            ali_transport_on_rx_data(&p_ali->transport,
                                     p_event->data.rx_data.p_data,
                                     p_event->data.rx_data.length);
            break;

        case BLE_AIS_EVT_TX_DONE:
            ali_transport_on_tx_complete(&p_ali->transport,
                                         p_event->data.tx_done.pkt_sent);
            ali_auth_on_tx_complete(&p_ali->auth);
            break;

        case BLE_AIS_EVT_SVC_ENABLED:
            ali_auth_on_enable_service(&p_ali->auth);
            if (!p_ali->is_auth_enabled) {
		notify_ota_event(p_ali, ALI_OTA_ON_AUTH_EVT, true);
            }
            break;

        default:
            break;
    }
}


/*@brief Function for initializing ble_ais module. */
static uint32_t ais_init(ali_t *p_ali, ali_init_t const *p_init)
{
    ble_ais_init_t init_ais;

    g_ali = p_ali;

    memset(&init_ais, 0, sizeof(ble_ais_init_t));
    init_ais.event_handler = (ble_ais_event_handler_t)ble_ais_event_handler;
    init_ais.p_context     = p_ali;
    init_ais.mtu           = p_init->max_mtu;
    return ble_ais_init(&p_ali->ais, &init_ais);
}


/*@brief Function for initializing ali_transport module, the transport layer. */
static uint32_t transport_init(ali_t *p_ali, ali_init_t const *p_init)
{
    ali_transport_init_t init_transport;

    memset(&init_transport, 0, sizeof(ali_transport_init_t));
    init_transport.tx_buffer     = p_ali->tx_buff;
    init_transport.tx_buffer_len = TX_BUFF_LEN;
    init_transport.rx_buffer     = (uint8_t *)p_ali->rx_buff;
    init_transport.rx_buffer_len = RX_BUFF_LEN;
    init_transport.timeout       = p_init->transport_timeout;
    os_register_event_filter(OS_EV_TRANS, transport_event_handler, p_ali);
    init_transport.p_evt_context = p_ali;
    init_transport.tx_func_notify =
      (ali_transport_tx_func_t)ble_ais_send_notification;
    init_transport.tx_func_indicate =
      (ali_transport_tx_func_t)ble_ais_send_indication;

    init_transport.p_tx_func_context = &p_ali->ais;

    return ali_transport_init(&p_ali->transport, &init_transport);
}


/*@brief Function for initializing ali_auth, the authentication module. */
static uint32_t auth_init(ali_t *p_ali, ali_init_t const *p_init, uint8_t *mac)
{
    ali_auth_init_t init_auth;

    memset(&init_auth, 0, sizeof(ali_auth_init_t));
    init_auth.feature_enable     = p_init->enable_auth;
    init_auth.timeout            = BZ_AUTH_TIMEOUT;
    os_register_event_filter(OS_EV_AUTH, auth_event_handler, p_ali);
    init_auth.p_evt_context      = p_ali;
    init_auth.tx_func            = (ali_auth_tx_func_t)tx_func_indicate;
    init_auth.p_tx_func_context  = p_ali;
    init_auth.p_mac              = mac;
    init_auth.p_secret           = p_init->secret.p_data;
    init_auth.secret_len         = p_init->secret.length;
    init_auth.p_product_secret   = p_init->product_secret.p_data;
    init_auth.product_secret_len = p_init->product_secret.length;
    init_auth.p_dkey             = p_init->device_key.p_data;
    init_auth.dkey_len           = p_init->device_key.length;
    init_auth.p_pkey             = p_init->product_key.p_data;
    init_auth.pkey_len           = p_init->product_key.length;
    init_auth.model_id           = p_init->model_id;

    return ali_auth_init(&p_ali->auth, &init_auth);
}


/*@brief Function for initializing ali_ext, the extend module. */
static uint32_t ext_init(ali_t *p_ali, ali_init_t const *p_init)
{
    ali_ext_init_t init_ext;

    memset(&init_ext, 0, sizeof(ali_ext_init_t));
    init_ext.event_handler     = (ali_ext_event_handler_t)ext_event_handler;
    init_ext.p_evt_context     = p_ali;
    init_ext.tx_func           = (ali_ext_tx_func_t)tx_func_indicate;
    init_ext.p_tx_func_context = p_ali;

    init_ext.p_fw_version   = p_init->sw_ver.p_data;
    init_ext.fw_version_len = p_init->sw_ver.length;
    init_ext.model_id       = p_init->model_id;

    ali_auth_get_device_name(&p_ali->auth, &init_ext.p_device_name,
                             &init_ext.device_name_len);
    ali_auth_get_product_key(&p_ali->auth, &init_ext.p_product_key,
                             &init_ext.product_key_len);
    ali_auth_get_secret(&p_ali->auth, &init_ext.p_secret, &init_ext.secret_len);

    // ali_auth_get_v2_signature(&p_ali->auth, &init_ext.p_v2_signature,
    // &init_ext.v2_signature_len);

    return ali_ext_init(&p_ali->ext, &init_ext);
}


/**@brief Function for verifying initialization parameters. */
static ret_code_t verify_init_params(ali_init_t const *p_init)
{
    VERIFY_PARAM_NOT_NULL(p_init);
    VERIFY_PARAM_NOT_NULL(p_init->event_handler);

    if (p_init->context_size < sizeof(ali_t)) {
        return BREEZE_ERROR_DATA_SIZE;
    }

    if (p_init->enable_auth) {
        if (p_init->mac.length != MAC_ASCII_LEN) {
            return BREEZE_ERROR_INVALID_LENGTH;
        }

        if (p_init->product_key.length == 0 && p_init->device_key.length == 0) {
            return BREEZE_ERROR_INVALID_LENGTH; // no product or device key
                                             // available.
        }

        if (p_init->product_key.length != 0) {
            VERIFY_PARAM_NOT_NULL(p_init->product_key.p_data);
        }

        if (p_init->device_key.length != 0) {
            VERIFY_PARAM_NOT_NULL(p_init->device_key.p_data);
            if (p_init->device_key.length > ALI_AUTH_DKEY_LEN_MAX) {
                return BREEZE_ERROR_INVALID_LENGTH;
            }
        }
    }

    return BREEZE_SUCCESS;
}


#ifdef CONFIG_AIS_SECURE_ADV
static void update_seq(void *arg)
{
    os_kv_set(AIS_SEQ_KV_KEY, &g_seq, sizeof(g_seq), 1);
    os_post_delayed_action(AIS_SEQ_UPDATE_FREQ, update_seq, NULL);
}


static void init_seq_number(uint32_t *seq)
{
    int len = sizeof(uint32_t);

    if (!seq)
        return;

    if (os_kv_get(AIS_SEQ_KV_KEY, seq, &len) != 0) {
        *seq = 0;
        len  = sizeof(uint32_t);
        os_kv_set(AIS_SEQ_KV_KEY, seq, len, 1);
    }

    os_post_delayed_action(AIS_SEQ_UPDATE_FREQ, update_seq, NULL);
}
#endif

ret_code_t ali_init(void *p_ali_ext, ali_init_t const *p_init)
{
    ali_t   *p_ali = (ali_t *)p_ali_ext;
    uint8_t  mac_be[ALI_AUTH_MAC_LEN];
    uint32_t err_code;
    bool     secret_per_device;
    uint8_t  bmac[AIS_BT_MAC_LEN] = { 0 };
    uint32_t size;

    ais_adv_init_t adv_data = {
        .flag = AIS_AD_GENERAL | AIS_AD_NO_BREDR,
        .name = { .ntype = AIS_ADV_NAME_FULL, .name = "AZ" },
    };

    /* check parameters */
    VERIFY_PARAM_NOT_NULL(p_ali);
    err_code = verify_init_params(p_init);
    VERIFY_SUCCESS(err_code);

    /* Check if 4-byte aligned. */
    if (((uint32_t)p_ali & 0x3) != 0) {
        return BREEZE_ERROR_INVALID_ADDR;
    }

    /* Initialize context */
    memset(p_ali, 0, sizeof(ali_t));
    p_ali->is_auth_enabled  = p_init->enable_auth;
    p_ali->event_handler    = p_init->event_handler;
    p_ali->p_evt_context    = p_init->p_evt_context;
    p_ali->is_authenticated = false;
    p_ali->conn_handle      = BLE_CONN_HANDLE_INVALID;

    /* check if V1 or V2 network. */
    if (p_init->product_key.length == ALI_AUTH_PKEY_V2_LEN &&
        p_init->device_key.length != 0 &&
        (p_init->secret.length == ALI_AUTH_SECRET_V2P_LEN ||
         p_init->secret.length == ALI_AUTH_SECRET_V2D_LEN)) {
        /* V2 Network. */
        secret_per_device = (p_init->secret.length == ALI_AUTH_SECRET_V2D_LEN);
    } else {
        /* V1 Network. */
        secret_per_device = (p_init->device_key.length != 0);
    }

#ifdef CONFIG_AIS_SECURE_ADV
    init_seq_number(&g_seq);
#endif

    /* Initialize Alibaba Information Service (AIS). */
    err_code = ais_init(p_ali, p_init);
    VERIFY_SUCCESS(err_code);

    ble_get_mac(bmac);

    endian_swap(mac_be, bmac, ALI_AUTH_MAC_LEN);

    /* Initialize transport layer. */
    err_code = transport_init(p_ali, p_init);
    VERIFY_SUCCESS(err_code);

    /* Initialize Authentication module. */
    err_code = auth_init(p_ali, p_init, mac_be);
    VERIFY_SUCCESS(err_code);

    /* Initialize extend module. */
    err_code = ext_init(p_ali, p_init);
    VERIFY_SUCCESS(err_code);

    // Dervive manufacturer-specific advertising data.
    create_manuf_spec_adv_data(p_ali, p_init->model_id, mac_be,
                               p_init->enable_auth, p_init->enable_ota,
                               secret_per_device);

    adv_data.vdata.len = sizeof(adv_data.vdata.data);
    err_code           = ali_get_manuf_spec_adv_data(p_ali, adv_data.vdata.data,
                                           &(adv_data.vdata.len));
    if (err_code) {
        printf("%s %d fail.\r\n", __func__, __LINE__);
        return AIS_ERR_INVALID_ADV_DATA;
    }

    /* append user adv data if any. */
    if (p_init->user_adv_len > 0) {
        size = sizeof(adv_data.vdata.data) - adv_data.vdata.len;
        if (size < p_init->user_adv_len) {
            printf("Warning: no space for user adv data (expected %d but"
                   " only %d left). No user adv data set!!!\r\n",
                   p_init->user_adv_len, size);
        } else {
            memcpy(adv_data.vdata.data + adv_data.vdata.len,
                   p_init->user_adv_data, p_init->user_adv_len);
            adv_data.vdata.len += p_init->user_adv_len;
        }
    }

    p_ali->is_initialized = true;

    ble_advertising_start(&adv_data);

    return BREEZE_SUCCESS;
}


void ali_reset(void *p_ali_ext)
{
    ali_t *p_ali = (ali_t *)p_ali_ext;

    /* Check parameters */
    VERIFY_PARAM_NOT_NULL_VOID(p_ali);

    /* Check if 4-byte aligned. */
    if (((uint32_t)p_ali & 0x3) != 0) {
        return;
    }

    /* Check if module is initialized. */
    VERIFY_MODULE_INITIALIZED_VOID();

    /* Reset variables. */
    p_ali->is_authenticated = false;

    ali_ext_reset(&p_ali->ext);
    ali_auth_reset(&p_ali->auth);
    ali_transport_reset(&p_ali->transport);
    ble_ais_set_auth(&p_ali->ais, false);
}


ret_code_t ali_send_notify(void *p_ali_ext, uint8_t cmd, uint8_t *p_data, uint16_t length)
{
    ret_code_t ret = BREEZE_SUCCESS;
    ali_t *p_ali = (ali_t *)p_ali_ext;

    /* Check parameters */
    VERIFY_PARAM_NOT_NULL(p_ali);
    VERIFY_PARAM_NOT_NULL(p_data);

    /* Check if 4-byte aligned. */
    if (((uint32_t)p_ali & 0x3) != 0) {
        return BREEZE_ERROR_INVALID_ADDR;
    }

    /* Check if module is initialized. */
    VERIFY_MODULE_INITIALIZED();

    if (length == 0 || length > ALI_TRANSPORT_MAX_TX_DATA_LEN) {
        return BREEZE_ERROR_DATA_SIZE;
    }

    if(cmd == 0){
	/*if default, send ALI_CMD_STATUS*/
	cmd = ALI_CMD_STATUS;
	ret = ali_transport_send(&p_ali->transport, ALI_TRANSPORT_TX_TYPE_NOTIFY,
                              cmd, p_data, length);
    }else{
        ret = ali_transport_send(&p_ali->transport, ALI_TRANSPORT_TX_TYPE_NOTIFY,
                              cmd, p_data, length);
    }
}


ret_code_t ali_send_indicate(void *p_ali_ext, uint8_t cmd, uint8_t *p_data, uint16_t length)
{
    ret_code_t ret = BREEZE_SUCCESS;
    ali_t *p_ali = (ali_t *)p_ali_ext;

    /* Check parameters */
    VERIFY_PARAM_NOT_NULL(p_ali);
    VERIFY_PARAM_NOT_NULL(p_data);

    /* Check if 4-byte aligned. */
    if (((uint32_t)p_ali & 0x3) != 0) {
        return BREEZE_ERROR_INVALID_ADDR;
    }

    if (length == 0 || length > ALI_TRANSPORT_MAX_TX_DATA_LEN) {
        return BREEZE_ERROR_DATA_SIZE;
    }

    if(cmd == 0){
        /*if default, send ALI_CMD_STATUS*/
	cmd = ALI_CMD_STATUS;
	ret = ali_transport_send(&p_ali->transport, ALI_TRANSPORT_TX_TYPE_INDICATE,
                              cmd, p_data, length);
    } else{
        ret = ali_transport_send(&p_ali->transport, ALI_TRANSPORT_TX_TYPE_INDICATE,
                              cmd, p_data, length);
    }
    return ret;
}


#ifdef CONFIG_AIS_SECURE_ADV
/**
 * Change the logic below if want to fetch sequence from cloud.
 * For now, we save/update it locally.
 */
static int get_next_seq(uint32_t *seq)
{
    if (!seq)
        return -1;
    *seq = g_seq++;
    return 0;
}

static void make_seq_le(uint32_t *seq)
{
    uint32_t test_num = 0x01020304;
    uint8_t *byte     = (uint8_t *)(&test_num), tmp;

    if (*byte == 0x04)
        return; /* already le */

#ifdef AIS_DEBUG
    printf("Big endian system, let's convert seq to little endian.\r\n");
    printf("Before conversion, the seq is %08x\r\n", *seq);
#endif

    byte    = (uint8_t *)seq;
    tmp     = byte[0];
    byte[0] = byte[3];
    byte[3] = tmp;
    tmp     = byte[1];
    byte[1] = byte[2];
    byte[3] = tmp;

#ifdef AIS_DEBUG
    AIS_DEBUG("After conversion, the seq is %08x\r\n", *seq);
#endif
}

static void printf_str_in_hex_format(const char *s, uint32_t len)
{
    uint8_t *p, i;

    if (!s)
        return;
    else
        p = (uint8_t *)s;

    for (i = 0; i < len; i++)
        printf("%02x ", p[i]);
}

#define DEVICE_NAME_STR   "deviceName"
#define DEVICE_SECRET_STR "deviceSecret"
#define PRODUCT_KEY_STR   "productKey"
#define SEQUENCE_STR      "sequence"
static int calc_sign(ali_t *p_ali, uint32_t seq, uint8_t *sign)
{
    SHA256_CTX context;
    uint8_t    full_sign[32], i, *p;

    if (!p_ali || !sign)
        return -1;

    make_seq_le(&seq);

#ifdef AIS_DEBUG
    printf("sha256 input (in hex): ");
    printf_str_in_hex_format(DEVICE_NAME_STR, strlen(DEVICE_NAME_STR));
    printf_str_in_hex_format(p_ali->auth.v2_network.device_name,
                             p_ali->auth.v2_network.device_name_len);
    printf_str_in_hex_format(DEVICE_SECRET_STR, strlen(DEVICE_SECRET_STR));
    printf_str_in_hex_format(p_ali->auth.v2_network.secret,
                             ALI_AUTH_V2_SECRET_LEN);
    printf_str_in_hex_format(PRODUCT_KEY_STR, strlen(PRODUCT_KEY_STR));
    printf_str_in_hex_format(p_ali->auth.v2_network.product_key,
                             ALI_AUTH_PKEY_V2_LEN);
    printf_str_in_hex_format(SEQUENCE_STR, strlen(SEQUENCE_STR));
    p = (uint8_t *)&seq;
    printf("%02x %02x %02x %02x\r\n", p[0], p[1], p[2], p[3]);
#endif

    sha256_init(&context);

    sha256_update(&context, DEVICE_NAME_STR, strlen(DEVICE_NAME_STR));
    sha256_update(&context, p_ali->auth.v2_network.device_name,
                  p_ali->auth.v2_network.device_name_len);

    sha256_update(&context, DEVICE_SECRET_STR, strlen(DEVICE_SECRET_STR));
    sha256_update(&context, p_ali->auth.v2_network.secret,
                  ALI_AUTH_V2_SECRET_LEN);

    sha256_update(&context, PRODUCT_KEY_STR, strlen(PRODUCT_KEY_STR));
    sha256_update(&context, p_ali->auth.v2_network.product_key,
                  ALI_AUTH_PKEY_V2_LEN);

    sha256_update(&context, SEQUENCE_STR, strlen(SEQUENCE_STR));
    sha256_update(&context, &seq, sizeof(seq));

    sha256_final(&context, full_sign);

#ifdef AIS_DEBUG
    printf("After sha256, the output (in hex): ");
    for (i = 0; i < 32; i++)
        printf("%02x ", full_sign[i]);
    printf("\r\n");
#endif

    memcpy(sign, full_sign, 4);

    return 0;
}
#endif


ret_code_t ali_get_manuf_spec_adv_data(void *p_ali_ext, uint8_t *p_data,
                                       uint16_t *length)
{
    ali_t *p_ali;

    if (p_ali_ext == NULL)
        p_ali = g_ali;
    else
        p_ali = (ali_t *)p_ali_ext;

    /* Check parameters */
    VERIFY_PARAM_NOT_NULL(p_ali);
    VERIFY_PARAM_NOT_NULL(p_data);
    VERIFY_PARAM_NOT_NULL(length);

    /* Check if 4-byte aligned. */
    if (((uint32_t)p_ali & 0x3) != 0) {
        return BREEZE_ERROR_INVALID_ADDR;
    }

#ifdef CONFIG_AIS_SECURE_ADV
    if (*length < (p_ali->manuf_spec_adv_data_len + 4 + 4)) {
#else
    if (*length < p_ali->manuf_spec_adv_data_len) {
#endif
        return BREEZE_ERROR_NO_MEM;
    }

#ifdef CONFIG_AIS_SECURE_ADV
    uint8_t  sign[4];
    uint32_t seq;

    if (get_next_seq(&seq) != 0) {
        return BREEZE_ERROR_GET_SEQ_FAIL;
    }

    if (calc_sign(p_ali, seq, sign) != 0) {
        return BREEZE_ERROR_GET_SIGN_FAIL;
    }

    memcpy(p_data, p_ali->manuf_spec_adv_data, p_ali->manuf_spec_adv_data_len);
    memcpy(p_data + p_ali->manuf_spec_adv_data_len, sign, 4);
    memcpy(p_data + p_ali->manuf_spec_adv_data_len + 4, &seq, 4);
    *length = p_ali->manuf_spec_adv_data_len + 4 + 4;
#else
    memcpy(p_data, p_ali->manuf_spec_adv_data, p_ali->manuf_spec_adv_data_len);
    *length = p_ali->manuf_spec_adv_data_len;
#endif

    return BREEZE_SUCCESS;
}

#ifdef CONFIG_AIS_SECURE_ADV
void set_adv_sequence(uint32_t seq)
{
    g_seq = seq;
    os_kv_set(AIS_SEQ_KV_KEY, &g_seq, sizeof(g_seq), 1);
}
#endif
