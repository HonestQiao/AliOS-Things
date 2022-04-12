/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#ifndef _AWSS_H_
#define _AWSS_H_

#include <stdint.h>

#if defined(__cplusplus)  /* If this is a C++ compiler, use C linkage */
extern "C"
{
#endif

/**
 * @brief   start wifi setup service
 *
 * @retval  -1 : wifi setup fail
 * @retval  0 : sucess
 * @note: awss_report_cloud must been called to enable wifi setup service
 */
int awss_start(void);

/**
 * @brief   stop wifi setup service
 *
 * @retval  -1 : failure
 * @retval  0 : sucess
 * @note
 *      if awss_stop is called before exit of awss_start, awss and notify will stop.
 */
int awss_stop(void);

/**
 * @brief   make sure user touches device belong to themselves
 *
 * @retval  -1 : failure
 * @retval  0 : sucess
 * @note: AWSS doesn't parse awss packet until user touches deivce use this api.
 */
int awss_config_press(void);

/**
 * @brief   get the awss config press status in realtime.
 *
 * @retval  1 : user has touched device
 * @retval  0 : user don't touch device
 */
uint8_t awss_get_config_press(void);
void awss_set_config_press(uint8_t press);

/**
 * @brief   report token to cloud after wifi setup success
 *
 * @retval  -1 : failure
 * @retval  0 : sucess
 */
int awss_report_cloud(void);

enum awss_event_t {
    AWSS_START = 0x1000,       // AWSS start without enbale, just supports device discover
    AWSS_ENABLE,               // AWSS enable
    AWSS_LOCK_CHAN,            // AWSS lock channel(Got AWSS sync packet)
    AWSS_CS_ERR,               // AWSS AWSS checksum is error
    AWSS_PASSWD_ERR,           // AWSS decrypt passwd error
    AWSS_GOT_SSID_PASSWD,      // AWSS parse ssid and passwd successfully
    AWSS_CONNECT_ADHA,         // AWSS try to connnect adha (device discover, router solution)
    AWSS_CONNECT_ADHA_FAIL,    // AWSS fails to connect adha
    AWSS_CONNECT_AHA,          // AWSS try to connect aha (AP solution)
    AWSS_CONNECT_AHA_FAIL,     // AWSS fails to connect aha
    AWSS_SETUP_NOTIFY,         // AWSS sends out device setup information (AP and router solution)
    AWSS_CONNECT_ROUTER,       // AWSS try to connect destination router
    AWSS_CONNECT_ROUTER_FAIL,  // AWSS fails to connect destination router.
    AWSS_GOT_IP,               // AWSS connects destination successfully and got ip address
    AWSS_SUC_NOTIFY,           // AWSS sends out success notify (AWSS sucess)
    AWSS_BIND_NOTIFY,          // AWSS sends out bind notify information to support bind between user and device
    AWSS_RESET = 0x3000,       // Linkkit reset success (just got reset response from cloud without any other operation)
};

#if defined(__cplusplus)  /* If this is a C++ compiler, use C linkage */
}
#endif

#endif
