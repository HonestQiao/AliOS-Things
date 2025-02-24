/*
 * Copyright (C) 2015-2019 Alibaba Group Holding Limited
 */

#include "aiot_dm_api.h"
#include "aiot_mqtt_api.h"
#include "aiot_state_api.h"
#include "aiot_sysdep_api.h"
#ifdef AOS_COMP_KV
#include "aos/kv.h"
#endif
#include "module_aiot.h"
#include "py/mperrno.h"
#include "py/mpstate.h"
#include "py/runtime.h"
#include "py_defines.h"
#include "py/mpthread.h"

#define MOD_STR       "AIOT_DEVICE"

#define OTA_MODE_NAME "system"
static bool linksdk_device_is_inited = false;
static bool linksdk_device_is_used = false;
// ota_service_t pyamp_g_aiot_device_appota_service;
// static ota_store_module_info_t module_info[3];

// extern const char *pyamp_jsapp_version_get(void);

const mp_obj_type_t linksdk_device_type;

typedef enum {
    ON_CONNECT = 1,
    ON_CLOSE = 2,
    ON_DISCONNECT = 3,
    ON_EVENT = 4,
    ON_SERVICE = 5,
    ON_SUBCRIBE = 6,
    ON_PROPS = 7,
    ON_ERROR = 8,
    ON_REPLY = 9,
    ON_RAWDATA = 10,
    ON_REGISTER = 11,
    ON_CB_MAX = 12,
} lk_cb_func_t;

typedef struct {
    iot_device_handle_t *iot_device_handle;
    char *topic;
    char *payload;
    char *service_id;
    char *params;
    int py_cb_ref;
    int ret_code;
    int topic_len;
    int payload_len;
    int params_len;
    int dm_recv;
    uint64_t msg_id;
    aiot_mqtt_option_t option;
    aiot_mqtt_event_type_t event_type;
    aiot_mqtt_recv_type_t recv_type;
    aiot_dm_recv_type_t dm_recv_type;
    aiot_dev_jscallback_type_t cb_type;

} device_notify_param_t;

typedef struct {
    mp_obj_base_t base;
    iot_device_handle_t *iot_device_handle;
} linksdk_device_obj_t;

static char *__amp_rawdup(char *raw, size_t len)
{
    char *dst;

    if (raw == NULL)
        return NULL;

    dst = aos_malloc(len + 1);
    if (dst == NULL)
        return NULL;

    memcpy(dst, raw, len);
    return dst;
}

static char *__amp_strdup(char *src)
{
    char *dst;
    size_t len = 0;

    if (src == NULL) {
        return NULL;
    }

    len = strlen(src);

    dst = aos_malloc(len + 1);
    if (dst == NULL) {
        return NULL;
    }

    memcpy(dst, src, len);
    dst[len] = '\0';
    return dst;
}

static void subcribe_cb(void *handle, const aiot_mqtt_recv_t *packet, void *userdata)
{
    iot_device_handle_t *iot_device_handle = (iot_device_handle_t *)userdata;
    mp_obj_t ret_dict;

    switch (packet->type) {
    case AIOT_MQTTRECV_PUB:
        {
            mp_sched_carg_t *carg = make_cargs(MP_SCHED_CTYPE_DICT);
            make_carg_entry(carg, 0, MP_SCHED_ENTRY_TYPE_STR, packet->data.pub.topic_len, packet->data.pub.topic,
                            "topic");
            make_carg_entry(carg, 1, MP_SCHED_ENTRY_TYPE_STR, packet->data.pub.payload_len, packet->data.pub.payload,
                            "payload");
            mp_obj_t callback = iot_device_handle->callback[ON_SUBCRIBE];
            callback_to_python_LoBo(callback, mp_const_none, carg);

            break;
        }
    default:
        break;
    }
}

/* cb call to python */
static void aiot_device_notify(void *pdata)
{
    device_notify_param_t *param = (device_notify_param_t *)pdata;

    mp_obj_t dict = MP_OBJ_NULL;
    mp_obj_t callback = MP_OBJ_NULL;

    if (param->dm_recv) {
        switch (param->dm_recv_type) {
        case AIOT_DMRECV_PROPERTY_SET:
            {
                mp_sched_carg_t *carg = make_cargs(MP_SCHED_CTYPE_DICT);
                make_carg_entry(carg, 0, MP_SCHED_ENTRY_TYPE_INT, param->ret_code, NULL, "code");
                make_carg_entry(carg, 1, MP_SCHED_ENTRY_TYPE_INT, param->msg_id, NULL, "msg_id");
                make_carg_entry(carg, 2, MP_SCHED_ENTRY_TYPE_INT, param->params_len, NULL, "params_len");
                make_carg_entry(carg, 3, MP_SCHED_ENTRY_TYPE_STR, param->params_len, param->params, "params");
                callback = param->iot_device_handle->callback[ON_PROPS];
                callback_to_python_LoBo(callback, mp_const_none, carg);

                aos_free(param->params);
                break;
            }

        case AIOT_DMRECV_ASYNC_SERVICE_INVOKE:
            {
                mp_sched_carg_t *carg = make_cargs(MP_SCHED_CTYPE_DICT);
                make_carg_entry(carg, 0, MP_SCHED_ENTRY_TYPE_INT, param->ret_code, NULL, "code");
                make_carg_entry(carg, 1, MP_SCHED_ENTRY_TYPE_INT, param->msg_id, NULL, "msg_id");
                make_carg_entry(carg, 2, MP_SCHED_ENTRY_TYPE_STR, strlen(param->service_id), param->service_id, "service_id");
                make_carg_entry(carg, 3, MP_SCHED_ENTRY_TYPE_INT, param->params_len, NULL, "params_len");
                make_carg_entry(carg, 4, MP_SCHED_ENTRY_TYPE_STR, param->params_len, param->params, "params");
                callback = param->iot_device_handle->callback[ON_SERVICE];
                callback_to_python_LoBo(callback, mp_const_none, carg);

                aos_free(param->service_id);
                aos_free(param->params);
                break;
            }
        case AIOT_DMRECV_RAW_DATA:
            {
                mp_sched_carg_t *carg = make_cargs(MP_SCHED_CTYPE_DICT);
                make_carg_entry(carg, 0, MP_SCHED_ENTRY_TYPE_INT, param->params_len, NULL, "params_len");
                make_carg_entry(carg, 1, MP_SCHED_ENTRY_TYPE_STR, param->params_len, param->params, "params");
                callback = param->iot_device_handle->callback[ON_RAWDATA];
                callback_to_python_LoBo(callback, mp_const_none, carg);

                aos_free(param->params);
                break;
            }
        default:
            aos_free(param);
            return;
        }
    } else if (param->option == AIOT_MQTTOPT_EVENT_HANDLER) {
        switch (param->event_type) {
        case AIOT_MQTTEVT_CONNECT:
        case AIOT_MQTTEVT_DISCONNECT:
            {
                mp_sched_carg_t *carg = make_cargs(MP_SCHED_CTYPE_DICT);
                make_carg_entry(carg, 0, MP_SCHED_ENTRY_TYPE_INT, param->ret_code, NULL, "code");
                callback = param->iot_device_handle->callback[param->cb_type];
                callback_to_python_LoBo(callback, mp_const_none, carg);

                aos_free(param->params);
                break;
            }
        default:
            aos_free(param);
            return;
        }
    } else {
        switch (param->recv_type) {
        case AIOT_MQTTRECV_PUB:
            break;
        default:
            aos_free(param);
            return;
        }
    }

    aos_free(param);
}

