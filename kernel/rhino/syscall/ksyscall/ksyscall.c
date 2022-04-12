/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */
#include <k_api.h>
#include <aos/aos.h>
#include <hal/soc/soc.h>
#include <syscall.h>
#include <stdio.h>


/* ------------------ task ------------------ */

static ktask_t *sys_krhino_cur_task_get_stub(void *arg)
{
    arg = arg;

    return krhino_cur_task_get();
}

static void sys_krhino_task_sleep_stub(void *arg)
{
    krhino_task_sleep_syscall_arg_t *_arg = arg;

    krhino_task_sleep(_arg->tick);
}

static kstat_t sys_krhino_task_del_stub(void *arg)
{
    krhino_task_del_syscall_arg_t *_arg = arg;

    return krhino_task_del(_arg->task);
}

static kstat_t sys_krhino_utask_create_stub(void *arg)
{
    krhino_utask_create_syscall_arg_t *_arg = arg;

    return krhino_utask_create(_arg->task,
                               _arg->name,
                               _arg->arg,
                               _arg->prio,
                               _arg->ticks,
                               _arg->stack_buf,
                               _arg->stack_size,
                               _arg->kstack_size,
                               _arg->entry,
                               _arg->autorun);
}


/* ------------------ time ------------------ */

static sys_time_t sys_krhino_sys_tick_get_stub(void *arg)
{
    arg = arg;

    return krhino_sys_tick_get();
}

static sys_time_t sys_krhino_sys_time_get_stub(void *arg)
{
    arg = arg;

    return krhino_sys_time_get();
}

static tick_t sys_krhino_ms_to_ticks_stub(void *arg)
{
    krhino_ms_to_ticks_syscall_arg_t *_arg = arg;

    return krhino_ms_to_ticks(_arg->ms);
}

static sys_time_t sys_krhino_ticks_to_ms_stub(void *arg)
{
    krhino_ticks_to_ms_syscall_arg_t *_arg = arg;

    return krhino_ticks_to_ms(_arg->ticks);
}


/* ------------------ mutex ------------------ */

static kstat_t sys_krhino_mutex_create_stub(void *arg)
{
    krhino_mutex_create_syscall_arg_t *_arg = arg;

    return krhino_mutex_create(_arg->mutex,
                               _arg->name);
}

static kstat_t sys_krhino_mutex_del_stub(void *arg)
{
    krhino_mutex_del_syscall_arg_t *_arg = arg;

    return krhino_mutex_del(_arg->mutex);
}

static kstat_t sys_krhino_mutex_lock_stub(void *arg)
{
    krhino_mutex_lock_syscall_arg_t *_arg = arg;

    return krhino_mutex_lock(_arg->mutex,
                             _arg->ticks);
}

static kstat_t sys_krhino_mutex_unlock_stub(void *arg)
{
    krhino_mutex_unlock_syscall_arg_t *_arg = arg;

    return krhino_mutex_unlock(_arg->mutex);
}


/* ------------------ semphore ------------------ */

static kstat_t sys_krhino_sem_create_stub(void *arg)
{
    krhino_sem_create_syscall_arg_t *_arg = arg;

    return krhino_sem_create(_arg->sem,
                             _arg->name,
                             _arg->count);
}

static kstat_t sys_krhino_sem_del_stub(void *arg)
{
    krhino_sem_del_syscall_arg_t *_arg = arg;

    return krhino_sem_del(_arg->sem);
}

static kstat_t sys_krhino_sem_give_stub(void *arg)
{
    krhino_sem_give_syscall_arg_t *_arg = arg;

    return krhino_sem_give(_arg->sem);
}

static kstat_t sys_krhino_sem_take_stub(void *arg)
{
    krhino_sem_take_syscall_arg_t *_arg = arg;

    return krhino_sem_take(_arg->sem,
                           _arg->ticks);
}

/* ------------------ buf queue -------------------*/

static kstat_t sys_krhino_buf_queue_create_stub(void *arg)
{
    krhino_buf_queue_create_syscall_arg_t *_arg = arg;

    return krhino_buf_queue_create(_arg->queue,
                                   _arg->name,
                                   _arg->buf,
                                   _arg->size,
                                   _arg->max_msg);
}

