/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#ifndef ALI_TRANSPORT_H__
#define ALI_TRANSPORT_H__

#include <stdint.h>
#include "common.h"
#include "breeze_hal_os.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define ALI_TRANSPORT_MAX_TX_DATA_LEN                                          \
    1024 /**< Maximum length of data (in bytes) that can be transmitted to the \
            \ \ peer. */
#define ALI_TRANSPORT_MAX_RX_DATA_LEN                                         \
    1024 /**< Maximum length of data (in bytes) that can be received from the \
            \ \ peer. */
#define ALI_TRANSPORT_VERSION                                                 \
    0 /**< Packet header version, conveyed in the version field of the packet \
         \ \ header. */

    /**
     * @brief Types of Tx.
     */
    typedef enum
    {
        ALI_TRANSPORT_TX_TYPE_NOTIFY,   /**< Use notification for Tx. */
        ALI_TRANSPORT_TX_TYPE_INDICATE, /**< Use indication for Tx. */
    } ali_transport_tx_type_t;


    /**@brief Structure for Tx/Rx completion event.
     * @note  Includes @ref ALI_TRANSPORT_EVT_TX_DONE and @ref
     * ALI_TRANSPORT_EVT_RX_DONE.
     */
    typedef struct
    {
        uint8_t *p_data;     /**< Pointer to memory used for transfer. */
        uint16_t length;     /**< Number of bytes transfered. */
        uint8_t  cmd;        /**< Command. */
        uint8_t  num_frames; /**< Number of frames involved in Tx / Rx. */
    } ali_transport_xfer_evt_t;


    /**@brief Structure for transport layer error event. */
    typedef struct
    {
        uint32_t source;   /**< The location which caused the error. */
        uint32_t err_code; /**< Error code which has been raised. */
    } ali_transport_error_evt_t;


    /**@brief Structure for transport layer event. */
    typedef struct
    {
        union
        {
            ali_transport_xfer_evt_t
              rxtx; /**< Data provided for transfer completion events. */
            ali_transport_error_evt_t
              error; /**< Data provided for error event. */
        } data;
    } ali_transport_event_t;


    // Forward declaration of the ali_transport_t type.
    typedef struct ali_transport_s ali_transport_t;


    /**
     * @brief Transport layer Tx function.
     *
     * @param[in] p_context   Context passed to interrupt handler, set on
     * initialization.
     * @param[in] p_data      Pointer to Tx data.
     * @param[in] length      Length of Tx data.
     */
    typedef uint32_t (*ali_transport_tx_func_t)(void    *p_context,
                                                uint8_t *p_data,
                                                uint16_t length);

    /**
     * @brief Transport layer event handler.
     *
     * @param[in] p_context   Context passed to interrupt handler, set on
     * initialization.
     * @param[in] p_event     Pointer to event structure. Event is allocated on
     * the stack so it is available only within the context of the event
     * handler.
     */
    typedef void (*ali_transport_event_handler_t)(
      void *p_context, ali_transport_event_t *p_event);


    /**@brief Structure for transport layer configuration. */
    typedef struct
    {
        uint8_t *tx_buffer;     /**< Tx buffer provided by the application. */
        uint16_t tx_buffer_len; /**< Size of Tx buffer provided. */
        uint8_t *rx_buffer;     /**< Rx buffer provided by the application. */
        uint16_t rx_buffer_len; /**< Size of Rx buffer provided. */
        uint32_t timeout;       /**< Timeout of Tx/Rx, in number of ticks. */
        ali_transport_event_handler_t
              event_handler; /**< Pointer to event handler. */
        void *p_evt_context; /**< Pointer to context which will be passed as a
                                parameter of event_handler. */
        ali_transport_tx_func_t
          tx_func_notify; /**< Pointer to Tx function (notify). */
        ali_transport_tx_func_t
              tx_func_indicate;  /**< Pointer to Tx function (indicate). */
        void *p_tx_func_context; /**< Pointer to context which will be passed as
                                    a parameter of tx_func. */
        uint8_t *p_key;          /**< Key for AES-128 encryption. */
    } ali_transport_init_t;

    typedef struct ecb_hal_data_s
    {
        uint8_t key[16];
        uint8_t data[256];
    } ecb_hal_data_t;

    /**@brief Transport layer structure. This contains various status
     * information for the layer. */
    struct ali_transport_s
    {
        struct
        {
            uint8_t *buff;       /**< Tx buffer. */
            uint16_t buff_size;  /**< Size of Tx buffer. */
            uint8_t *data;       /**< Pointer to tx data. */
            uint16_t len;        /**< Size of Tx data. */
            uint16_t bytes_sent; /**< Number of bytes sent. */
            uint8_t  encrypted;  /**< Flag telling whether encryption is active
                                    (chapter 5.2.1) */
            uint8_t    msg_id;   /**< Tx: Message ID (chapter 5.2.1). */
            uint8_t    cmd;      /**< Tx: Command (chapter 5.2.1). */
            uint8_t    total_frame; /**< Tx: Total frame (chapter 5.2.1). */
            uint8_t    frame_seq;   /**< Tx: Frame sequence (chapter 5.2.1). */
            uint8_t    zeroes_padded; /**< Tx: Number of zeroes padded. */
            uint16_t   pkt_req; /**< Number of packets requested for Tx. */
            uint16_t   pkt_cfm; /**< Number of packets confirmed for Tx. */
            os_timer_t timer;   /**< Timer for Tx timeout. */
            void *p_context; /**< Pointer to context which will be passed as a
                                parameter of tx_func. */
            ali_transport_tx_func_t
              active_func; /**< Pointer to the active Tx function. */
            ali_transport_tx_func_t
              notify_func; /**< Pointer to Tx function (notify). */
            ali_transport_tx_func_t
                           indicate_func; /**< Pointer to Tx function (indicate). */
            ecb_hal_data_t ecb_context; /**< ECB context from softdevice. */
        } tx;
        struct
        {
            uint8_t   *buff;           /**< Rx buffer. */
            uint16_t   buff_size;      /**< Size of Rx buffer. */
            uint16_t   bytes_received; /**< Number of bytes received. */
            uint8_t    msg_id;         /**< Rx: Message ID (chapter 5.2.1). */
            uint8_t    cmd;            /**< Rx: Command (chapter 5.2.1). */
            uint8_t    total_frame;    /**< Rx: Total frame (chapter 5.2.1). */
            uint8_t    frame_seq; /**< Rx: Frame sequence (chapter 5.2.1). */
            os_timer_t timer;     /**< Timer for Rx timeout. */
        } rx;
        uint16_t max_pkt_size; /**< MTU - 3 */
        ali_transport_event_handler_t
              event_handler; /**< Pointer to event handler. */
        void *p_evt_context; /**< Pointer to context which will be passed as a
                                parameter of event_handler. */
        void *p_key;         /**< Pointer to encryption key. */
        uint32_t
              timeout; /**< Timeout of Tx/Rx state machine, in number of ticks. */
        void *p_aes_ctx; /**< AES-128 context, e.g. key, iv, etc. */
    };


    /**
     * @brief Function for initializing the Transport layer.
     *
     * This function configures and enables the transport layer. Buffers should
     * be provided by the application and assigned in the @ref
     * ali_transport_init_t structure.
     *
     * @param[in] p_transport   Transport layer structure.
     * @param[in] p_init        Initial configuration. Default configuration
     * used if NULL.
     *
     * @retval    BREEZE_SUCCESS             If initialization was successful.
     * @retval    BREEZE_ERROR_INVALID_PARAM If invalid parameters have been
     * provided.
     * @retval    BREEZE_ERROR_NULL          If NULL pointers are provided.
     */
    ret_code_t ali_transport_init(ali_transport_t            *p_transport,
                                  ali_transport_init_t const *p_init);


    /**
     * @brief Function for resetting the state machine of transport layer.
     *
     * @param[in] p_transport Transport layer structure.
     */
    void ali_transport_reset(ali_transport_t *p_transport);


    /**
     * @brief Function for sending data over Bluetooth LE.
     *
     * @param[in] p_transport Transport layer structure.
     * @param[in] tx_type     Type of Tx.
     * @param[in] cmd         Command.
     * @param[in] p_data      Pointer to data.
     * @param[in] length      Number of bytes to send.
     *
     * @retval    BREEZE_SUCCESS            If data has been queued successfully.
     * @retval    BREEZE_ERROR_BUSY         If Tx is on-going.
     * @retval    BREEZE_ERROR_NULL         If NULL pointer has been provided.
     */
    ret_code_t ali_transport_send(ali_transport_t        *p_transport,
                                  ali_transport_tx_type_t tx_type, uint8_t cmd,
                                  uint8_t const *const p_data, uint16_t length);


    /**@brief Function for handling the Rx data from lower layers.
     *
     * @param[in]   p_transport Transport layer structure.
     * @param[in]   p_data      Pointer to data.
     * @param[in]   length      Length of data.
     */
    void ali_transport_on_rx_data(ali_transport_t *p_transport, uint8_t *p_data,
                                  uint16_t length);


    /**@brief Function for handling the event @ref BLE_EVT_TX_COMPLETE from BLE
     * Stack.
     *
     * @param[in]   p_transport Transport layer structure.
     * @param[in]   pkt_sent    Number of packets sent.
     */
    void ali_transport_on_tx_complete(ali_transport_t *p_transport,
                                      uint16_t         pkt_sent);

    /**@brief Function for setting encryption key.
     *
     * @param[in]   p_transport Transport layer structure.
     * @param[in]   p_key       Pointer to 16-byte key..
     *
     * @retval    BREEZE_SUCCESS               If data has been queued
     * successfully.
     * @retval    BREEZE_ERROR_NULL            If NULL pointer is provided.
     */
    uint32_t ali_transport_set_key(ali_transport_t *p_transport,
                                   uint8_t         *p_key);


#ifdef __cplusplus
}
#endif

#endif // ALI_TRANSPORT_H__