/* 用户数据接收处理回调函数 */
static void aiot_app_dm_recv_handler(void *dm_handle, const aiot_dm_recv_t *recv, void *userdata)
{
    iot_device_handle_t *iot_device_handle = (iot_device_handle_t *)userdata;

    device_notify_param_t *param;

    amp_debug(MOD_STR, "aiot_app_dm_recv_handler IS CALLED.\r\n");

    param = aos_malloc(sizeof(device_notify_param_t));
    if (!param) {
        amp_error(MOD_STR, "alloc device_notify_param_t fail.\r\n");
        return;
    }
    memset(param, 0, sizeof(device_notify_param_t));
    param->dm_recv = 1;
    param->dm_recv_type = recv->type;
    param->iot_device_handle = iot_device_handle;
    switch (recv->type) {
    /* 属性上报, 事件上报, 获取期望属性值或者删除期望属性值的应答 */
    case AIOT_DMRECV_GENERIC_REPLY:
        {
            amp_debug(MOD_STR, "msg_id = %d, code = %d, data = %.*s, message = %.*s\r\n",
                      recv->data.generic_reply.msg_id, recv->data.generic_reply.code, recv->data.generic_reply.data_len,
                      recv->data.generic_reply.data, recv->data.generic_reply.message_len,
                      recv->data.generic_reply.message);
        }
        break;

    /* 属性设置 */
    case AIOT_DMRECV_PROPERTY_SET:
        {
            amp_debug(MOD_STR, "msg_id = %ld, params = %.*s\r\n", (unsigned long)recv->data.property_set.msg_id,
                      recv->data.property_set.params_len, recv->data.property_set.params);

            // param->js_cb_ref =
            // iot_device_handle->js_cb_ref[AIOT_DEV_JSCALLBACK_ONPROPS_REF];
            param->msg_id = recv->data.property_set.msg_id;
            param->params_len = recv->data.property_set.params_len;
            param->params = __amp_strdup(recv->data.property_set.params);
            param->cb_type = AIOT_DEV_JSCALLBACK_ONPROPS_REF;

            /* TODO: 以下代码演示如何对来自云平台的属性设置指令进行应答,
             * 用户可取消注释查看演示效果 */
            // {
            //     aiot_dm_msg_t msg;
            //     memset(&msg, 0, sizeof(aiot_dm_msg_t));
            //     msg.type = AIOT_DMMSG_PROPERTY_SET_REPLY;
            //     msg.data.property_set_reply.msg_id =
            //     recv->data.property_set.msg_id;
            //     msg.data.property_set_reply.code = 200;
            //     msg.data.property_set_reply.data = "{}";
            //     int32_t res = aiot_dm_send(dm_handle, &msg);
            //     if (res < 0) {
            //         printf("aiot_dm_send failed\r\n");
            //     }
            // }
        }
        break;

    /* 异步服务调用 */
    case AIOT_DMRECV_ASYNC_SERVICE_INVOKE:
        {
            amp_debug(MOD_STR, "msg_id = %ld, service_id = %s, params = %.*s\r\n",
                      (unsigned long)recv->data.async_service_invoke.msg_id, recv->data.async_service_invoke.service_id,
                      recv->data.async_service_invoke.params_len, recv->data.async_service_invoke.params);

            // param->js_cb_ref =
            // iot_device_handle->js_cb_ref[AIOT_DEV_JSCALLBACK_ONSERVICE_REF];
            param->msg_id = recv->data.async_service_invoke.msg_id;
            param->params_len = recv->data.async_service_invoke.params_len;
            param->service_id = __amp_strdup(recv->data.async_service_invoke.service_id);
            param->params = __amp_strdup(recv->data.async_service_invoke.params);
            param->cb_type = AIOT_DEV_JSCALLBACK_ONPROPS_REF;

            /* TODO: 以下代码演示如何对来自云平台的异步服务调用进行应答,
             * 用户可取消注释查看演示效果
             *
             * 注意: 如果用户在回调函数外进行应答, 需要自行保存msg_id,
             * 因为回调函数入参在退出回调函数后将被SDK销毁, 不可以再访问到
             */
            // {
            //     aiot_dm_msg_t msg;
            //     memset(&msg, 0, sizeof(aiot_dm_msg_t));
            //     msg.type = AIOT_DMMSG_ASYNC_SERVICE_REPLY;
            //     msg.data.async_service_reply.msg_id =
            //     recv->data.async_service_invoke.msg_id;
            //     msg.data.async_service_reply.code = 200;
            //     msg.data.async_service_reply.service_id =
            //     "ToggleLightSwitch"; msg.data.async_service_reply.data =
            //     "{\"dataA\": 20}"; int32_t res = aiot_dm_send(dm_handle,
            //     &msg); if (res < 0) {
            //         printf("aiot_dm_send failed\r\n");
            //     }
            // }
        }
        break;

    /* 同步服务调用 */
    case AIOT_DMRECV_SYNC_SERVICE_INVOKE:
        {
            amp_debug(MOD_STR,
                      "msg_id = %ld, rrpc_id = %s, service_id = %s, params "
                      "= %.*s\r\n",
                      (unsigned long)recv->data.sync_service_invoke.msg_id, recv->data.sync_service_invoke.rrpc_id,
                      recv->data.sync_service_invoke.service_id, recv->data.sync_service_invoke.params_len,
                      recv->data.sync_service_invoke.params);

            /* TODO: 以下代码演示如何对来自云平台的同步服务调用进行应答,
             * 用户可取消注释查看演示效果
             *
             * 注意: 如果用户在回调函数外进行应答,
             * 需要自行保存msg_id和rrpc_id字符串,
             * 因为回调函数入参在退出回调函数后将被SDK销毁, 不可以再访问到
             */
            // {
            //     aiot_dm_msg_t msg;
            //     memset(&msg, 0, sizeof(aiot_dm_msg_t));
            //     msg.type = AIOT_DMMSG_SYNC_SERVICE_REPLY;
            //     msg.data.sync_service_reply.rrpc_id =
            //     recv->data.sync_service_invoke.rrpc_id;
            //     msg.data.sync_service_reply.msg_id =
            //     recv->data.sync_service_invoke.msg_id;
            //     msg.data.sync_service_reply.code = 200;
            //     msg.data.sync_service_reply.service_id =
            //     "SetLightSwitchTimer"; msg.data.sync_service_reply.data =
            //     "{}"; int32_t res = aiot_dm_send(dm_handle, &msg); if (res
            //     <0) {
            //         printf("aiot_dm_send failed\r\n");
            //     }
            // }
        }
        break;

    /* 下行二进制数据 */
    case AIOT_DMRECV_RAW_DATA:
        {
            amp_debug(MOD_STR, "raw data len = %d\r\n", recv->data.raw_data.data_len);
            /* TODO: 以下代码演示如何发送二进制格式数据,
             * 若使用需要有相应的数据透传脚本部署在云端 */
            // {
            //     aiot_dm_msg_t msg;
            //     uint8_t raw_data[] = {0x01, 0x02};

            //     memset(&msg, 0, sizeof(aiot_dm_msg_t));
            //     msg.type = AIOT_DMMSG_RAW_DATA;
            //     msg.data.raw_data.data = raw_data;
            //     msg.data.raw_data.data_len = sizeof(raw_data);
            //     aiot_dm_send(dm_handle, &msg);
            // }
        }
        param->params_len = recv->data.raw_data.data_len;
        param->params = __amp_rawdup(recv->data.raw_data.data, recv->data.raw_data.data_len);
        param->cb_type = ON_RAWDATA;
        break;

    /* 二进制格式的同步服务调用, 比单纯的二进制数据消息多了个rrpc_id */
    case AIOT_DMRECV_RAW_SYNC_SERVICE_INVOKE:
        {
            amp_debug(MOD_STR, "raw sync service rrpc_id = %s, data_len = %d\r\n",
                      recv->data.raw_service_invoke.rrpc_id, recv->data.raw_service_invoke.data_len);
        }
        break;

    default:
        break;
    }

    py_task_schedule_call(aiot_device_notify, param);
}
/* cb register */
STATIC mp_obj_t aiot_register_cb(mp_obj_t self_in, mp_obj_t id, mp_obj_t func)
{
    int callback_id = mp_obj_get_int(id);

    linksdk_device_obj_t *self = self_in;
    if (self == NULL) {
        return mp_obj_new_int(-1);
    }
    if (self->iot_device_handle->callback == NULL) {
        return mp_obj_new_int(-1);
    }
    self->iot_device_handle->callback[callback_id] = func;
    return mp_obj_new_int(0);
}
MP_DEFINE_CONST_FUN_OBJ_3(native_aiot_register_cb, aiot_register_cb);