static kstat_t sys_krhino_buf_queue_del_stub(void *arg)
{

    krhino_buf_queue_del_syscall_arg_t *_arg = arg;

    return krhino_buf_queue_del(_arg->queue);
}

static kstat_t sys_krhino_buf_queue_dyn_create_stub(void *arg)
{

    krhino_buf_queue_dyn_create_syscall_arg_t *_arg = arg;

    return krhino_buf_queue_dyn_create(_arg->queue,
                                       _arg->name,
                                       _arg->size,
                                       _arg->max_msg);
}

static kstat_t sys_krhino_buf_queue_dyn_del_stub(void *arg)
{

    krhino_buf_queue_dyn_del_syscall_arg_t *_arg = arg;

    return krhino_buf_queue_dyn_del(_arg->queue);
}

static kstat_t sys_krhino_buf_queue_send_stub(void *arg)
{

    krhino_buf_queue_send_syscall_arg_t *_arg = arg;

    return krhino_buf_queue_send(_arg->queue,
                                 _arg->msg,
                                 _arg->size);
}

static kstat_t sys_krhino_buf_queue_recv_stub(void *arg)
{

    krhino_buf_queue_recv_syscall_arg_t *_arg = arg;

    return krhino_buf_queue_recv(_arg->queue,
                                 _arg->ticks,
                                 _arg->msg,
                                 _arg->size);
}

static kstat_t sys_krhino_buf_queue_flush_stub(void *arg)
{

    krhino_buf_queue_flush_syscall_arg_t *_arg = arg;

    return krhino_buf_queue_flush(_arg->queue);
}

static kstat_t sys_krhino_buf_queue_info_get_stub(void *arg)
{

    krhino_buf_queue_info_get_syscall_arg_t *_arg = arg;

    return krhino_buf_queue_info_get(_arg->queue,
                                     _arg->info);
}


/* ------------------- mm --------------------- */

static void *sys_krhino_mm_alloc_stub(void *arg)
{
    krhino_mm_alloc_syscall_arg_t *_arg = arg;

    return krhino_mm_alloc(_arg->size);
}

static void *sys_krhino_mm_free_stub(void *arg)
{
    krhino_mm_free_syscall_arg_t *_arg = arg;

    krhino_mm_alloc(_arg->ptr);
}

static void *sys_krhino_mm_realloc_stub(void *arg)
{
    krhino_mm_realloc_syscall_arg_t *_arg = arg;

    return krhino_mm_realloc(_arg->oldmem,
                           _arg->newsize);
}


/* ----------------- hal uart ------------------- */

static int32_t sys_hal_uart_send_stub(void *arg)
{
    hal_uart_send_syscall_arg_t *_arg = arg;

    return hal_uart_send(_arg->uart,
                         _arg->data,
                         _arg->size,
                         _arg->timeout);
}


/* -------------------- vfs ---------------------- */

static off_t sys_aos_lseek_stub(void *arg)
{
    aos_lseek_syscall_arg_t *_arg = arg;

    return aos_lseek(_arg->fd,
                     _arg->offset,
                     _arg->whence);
}

static int sys_aos_close_stub(void *arg)
{
    aos_close_syscall_arg_t *_arg = arg;

    return aos_close(_arg->fd);
}

static ssize_t sys_aos_read_stub(void *arg)
{
    aos_read_syscall_arg_t *_arg = arg;

    return aos_read(_arg->fd,
                    _arg->buf,
                    _arg->nbytes);
}

static ssize_t sys_aos_write_stub(void *arg)
{
    aos_read_syscall_arg_t *_arg = arg;

    return aos_write(_arg->fd,
                    _arg->buf,
                    _arg->nbytes);
}


/**************************************************************
 *   syscall table
 *************************************************************/
