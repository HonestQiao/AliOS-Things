/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <stdint.h>
#include "k_api.h"
#include <hal/soc/soc.h>
#include <aos/aos.h>

extern void os_crosscore_int_init();
extern void cpu_first_task_start(void);

#define MAX_CPUS 4

#if (RHINO_CONFIG_CPU_NUM > MAX_CPUS)
#error core num error RHINO_CONFIG_CPU_NUM > MAX_CPUS
#endif

static unsigned int num_cpus = RHINO_CONFIG_CPU_NUM;


void configure_cpu(uint32_t cpu)
{
    const unsigned int all_ways = 0xf;

    disable_strict_align_check();

    // Enable branch prediction
    arm_branch_target_cache_invalidate();
    arm_branch_prediction_enable();

    // Enable L1 caches
    arm_dcache_enable();
    arm_dcache_invalidate();
    arm_icache_enable();
    arm_icache_invalidate();

    // Invalidate SCU copy of TAG RAMs
    scu_secure_invalidate(cpu, all_ways);

    // Join SMP
    scu_join_smp();
    scu_enable_maintenance_broadcast();
}

void smp_cpu_init(void)
{
    uint32_t cpu_id = cpu_get_current();

    if (cpu_id == 0)
    {
        int cpu_count = cpu_get_cores();

        /*
        if (num_cpus > cpu_count)
        {
            num_cpus = cpu_count;
        }*/

        num_cpus = (cpu_count >= num_cpus ? num_cpus : cpu_count);
        
        printf("Using %d CPU core(s)\n", num_cpus);

        scu_enable();
        configure_cpu(cpu_id);

    }
    else
    {
        platform_init();
        configure_cpu(cpu_id);

        aos_hw_vector_init();
        isr_stk_init();

        /*int cross core init*/
        os_crosscore_int_init();
        
#if (RHINO_CONFIG_CPU_NUM > 1)
        extern kspinlock_t g_smp_printlock;
        krhino_spin_lock(&g_smp_printlock);
#endif

        printf("Starting scheduler on CPU:%d.\r\n",cpu_id);
#if (RHINO_CONFIG_CPU_NUM > 1)
        krhino_spin_unlock(&g_smp_printlock); 
#endif

        cpu_first_task_start();        
    }
   
}


void os_load_slavecpu(void)
{
    int i;
    os_crosscore_int_init();
    
    // start all cpus
    for (i = 1; i < num_cpus; i++)
    {
        cpu_start_secondary(i, &smp_cpu_init, 0);
    }

    k_wait_allcores();
    
}