/* connect event */
static void aiot_connect_event(iot_device_handle_t *iot_device_handle)
{
    int res = 0;

    if (iot_device_handle->dm_handle != NULL) {
        amp_warn(MOD_STR, "dm_handle is already initialized");
        return;
    }

    /* device model service */
    iot_device_handle->dm_handle = aiot_dm_init();
    if (iot_device_handle->dm_handle == NULL) {
        amp_debug(MOD_STR, "aiot_dm_init failed\r\n");
        return;
    }

    /* 配置MQTT实例句柄 */
    aiot_dm_setopt(iot_device_handle->dm_handle, AIOT_DMOPT_MQTT_HANDLE, iot_device_handle->mqtt_handle);
    /* 配置消息接收处理回调函数 */
    aiot_dm_setopt(iot_device_handle->dm_handle, AIOT_DMOPT_RECV_HANDLER, (void *)aiot_app_dm_recv_handler);
    /* 配置回调函数参数 */
    aiot_dm_setopt(iot_device_handle->dm_handle, AIOT_DMOPT_USERDATA, iot_device_handle);

    /* app device active info report */
    res = pyamp_amp_app_devinfo_report(iot_device_handle->mqtt_handle);
    if (res < STATE_SUCCESS) {
        amp_error(MOD_STR, "device active info report failed,ret:-0x%04X\r\n", -res);
    }
}

static void aiot_mqtt_message_cb(iot_mqtt_message_t *message, void *userdata)
{
    amp_debug(MOD_STR, "aiot_mqtt_message_cb IS CALLED\r\n");
    iot_mqtt_userdata_t *udata = (iot_mqtt_userdata_t *)userdata;
    if (!message || !udata) {
        return;
    }
    device_notify_param_t *param = aos_malloc(sizeof(device_notify_param_t));
    if (!param) {
        amp_error(MOD_STR, "alloc device notify param fail\r\n");
        return;
    }
    memset(param, 0, sizeof(device_notify_param_t));
    param->iot_device_handle = (iot_device_handle_t *)udata->handle;
    param->option = message->option;
    amp_debug(MOD_STR, "message->option is %d %p\r\n", message->option, param->iot_device_handle);

    if (message->option == AIOT_MQTTOPT_EVENT_HANDLER) {
        amp_debug(MOD_STR, "message->event.type is %d\r\n", message->event.type);
        switch (message->event.type) {
        case AIOT_MQTTEVT_CONNECT:
        case AIOT_MQTTEVT_RECONNECT:
            aiot_connect_event(param->iot_device_handle);
            param->ret_code = message->event.code;
            param->event_type = message->event.type;
            param->cb_type = ON_CONNECT;
            break;
        case AIOT_MQTTEVT_DISCONNECT:
            param->ret_code = message->event.code;
            param->event_type = message->event.type;
            param->cb_type = ON_DISCONNECT;
            break;
        default:
            aos_free(param);
            return;
        }
    } else if (message->option == AIOT_MQTTOPT_RECV_HANDLER) {
        amp_debug(MOD_STR, "message->recv.type is %d\r\n", message->recv.type);
        switch (message->recv.type) {
        case AIOT_MQTTRECV_PUB:
            param->ret_code = message->recv.code;
            param->topic_len = message->recv.topic_len;
            param->payload_len = message->recv.payload_len;
            param->topic = __amp_strdup(message->recv.topic);
            param->payload = __amp_strdup(message->recv.payload);
            param->recv_type = message->recv.type;
            break;
        default:
            aos_free(param);
            return;
        }
    } else {
        aos_free(param);
        return;
    }
    py_task_schedule_call(aiot_device_notify, param);
}

