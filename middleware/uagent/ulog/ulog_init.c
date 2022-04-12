#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "ulog/ulog.h"
#include "k_api.h"
#include "aos/cli.h"

bool log_init = false;


#ifdef AOS_COMP_CLI

#ifdef ULOG_CONFIG_ASYNC

extern void on_filter_level_change(const SESSION_TYPE session, const uint8_t level);
extern void ulog_async_init(const uint8_t host_name[8]);


__attribute__((weak)) void update_net_cli(const char cmd, const char* param)
{
    /* Will be overwrite in session_udp, as this implement take effect only the UDP feature support,
       I don't like so many compile switcher in the code which result reader is not so comfortable,
       so weak attribute used here.
    */
}

static void cmd_cli_ulog(char *pwbuf, int blen, int argc, char *argv[])
{
    bool exit_loop = false;
    uint8_t session = 0xFF;
    uint8_t level   = 0xFF;
    uint8_t i;
	for (i = 1; i < argc && !exit_loop; i+=2)
	{
		const char* option = argv[i];
        const char* param  = argv[i+1];
        if(option!=NULL && param!=NULL && strlen(option)==1){
            switch(option[0])
            {
                case 's'://session
                session = strtoul(param, NULL, 10);
                break;

                case 'l'://level
                level = strtoul(param, NULL, 10);
                break;

                case 'a'://syslog watcher address
                case 'p'://syslog watcher port
                update_net_cli(option[0], param);

                    break;

                default: //unknown option
                exit_loop = true;
                break;
            }
        }else{//unknown format
            break;
        }
	}

    if( (session<SESSION_CNT) && (level<=LOG_NONE) ){
        on_filter_level_change(session, level);
    }
}
#endif

static void sync_log_cmd(char *buf, int len, int argc, char **argv)
{
    const char *lvls[] = {
        [AOS_LL_FATAL] = "fatal", [AOS_LL_ERROR] = "error",
        [AOS_LL_WARN] = "warn",   [AOS_LL_INFO] = "info",
        [AOS_LL_DEBUG] = "debug",
    };

    if (argc < 2) {
        aos_cli_printf("log level : %02x\r\n", aos_get_log_level());
        return;
    }

    int i;
    for (i = 0; i < sizeof(lvls) / sizeof(lvls[0]); i++) {
        if (strncmp(lvls[i], argv[1], strlen(lvls[i]) + 1) != 0) {
            continue;
        }

        aos_set_log_level((aos_log_level_t)i);
        aos_cli_printf("set log level success\r\n");
        return;
    }
    aos_cli_printf("set log level fail\r\n");
}

static struct cli_command ulog_cmd[] = {
    { "loglevel","set sync log level",sync_log_cmd },
#ifdef ULOG_CONFIG_ASYNC
    { "ulog", "ulog [option param]",cmd_cli_ulog },
#endif
};

#endif /* AOS_COMP_CLI */

void ulog_init(const uint8_t host_name[8])
{
    if(!log_init){
        log_init_mutex();
#ifdef ULOG_CONFIG_ASYNC
        ulog_async_init(host_name);
#endif
#ifdef AOS_COMP_CLI
        aos_cli_register_commands(&ulog_cmd[0], sizeof(ulog_cmd)/sizeof(struct cli_command));
#endif
        log_init = true;
    }
}

