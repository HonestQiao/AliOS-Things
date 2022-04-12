/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <breeze_export.h>

extern int awss_success_notify();

static void dev_status_changed_handler(breeze_event_t event)
{
    return;
}

static void set_dev_status_handler(uint8_t *buffer, uint32_t length)
{
    return;
}

static void get_dev_status_handler(uint8_t *buffer, uint32_t length)
{
    return;
}

static void breeze_awss_init_helper
(
    struct device_config *init,
    apinfo_ready_cb cb,
    breeze_dev_info_t *dinfo
)
{
    memset(init, 0, sizeof(struct device_config));
    init->status_changed_cb = dev_status_changed_handler;
    init->set_cb            = set_dev_status_handler;
    init->get_cb            = get_dev_status_handler;
    init->apinfo_cb         = cb;

    init->enable_auth = true;
    init->product_id = dinfo->product_id;

    init->product_key_len = strlen(dinfo->product_key);
    memcpy(init->product_key, dinfo->product_key, init->product_key_len);

    init->product_secret_len = strlen(dinfo->product_secret);
    memcpy(init->product_secret, dinfo->product_secret, init->product_secret_len);

    /* device key may be NULL */
    if (dinfo->device_name != NULL) {
        init->device_key_len = strlen(dinfo->device_name);
        memcpy(init->device_key, dinfo->device_name, init->device_key_len);
    } else {
        init->device_key_len = 0;
    }

    /* device secret may be NULL */
    if (dinfo->device_secret != NULL) {
        init->secret_len = strlen(dinfo->device_secret);
        memcpy(init->secret, dinfo->device_secret, init->secret_len);
    } else {
        init->secret_len = 0;
    }

    memcpy(init->version, "1.0.0", strlen("1.0.0"));
}

void breeze_awss_init
(
    apinfo_ready_cb cb,
    breeze_dev_info_t *info
)
{
    int ret;
    struct device_config brzinit;

    breeze_awss_init_helper(&brzinit, cb, info);

    ret = breeze_start(&brzinit);
    if (ret != 0) {
        printf("breeze_start failed (ret = %d).\r\n", ret);
    } else {
        printf("breeze_start succeed.\r\n");
    }
}

void breeze_awss_start()
{
    breeze_event_dispatcher();
}