/* dynmic register */
static mp_obj_t aiot_dynreg(mp_obj_t self_in, mp_obj_t data, mp_obj_t cb)
{
    int ret = -1;
    char *region = NULL;
    char *productKey = NULL;
    char *deviceName = NULL;
    char *productSecret = NULL;
    linksdk_device_obj_t *self = self_in;
    if (self_in == NULL) {
        mp_raise_OSError(ENOENT);
        return mp_obj_new_int(ret);
    }

    if (!mp_obj_is_dict_or_ordereddict(data)) {
        amp_error(MOD_STR, "%s  param1 type error,param type must be dict\r\n", __func__);
        return mp_obj_new_int(-1);
    }

    mp_obj_t index = mp_obj_new_str_via_qstr("productKey", 10);
    productKey = mp_obj_str_get_str(mp_obj_dict_get(data, index));

    index = mp_obj_new_str_via_qstr("productSecret", 13);
    productSecret = mp_obj_str_get_str(mp_obj_dict_get(data, index));

    index = mp_obj_new_str_via_qstr("deviceName", 10);
    deviceName = mp_obj_str_get_str(mp_obj_dict_get(data, index));

    // Compatible with old usage， use cn-shanghai by default
    if (mp_obj_dict_len(data) < 4) {
        region = "cn-shanghai";
    } else {
        index = mp_obj_new_str_via_qstr("region", 6);
        region = mp_obj_str_get_str(mp_obj_dict_get(data, index));
    }

    if (region == NULL || productKey == NULL || productSecret == NULL) {
        amp_error(MOD_STR, "Invalid params, please check it. region=%s productKey=%s productSecret=%s deviceName=%d\r\n", region, productKey, productSecret,
             deviceName);
        return mp_obj_new_int(-1);
    }
    aos_kv_set(AMP_CUSTOMER_REGION, region, IOTX_REGION_LEN, 1);
    aos_kv_set(AMP_CUSTOMER_PRODUCTKEY, productKey, IOTX_PRODUCT_KEY_LEN, 1);
    aos_kv_set(AMP_CUSTOMER_DEVICENAME, deviceName, IOTX_DEVICE_NAME_LEN, 1);
    aos_kv_set(AMP_CUSTOMER_PRODUCTSECRET, productSecret, IOTX_PRODUCT_SECRET_LEN, 1);

    ret = pyamp_aiot_dynreg_http(cb);
    if (ret < STATE_SUCCESS) {
        amp_debug(MOD_STR, "device dynmic register failed\r\n");
        return mp_obj_new_int(ret);
    }

    return mp_obj_new_int(0);
}
MP_DEFINE_CONST_FUN_OBJ_3(native_aiot_dynreg, aiot_dynreg);

/* device connect task */
static void aiot_device_connect(void *pdata)
{
    int res = -1;
    device_notify_param_t *param;
    // ota_service_t *ota_svc = &pyamp_g_aiot_device_appota_service;
    iot_device_handle_t *iot_device_handle = (iot_device_handle_t *)pdata;
    iot_mqtt_userdata_t *userdata;

    amp_debug(MOD_STR, "start mqtt connect task\r\n");

    uint16_t keepaliveSec = 0;

    keepaliveSec = iot_device_handle->keepaliveSec;

    userdata = aos_malloc(sizeof(iot_mqtt_userdata_t));
    if (!userdata) {
        amp_error(MOD_STR, "alloc mqtt userdata fail\r\n");
        return;
    }
    memset(userdata, 0, sizeof(iot_mqtt_userdata_t));
    userdata->callback = aiot_mqtt_message_cb;
    userdata->handle = iot_device_handle;

    res = pyamp_aiot_mqtt_client_start(&iot_device_handle->mqtt_handle, keepaliveSec, userdata);
    if (res < STATE_SUCCESS) {
        amp_error(MOD_STR, "mqtt client create failed,ret:-0x%04X\r\n", -res);
        aos_free(userdata);
        aos_free(iot_device_handle);
        return;
    }

    iot_device_handle->g_iot_conn_flag = 1;
    iot_device_handle->res = res;

    while (!iot_device_handle->g_iot_close_flag) {
        aos_msleep(1000);
    }

    pyamp_aiot_mqtt_client_stop(&iot_device_handle->mqtt_handle);

    aos_free(userdata);
    iot_device_handle->g_iot_conn_flag = 0;
    linksdk_device_is_used = false;
    aos_sem_signal(&iot_device_handle->g_iot_close_sem);
    aos_task_exit(0);
}

/*************************************************************************************
 * Function:    native_aiot_create_device
 * Input:       none
 * Output:      return socket fd when create socket success,
 *            return error number when create socket fail
 **************************************************************************************/
