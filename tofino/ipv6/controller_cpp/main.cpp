#include "LocalClient.h"

#include <stdio.h>
#include <unistd.h>
#include <getopt.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <bf_rt/bf_rt_common.h>
#include <bf_switchd/bf_switchd.h>
#ifdef __cplusplus
}
#endif

#define SDE_INSTALL "/home/p4user/bf-sde-9.13.4/install"
#define CONF_FILE_DIR "share/p4/targets/tofino"
#define CONF_FILE_PATH(prog) \
    SDE_INSTALL "/" CONF_FILE_DIR "/" prog ".conf"

#define OPT_INTERVAL 0
#define OPT_GLOBAL_TABLE_SIZE 1
#define OPT_ALPHA 7
#define OPT_MONITORED 8
#define OPT_OUTGOING 9
#define OPT_INCOMING 10

using namespace std;
using namespace bfrt;

Args* parse_options(int argc, char **argv){
    int option_index = 0;
    Args* args = new Args;

    static struct option options[] = {
        {"interval", required_argument, 0, OPT_INTERVAL},
        {"global-table-size", required_argument, 0, OPT_GLOBAL_TABLE_SIZE},
        {"alpha", required_argument, 0, OPT_ALPHA},
        {"monitored", required_argument, 0, OPT_MONITORED},
        {"outgoing", required_argument, 0, OPT_OUTGOING},
        {"incoming", required_argument, 0, OPT_INCOMING},
        {NULL, 0, 0, 0}
    };

    bool incoming_ports = false;
    bool outgoing_ports = false;

    while(1){
        int opt = getopt_long(argc, argv, "", options, &option_index);

        if(opt == -1){
            break;
        }

        switch(opt){
            case OPT_INTERVAL:
                args->time_interval = atoi(optarg);
                break;
            case OPT_GLOBAL_TABLE_SIZE:
                args->global_table_size = atoi(optarg);
                break;
            case OPT_ALPHA:
                args->alpha = atoi(optarg);
                break;
            case OPT_MONITORED:
                args->monitored_path = string(optarg);
                break;
            case OPT_OUTGOING:
                if (!outgoing_ports) { 
                    outgoing_ports = true;
                    args->outgoing.clear();
                }
                args->outgoing.push_back((uint16_t) atoi(optarg));
                break;
            case OPT_INCOMING:
                if (!incoming_ports) { 
                    incoming_ports = true;
                    args->incoming.clear();
                }
                args->incoming.push_back((uint16_t) atoi(optarg));
                break;
            default:
                printf("Invalid option\n");
                break;
        }
    }

    return args;
}

int main(int argc, char **argv){
    /* Shared API variables */
    bf_rt_target_t dev_tgt;
    shared_ptr<BfRtSession> session;
    const BfRtInfo *bf_rt_info = nullptr;

    bf_switchd_context_t *switchd_ctx;
    bf_status_t bf_status;

    /* Check if root privileges exist or not, exit if not. */
    if (geteuid() != 0) {
        printf("This program must be run as root user \n");
        exit(1);
    }

    /* Allocate memory for the libbf_switchd context. */
    switchd_ctx = (bf_switchd_context_t *)calloc(1, sizeof(bf_switchd_context_t));
    if (!switchd_ctx) {
        printf("Cannot Allocate switchd context\n");
        exit(1);
    }

    /* Initialize the switchd context */
    switchd_ctx->install_dir           = strdup(SDE_INSTALL);
    switchd_ctx->conf_file             = strdup(CONF_FILE_PATH(PROG_NAME));
    switchd_ctx->running_in_background = true; // no cli
    switchd_ctx->dev_sts_thread        = true; 
    switchd_ctx->dev_sts_port          = 7777; //INIT_STATUS_TCP_PORT;

    /* Setup the device */
    memset(&dev_tgt, 0, sizeof(dev_tgt));
    dev_tgt.dev_id = 0;
    dev_tgt.pipe_id = BF_DEV_PIPE_ALL;

    /* Initialize the device */
    bf_status = bf_switchd_lib_init(switchd_ctx);
    if (bf_status != BF_SUCCESS) {
        printf("Failed to initialize device: %s\n", bf_err_str(bf_status));
        free(switchd_ctx);
        exit(1);
    }

    /* Initialize the BrRt session */
    session = BfRtSession::sessionCreate();
    if (session == nullptr) {
        printf("Failed to establish BfRt session.\n");
        free(switchd_ctx);
        exit(1);
    }

    /* Retrieve BfRtInfo */
    auto &dev_mgr = BfRtDevMgr::getInstance();
    bf_status = dev_mgr.bfRtInfoGet(dev_tgt.dev_id, "telescope", &bf_rt_info);
    bf_sys_assert(bf_status == BF_SUCCESS);
    
    Args* args = parse_options(argc, argv);
    printf("Parsed options\n");
    
    LocalClient *local_client = new LocalClient(args, session, dev_tgt, bf_rt_info);
    local_client->run();

    /* Destroy session */
    bf_status = session->sessionDestroy();
    bf_sys_assert(bf_status == BF_SUCCESS);

    if (switchd_ctx) free(switchd_ctx);

    return bf_status;
}