void *syscall_tbl[] = {
    /* --------------------- task ------------------------*/
    [SYS_KRHINO_CUR_TASK_GET] = sys_krhino_cur_task_get_stub,   // 0
    [SYS_KRHINO_TASK_SLEEP] = sys_krhino_task_sleep_stub,       // 1
    [SYS_KRHINO_TASK_DEL] = sys_krhino_task_del_stub,           // 2
    [SYS_KRHINO_UTASK_CREATE] = sys_krhino_utask_create_stub,   // 3

    /* --------------------- time ------------------------*/
    [SYS_KRHINO_SYS_TIME_GET] = sys_krhino_sys_time_get_stub,   // 4
    [SYS_KRHINO_SYS_TICK_GET] = sys_krhino_sys_tick_get_stub,   // 5
    [SYS_KRHINO_MS_TO_TICKS] = sys_krhino_ms_to_ticks_stub,     // 6
    [SYS_KRHINO_TICKS_TO_MS] = sys_krhino_ticks_to_ms_stub,     // 7

    /* --------------------- mutex -----------------------*/
    [SYS_KRHINO_MUTEX_CREATE] = sys_krhino_mutex_create_stub,   // 8
    [SYS_KRHINO_MUTEX_DEL] = sys_krhino_mutex_del_stub,         // 9
    [SYS_KRHINO_MUTEX_LOCK] = sys_krhino_mutex_lock_stub,       // 10
    [SYS_KRHINO_MUTEX_UNLOCK] = sys_krhino_mutex_unlock_stub,   // 11

    /* -------------------- semphore ----------------------*/
    [SYS_KRHINO_SEM_CREATE] = sys_krhino_sem_create_stub,       // 12
    [SYS_KRHINO_SEM_DEL] = sys_krhino_sem_del_stub,             // 13
    [SYS_KRHINO_SEM_TAKE] = sys_krhino_sem_take_stub,           // 14
    [SYS_KRHINO_SEM_GIVE] = sys_krhino_sem_give_stub,           // 15

    /* --------------------buf queue ----------------------*/
    [SYS_KRHINO_BUF_QUEUE_CREATE] = sys_krhino_buf_queue_create_stub,
    [SYS_KRHINO_BUF_QUEUE_DEL] = sys_krhino_buf_queue_del_stub,
    [SYS_KRHINO_BUF_QUEUE_DYN_CREATE] = sys_krhino_buf_queue_dyn_create_stub,
    [SYS_KRHINO_BUF_QUEUE_DYN_DEL] = sys_krhino_buf_queue_dyn_del_stub,
    [SYS_KRHINO_BUF_QUEUE_SEND] = sys_krhino_buf_queue_send_stub,
    [SYS_KRHINO_BUF_QUEUE_RECV] = sys_krhino_buf_queue_recv_stub,
    [SYS_KRHINO_BUF_QUEUE_FLUSH] = sys_krhino_buf_queue_flush_stub,
    [SYS_KRHINO_BUF_QUEUE_INFO_GET] = sys_krhino_buf_queue_info_get_stub,

    /* ---------------------- mm -------------------------*/
    [SYS_KRHINO_MM_ALLOC] = sys_krhino_mm_alloc_stub,
    [SYS_KRHINO_MM_FREE] = sys_krhino_mm_free_stub,
    [SYS_KRHINO_MM_REALLOC] = sys_krhino_mm_realloc_stub,

    /* ------------------- hal uart ----------------------*/
    /*
    [SYS_HAL_UART_INIT] = sys_hal_uart_init_stub,
    */
    [SYS_HAL_UART_SEND] = sys_hal_uart_send_stub,
    /*
    [SYS_HAL_UART_RECV] = sys_hal_uart_recv_stub,
    [SYS_HAL_UART_RECV_II] = sys_hal_uart_recv_II_stub,
    [SYS_HAL_UART_FINALISE] = sys_hal_uart_finalize_stub,
    */

    /* ---------------- vfs ------------------*/
    [SYS_AOS_LSEEK] = sys_aos_lseek_stub,
    [SYS_AOS_CLOSE] = sys_aos_close_stub,
    [SYS_AOS_READ] = sys_aos_read_stub,
    [SYS_AOS_WRITE] = sys_aos_write_stub,

    NULL
};