static mp_obj_t aiot_create_device(mp_obj_t self_in, mp_obj_t data)
{
    int res = -1;
    void *mqtt_handle = NULL;
    const char *region = NULL;
    const char *productKey = NULL;
    const char *deviceName = NULL;
    const char *deviceSecret = NULL;
    u_int32_t keepaliveSec = 0;
    aos_task_t iot_device_task;
    // ota_service_t *ota_svc = &pyamp_g_aiot_device_appota_service;

    linksdk_device_obj_t *self = self_in;
    if (self_in == NULL) {
        mp_raise_OSError(ENOENT);
        amp_error(MOD_STR, "aiot_device_connect, self is NULL. \r\n");
        return mp_obj_new_int(-1);
    }

    if (linksdk_device_is_used == true) {
        mp_raise_OSError(ENOENT);
        return mp_obj_new_int(-1);
    } else {
        linksdk_device_is_used = true;
    }

    if (!mp_obj_is_dict_or_ordereddict(data)) {
        amp_error(MOD_STR, "%s  param1 type error,param type must be dict\r\n", __func__);
        goto failed;
    }

    mp_obj_t index = mp_obj_new_str_via_qstr("productKey", 10);
    productKey = mp_obj_str_get_str(mp_obj_dict_get(data, index));

    index = mp_obj_new_str_via_qstr("deviceName", 10);
    deviceName = mp_obj_str_get_str(mp_obj_dict_get(data, index));

    index = mp_obj_new_str_via_qstr("deviceSecret", 12);
    deviceSecret = mp_obj_str_get_str(mp_obj_dict_get(data, index));

    index = mp_obj_new_str_via_qstr("keepaliveSec", 12);
    keepaliveSec = mp_obj_get_int(mp_obj_dict_get(data, index));

    // Compatible with old usage， use cn-shanghai by default
    if (mp_obj_dict_len(data) < 5) {
        region = "cn-shanghai";
    } else {
        index = mp_obj_new_str_via_qstr("region", 6);
        region = mp_obj_str_get_str(mp_obj_dict_get(data, index));
    }

    if (region == NULL || productKey == NULL || deviceSecret == NULL || deviceName == NULL) {
        amp_error(MOD_STR, "Invalid params, please check it. region=%s productKey=%s deviceName=%s deviceSecret=%s keepaliveSec=%d\r\n", region, productKey, deviceName,
             deviceSecret, keepaliveSec);
        return mp_obj_new_int(-1);
    }
    /*
         memset(ota_svc->pk, 0, sizeof(ota_svc->pk));
         memset(ota_svc->dn, 0, sizeof(ota_svc->dn));
         memset(ota_svc->ds, 0, sizeof(ota_svc->ds));
         memcpy(ota_svc->pk, productKey, strlen(productKey));
         memcpy(ota_svc->dn, deviceName, strlen(deviceName));
         memcpy(ota_svc->ds, deviceSecret, strlen(deviceSecret));
     */
    aos_kv_set(AMP_CUSTOMER_REGION, region, IOTX_REGION_LEN, 1);
    aos_kv_set(AMP_CUSTOMER_PRODUCTKEY, productKey, IOTX_PRODUCT_KEY_LEN, 1);
    aos_kv_set(AMP_CUSTOMER_DEVICENAME, deviceName, IOTX_DEVICE_NAME_LEN, 1);
    aos_kv_set(AMP_CUSTOMER_DEVICESECRET, deviceSecret, IOTX_DEVICE_SECRET_LEN, 1);

    self->iot_device_handle->keepaliveSec = keepaliveSec;

    res = aos_task_new_ext(&iot_device_task, "amp aiot device task", aiot_device_connect, self->iot_device_handle,
                           1024 * 5, AOS_DEFAULT_APP_PRI);
    if (res != STATE_SUCCESS) {
        amp_error(MOD_STR, "iot create task failed.\r\n");
        goto failed;
    }
    return mp_obj_new_int(0);

failed:
    linksdk_device_is_used = false;
    return mp_obj_new_int(-1);
}

MP_DEFINE_CONST_FUN_OBJ_2(native_aiot_create_device, aiot_create_device);

/* post props */
static mp_obj_t aiot_postProps(mp_obj_t self_in, mp_obj_t data)
{
    int res = -1;
    char *payload_str = NULL;
    amp_debug(MOD_STR, "native_aiot_postProps is called.\r\n");

    linksdk_device_obj_t *self = self_in;
    if (self_in == NULL) {
        mp_raise_OSError(ENOENT);
        return mp_obj_new_int(res);
    }

    if (!mp_obj_is_dict_or_ordereddict(data)) {
        amp_error(MOD_STR, "%s  data type error,param type must be dict\r\n", __func__);
        return mp_obj_new_int(-1);
    }

    mp_obj_t index = mp_obj_new_str_via_qstr("params", 6);
    payload_str = mp_obj_str_get_str(mp_obj_dict_get(data, index));

    amp_debug(MOD_STR, "PostProps payload is %s\r\n", payload_str);

    if (self->iot_device_handle->dm_handle == NULL) {
        amp_error(MOD_STR, "*****dm handle is null\r\n");
        return mp_obj_new_int(-1);
    }
    res = pyamp_aiot_app_send_property_post(self->iot_device_handle->dm_handle, payload_str);
    if (res < STATE_SUCCESS) {
        amp_error(MOD_STR, "property post failed, res:-0x%04X\r\n", -res);
        return mp_obj_new_int(res);
    }

    return mp_obj_new_int(0);
}
MP_DEFINE_CONST_FUN_OBJ_2(native_aiot_postProps, aiot_postProps);

/* post event */
static mp_obj_t aiot_postEvent(mp_obj_t self_in, mp_obj_t data)
{
    int res = -1;
    char *event_id = NULL;
    char *event_payload = NULL;
    amp_debug(MOD_STR, "native_aiot_postEvent is called.\r\n");
    linksdk_device_obj_t *self = self_in;
    if (self_in == NULL) {
        mp_raise_OSError(ENOENT);
        return mp_obj_new_int(res);
    }

    if (!mp_obj_is_dict_or_ordereddict(data)) {
        amp_warn(MOD_STR, "%s data type error,param type must be dict.\r\n", __func__);
        return mp_obj_new_int(-1);
    }

    mp_obj_t index = mp_obj_new_str_via_qstr("id", 2);
    event_id = mp_obj_str_get_str(mp_obj_dict_get(data, index));

    index = mp_obj_new_str_via_qstr("params", 6);
    mp_obj_t payload_d = mp_obj_dict_get(data, index);

    vstr_t vstr;
    mp_print_t print;
    vstr_init_print(&vstr, 8, &print);
    mp_obj_print_helper(&print, payload_d, PRINT_JSON);
    mp_obj_t tmp = mp_obj_new_str_from_vstr(&mp_type_str, &vstr);

    event_payload = mp_obj_str_get_str(tmp);

    amp_debug(MOD_STR, "native_aiot_postEvent event_id=%s event_payload=%s\r\n", event_id, event_payload);

    res = pyamp_aiot_app_send_event_post(self->iot_device_handle->dm_handle, event_id, event_payload);
    if (res < STATE_SUCCESS) {
        amp_error(MOD_STR, "post event failed!\r\n");
        return mp_obj_new_int(res);
    }
    return mp_obj_new_int(0);
}
MP_DEFINE_CONST_FUN_OBJ_2(native_aiot_postEvent, aiot_postEvent);

/* post rawdata */
static mp_obj_t aiot_postRaw(mp_obj_t self_in, mp_obj_t data)
{
    int res = -1;
    mp_obj_t bytearray = NULL;

    linksdk_device_obj_t *self = self_in;
    if (self_in == NULL) {
        mp_raise_OSError(ENOENT);
        return mp_obj_new_int(res);
    }

    if (!mp_obj_is_dict_or_ordereddict(data)) {
        amp_error(MOD_STR, "%s  data type error,param type must be dict\r\n", __func__);
        return mp_obj_new_int(-1);
    }

    mp_obj_t index = mp_obj_new_str_via_qstr("param", 5);
    bytearray = mp_obj_dict_get(data, index);
    if (!bytearray) {
        amp_error(MOD_STR, "%s  params data type error,param data type must be bytearray\r\n", __func__);
        return mp_obj_new_int(-1);
    }
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(bytearray, &bufinfo, MP_BUFFER_READ);

    if (self->iot_device_handle->dm_handle == NULL)
        return mp_obj_new_int(-1);

    res = aiot_app_send_rawdata_post(self->iot_device_handle->dm_handle, bufinfo.buf, bufinfo.len);
    if (res < STATE_SUCCESS) {
        amp_error(MOD_STR, "property post failed, res:-0x%04X.\r\n", -res);
        return mp_obj_new_int(res);
    }

    return mp_obj_new_int(0);
}
MP_DEFINE_CONST_FUN_OBJ_2(native_aiot_postRaw, aiot_postRaw);

/*  onprops */
static mp_obj_t aiot_onProps(mp_obj_t self_in, mp_obj_t data)
{
    int res = -1;

    linksdk_device_obj_t *self = (linksdk_device_obj_t *)self_in;
    if (self_in == NULL) {
        mp_raise_OSError(ENOENT);
        return mp_obj_new_int(res);
    }

    return mp_obj_new_int(res);
}
MP_DEFINE_CONST_FUN_OBJ_2(native_aiot_onProps, aiot_onProps);

/*  onService */
static mp_obj_t aiot_onService(mp_obj_t self_in, mp_obj_t data)
{
    int res = 0;
    return mp_obj_new_int(res);
}
MP_DEFINE_CONST_FUN_OBJ_2(native_aiot_onService, aiot_onService);

/* get ntp time */
static mp_obj_t hapy_get_ntp_time(mp_obj_t self_in, mp_obj_t cb)
{
    int res = -1;
    linksdk_device_obj_t *self = self_in;
    if (self_in == NULL) {
        return mp_obj_new_int(-1);
    }
    iot_device_handle_t *iot_device_handle = self->iot_device_handle;
    if (!iot_device_handle) {
        amp_error(MOD_STR, "parameter must be handle\r\n");
        return mp_obj_new_int(-1);
    }
    res = hapy_aiot_amp_ntp_service(iot_device_handle->mqtt_handle, cb);
    if (res < STATE_SUCCESS) {
        amp_error(MOD_STR, "device get ntp failed\r\n");
        return mp_obj_new_int(res);
    }

    return mp_obj_new_int(0);
}
MP_DEFINE_CONST_FUN_OBJ_2(native_aiot_get_ntp_time, hapy_get_ntp_time);

/* upload File */
static mp_obj_t aiot_uploadFile(size_t n_args, const mp_obj_t *args)
{
    int ret = -1;

    char *upload_id = NULL;
    linksdk_device_obj_t *self = (linksdk_device_obj_t *)MP_OBJ_TO_PTR(args[0]);
    mp_obj_t remoteFileName = args[1];
    mp_obj_t localFilePath = args[2];
    mp_obj_t cb = args[3];
    mp_buffer_info_t bufinfo;

    iot_device_handle_t *iot_device_handle = self->iot_device_handle;
    if (!iot_device_handle) {
        amp_error(MOD_STR, "parameter must be handle\r\n");
        goto out;
    }
    char *filename = (char *)mp_obj_str_get_str(remoteFileName);
    char *filepath = (char *)mp_obj_str_get_str(localFilePath);

    MP_THREAD_GIL_EXIT();
    upload_id = pyamp_aiot_upload_mqtt(iot_device_handle->mqtt_handle, filename, filepath, 0, cb);
    MP_THREAD_GIL_ENTER();
    if (!upload_id) {
        amp_error(MOD_STR, "aiot upload file failed!\r\n");
        goto out;
    }
    return mp_obj_new_str(upload_id, strlen(upload_id));
out:
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR(native_aiot_uploadFile, 3, aiot_uploadFile);

/* upload Content */
static mp_obj_t aiot_uploadContent(size_t n_args, const mp_obj_t *args)
{
    int ret = -1;

    char *upload_id = NULL;
    linksdk_device_obj_t *self = (linksdk_device_obj_t *)MP_OBJ_TO_PTR(args[0]);
    mp_obj_t file_name = args[1];
    mp_obj_t data = args[2];
    mp_obj_t cb = args[3];
    mp_buffer_info_t bufinfo;

    iot_device_handle_t *iot_device_handle = self->iot_device_handle;
    if (!iot_device_handle) {
        amp_error(MOD_STR, "parameter must be handle\r\n");
        goto out;
    }
    char *filename = (char *)mp_obj_str_get_str(file_name);
    mp_get_buffer_raise(data, &bufinfo, MP_BUFFER_READ);
    // printf("bufinfo len 0x%x\n", bufinfo.len);
    MP_THREAD_GIL_EXIT();
    upload_id = pyamp_aiot_upload_mqtt(iot_device_handle->mqtt_handle, filename, bufinfo.buf, bufinfo.len, cb);
    MP_THREAD_GIL_ENTER();
    if (!upload_id) {
        amp_error(MOD_STR, "aiot upload content failed!\r\n");
        goto out;
    }
    return mp_obj_new_str(upload_id, strlen(upload_id));
out:
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR(native_aiot_uploadContent, 3, aiot_uploadContent);

/* publish */
static mp_obj_t aiot_publish(mp_obj_t self_in, mp_obj_t data)
{
    int res = -1;
    const char *topic;
    const char *payload;
    uint16_t payload_len = 0;
    uint8_t qos = 0;

    linksdk_device_obj_t *self = (linksdk_device_obj_t *)self_in;
    if (self_in == NULL) {
        mp_raise_OSError(ENOENT);
        return mp_obj_new_int(res);
    }

    if (!mp_obj_is_dict_or_ordereddict(data)) {
        amp_error(MOD_STR, "%s  data type error,param type must be dict\r\n", __func__);
        return mp_obj_new_int(-1);
    }

    mp_obj_t index = mp_obj_new_str_via_qstr("topic", 5);
    topic = mp_obj_str_get_str(mp_obj_dict_get(data, index));

    index = mp_obj_new_str_via_qstr("payload", 7);
    payload = mp_obj_str_get_str(mp_obj_dict_get(data, index));
    payload_len = strlen(payload);

    index = mp_obj_new_str_via_qstr("qos", 3);
    qos = mp_obj_get_int(mp_obj_dict_get(data, index));
    amp_debug(MOD_STR, "publish topic: %s, payload: %s, payload_len is %d , qos is: %d\r\n", topic, payload,
              payload_len, qos);
    MP_THREAD_GIL_EXIT();
    res = aiot_mqtt_pub(self->iot_device_handle->mqtt_handle, topic, payload, payload_len, qos);
    MP_THREAD_GIL_ENTER();
    if (res < STATE_SUCCESS) {
        amp_error(MOD_STR, "aiot app mqtt publish failed\r\n");
        return mp_obj_new_int(res);
    }

    return mp_obj_new_int(0);
}
MP_DEFINE_CONST_FUN_OBJ_2(native_aiot_publish, aiot_publish);

/* unsubscribe */
static mp_obj_t aiot_unsubscribe(mp_obj_t self_in, mp_obj_t data)
{
    int res = -1;
    char *topic;
    uint8_t qos = 0;

    linksdk_device_obj_t *self = self_in;
    if (self_in == NULL) {
        mp_raise_OSError(ENOENT);
        return mp_obj_new_int(res);
    }

    if (!mp_obj_is_dict_or_ordereddict(data)) {
        amp_error(MOD_STR, "%s  data type error,param type must be dict\r\n", __func__);
        return mp_obj_new_int(-1);
    }

    mp_obj_t index = mp_obj_new_str_via_qstr("topic", 5);
    topic = mp_obj_str_get_str(mp_obj_dict_get(data, index));

    amp_debug(MOD_STR, "unsubscribe topic: %s\r\n", topic);

    res = aiot_mqtt_unsub(self->iot_device_handle->mqtt_handle, topic);
    if (res < STATE_SUCCESS) {
        amp_error(MOD_STR, "aiot app mqtt unsubscribe failed\r\n");
        return mp_obj_new_int(res);
    } else {
        amp_info(MOD_STR, "unsubcribe topic: %s  succeed\r\n", topic);
    }

    return mp_obj_new_int(0);
}
MP_DEFINE_CONST_FUN_OBJ_2(native_aiot_unsubscribe, aiot_unsubscribe);

/* subscribe */
static mp_obj_t aiot_subscribe(mp_obj_t self_in, mp_obj_t data)
{
    int res = -1;
    char *topic = NULL;
    uint32_t qos = 0;
    amp_debug(MOD_STR, "native_aiot_subscribe is called\r\n");

    linksdk_device_obj_t *self = self_in;
    if (self_in == NULL) {
        mp_raise_OSError(ENOENT);
        return mp_obj_new_int(res);
    }

    if (!mp_obj_is_dict_or_ordereddict(data)) {
        amp_error(MOD_STR, "%s  data type error,param type must be dict\r\n", __func__);
        return mp_obj_new_int(-1);
    }

    mp_obj_t index = mp_obj_new_str_via_qstr("topic", 5);
    topic = mp_obj_str_get_str(mp_obj_dict_get(data, index));

    index = mp_obj_new_str_via_qstr("qos", 3);
    qos = mp_obj_get_int(mp_obj_dict_get(data, index));

    amp_debug(MOD_STR, "subscribe topic: %s, qos is: %d.\r\n", topic, qos);

    res =
        aiot_mqtt_sub(self->iot_device_handle->mqtt_handle, topic, subcribe_cb, (uint8_t)qos, self->iot_device_handle);
    if (res < STATE_SUCCESS) {
        amp_error(MOD_STR, "aiot app mqtt subscribe failed\r\n");
        return mp_obj_new_int(res);
    } else {
        amp_info(MOD_STR, "subcribe topic: %s  succeed\r\n", topic);
    }

    return mp_obj_new_int(0);
}
MP_DEFINE_CONST_FUN_OBJ_2(native_aiot_subscribe, aiot_subscribe);

/*************************************************************************************
 * Function:    native_aiot_close
 * Description: js native addon for
 *            UDP.close(sock_id)
 * Called by:   js api
 * Input:       sock_id: interger
 *
 * Output:      return 0 when UDP.close call ok
 *            return error number UDP.close call fail
 **************************************************************************************/
static mp_obj_t aiot_device_close(mp_obj_t self_in)
{
    int res = -1;
    linksdk_device_obj_t *self = self_in;
    if (self_in == NULL) {
        goto out;
    }

    if (linksdk_device_is_used == false) {
        printf("close failed, connect content is not run\n");
        res = -1;
        goto out;
    }

    iot_device_handle_t *iot_device_handle = self->iot_device_handle;

    if (iot_device_handle->g_iot_conn_flag) {
        iot_device_handle->g_iot_close_flag = 1;
        aos_sem_wait(&iot_device_handle->g_iot_close_sem, 8000);
        iot_device_handle->g_iot_close_flag = 0;
        linksdk_device_is_inited = false;
        return mp_obj_new_int(0);
    }

out:
    linksdk_device_is_inited = false;
    return mp_obj_new_int(-1);
}
MP_DEFINE_CONST_FUN_OBJ_1(native_aiot_device_close, aiot_device_close);

/* get device handle */
static mp_obj_t get_device_handle(mp_obj_t self_in)
{
    int res = -1;
    linksdk_device_obj_t *self = self_in;
    if (self_in == NULL) {
        goto out;
    }
    iot_device_handle_t *iot_device_handle = self->iot_device_handle;
    if (!iot_device_handle) {
        amp_error(MOD_STR, "parameter must be handle\r\n");
        goto out;
    } else {
        return mp_obj_new_int((mp_int_t)iot_device_handle);
    }
out:
    return mp_obj_new_int((mp_int_t)0);
}
MP_DEFINE_CONST_FUN_OBJ_1(native_aiot_get_device_handle, get_device_handle);

/* get device info */
static mp_obj_t aiot_getDeviceInfo(mp_obj_t self_in)
{
    int res = -1;
    if (self_in == NULL) {
        goto out;
    }
    /* get device tripple info */
    char region[IOTX_REGION_LEN + 1] = {0};
    char productKey[IOTX_PRODUCT_KEY_LEN + 1] = { 0 };
    char productSecret[IOTX_PRODUCT_SECRET_LEN + 1] = { 0 };
    char deviceName[IOTX_DEVICE_NAME_LEN + 1] = { 0 };
    char deviceSecret[IOTX_DEVICE_SECRET_LEN + 1] = { 0 };

    int regionLen = IOTX_REGION_LEN;
    int productKeyLen = IOTX_PRODUCT_KEY_LEN;
    int productSecretLen = IOTX_PRODUCT_SECRET_LEN;
    int deviceNameLen = IOTX_DEVICE_NAME_LEN;
    int deviceSecretLen = IOTX_DEVICE_SECRET_LEN;

    aos_kv_get(AMP_CUSTOMER_REGION, region, &regionLen);
    aos_kv_get(AMP_CUSTOMER_PRODUCTKEY, productKey, &productKeyLen);
    aos_kv_get(AMP_CUSTOMER_PRODUCTSECRET, productSecret, &productSecretLen);
    aos_kv_get(AMP_CUSTOMER_DEVICENAME, deviceName, &deviceNameLen);
    aos_kv_get(AMP_CUSTOMER_DEVICESECRET, deviceSecret, &deviceSecretLen);
    // dict solution
    mp_obj_t aiot_deviceinfo_dict = mp_obj_new_dict(5);
    mp_obj_dict_store(aiot_deviceinfo_dict, MP_OBJ_NEW_QSTR(qstr_from_str("region")),
                        mp_obj_new_str(region, strlen(region)));
    mp_obj_dict_store(aiot_deviceinfo_dict, MP_OBJ_NEW_QSTR(qstr_from_str("productKey")),
                        mp_obj_new_str(productKey, strlen(productKey)));
    mp_obj_dict_store(aiot_deviceinfo_dict, MP_OBJ_NEW_QSTR(qstr_from_str("productSecret")),
                        mp_obj_new_str(productSecret, strlen(productSecret)));
    mp_obj_dict_store(aiot_deviceinfo_dict, MP_OBJ_NEW_QSTR(qstr_from_str("deviceName")),
                        mp_obj_new_str(deviceName, strlen(deviceName)));
    mp_obj_dict_store(aiot_deviceinfo_dict, MP_OBJ_NEW_QSTR(qstr_from_str("deviceSecret")),
                        mp_obj_new_str(deviceSecret, strlen(deviceSecret)));
    return aiot_deviceinfo_dict;
out:
    return mp_obj_new_int((mp_int_t)0);
}
MP_DEFINE_CONST_FUN_OBJ_1(native_aiot_getDeviceInfo, aiot_getDeviceInfo);

STATIC const mp_rom_map_elem_t linkkit_device_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_on), MP_ROM_PTR(&native_aiot_register_cb) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_connect), MP_ROM_PTR(&native_aiot_create_device) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_getDeviceHandle), MP_ROM_PTR(&native_aiot_get_device_handle) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_getDeviceInfo), MP_ROM_PTR(&native_aiot_getDeviceInfo) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_uploadFile), MP_ROM_PTR(&native_aiot_uploadFile) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_uploadContent), MP_ROM_PTR(&native_aiot_uploadContent) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_register), MP_ROM_PTR(&native_aiot_dynreg) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_subscribe), MP_ROM_PTR(&native_aiot_subscribe) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_unsubscribe), MP_ROM_PTR(&native_aiot_unsubscribe) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_publish), MP_ROM_PTR(&native_aiot_publish) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_postProps), MP_ROM_PTR(&native_aiot_postProps) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_onProps), MP_ROM_PTR(&native_aiot_onProps) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_postRaw), MP_ROM_PTR(&native_aiot_postRaw) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_postEvent), MP_ROM_PTR(&native_aiot_postEvent) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_onService), MP_ROM_PTR(&native_aiot_onService) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_getNtpTime), MP_ROM_PTR(&native_aiot_get_ntp_time) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_end), MP_ROM_PTR(&native_aiot_device_close) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ON_CONNECT), MP_ROM_INT(ON_CONNECT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ON_DISCONNECT), MP_ROM_INT(ON_DISCONNECT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ON_SERVICE), MP_ROM_INT(ON_SERVICE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ON_PROPS), MP_ROM_INT(ON_PROPS) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ON_CLOSE), MP_ROM_INT(ON_CLOSE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ON_ERROR), MP_ROM_INT(ON_ERROR) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ON_RAWDATA), MP_ROM_INT(ON_RAWDATA) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ON_SUBCRIBE), MP_ROM_INT(ON_SUBCRIBE) },

};
STATIC MP_DEFINE_CONST_DICT(linksdk_device_locals_dict, linkkit_device_locals_dict_table);

STATIC void linksdk_device_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind)
{
    linksdk_device_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "aiot_device");
}

STATIC mp_obj_t linksdk_device_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args)
{
    if (linksdk_device_is_inited == true) {
        amp_error(MOD_STR,
                  "linksdk_device is limited to one instance running at a "
                  "time, pls check it\r\n");
        return MP_OBJ_NULL;
    }
    linksdk_device_is_inited = true;

    linksdk_device_obj_t *device_obj = m_new_obj(linksdk_device_obj_t);
    if (!device_obj) {
        mp_raise_OSError(ENOMEM);
        return mp_const_none;
    }
    memset(device_obj, 0, sizeof(linksdk_device_obj_t));

    iot_device_handle_t *iot_device_handle = m_new_obj(iot_device_handle_t);
    if (!iot_device_handle) {
        mp_raise_OSError(ENOMEM);
        return mp_const_none;
    }
    memset(iot_device_handle, 0, sizeof(iot_device_handle_t));
    iot_device_handle->callback = m_new0(mp_obj_t, ON_CB_MAX);
    aos_sem_new(&iot_device_handle->g_iot_connect_sem, 0);
    aos_sem_new(&iot_device_handle->g_iot_close_sem, 0);

    device_obj->iot_device_handle = iot_device_handle;
    device_obj->base.type = &linksdk_device_type;

    return MP_OBJ_FROM_PTR(device_obj);
}

const mp_obj_type_t linksdk_device_type = {
    { &mp_type_type },
    .name = MP_QSTR_Device,
    .print = linksdk_device_print,
    .make_new = linksdk_device_make_new,
    .locals_dict = (mp_obj_t)&linksdk_device_locals_dict,
};
