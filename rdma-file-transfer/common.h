#ifndef RDMA_COMMON_H
#define RDMA_COMMON_H

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rdma/rdma_cma.h>
#include <glog/logging.h>

#define TEST_NZ(x)                                               \
    do                                                           \
    {                                                            \
        if ((x))                                                 \
            rc_die("error: " #x " failed (returned non-zero)."); \
    } while (0)
#define TEST_Z(x)                                                 \
    do                                                            \
    {                                                             \
        if (!(x))                                                 \
            rc_die("error: " #x " failed (returned zero/null)."); \
    } while (0)

struct context
{
    struct ibv_context *ctx;
    struct ibv_pd *pd;
    struct ibv_cq *cq;
    struct ibv_comp_channel *comp_channel;

    pthread_t cq_poller_thread;
};

#define MAX_FILE_NAME 256

struct SessionContext
{
    char *buffer;
    struct ibv_mr *buffer_mr;

    struct message *msg;
    struct ibv_mr *msg_mr;

    uint64_t peer_addr;
    uint32_t peer_rkey;

    struct context *qp_ctx;

    char ip_addr[MAX_FILE_NAME];
};

typedef void (*pre_conn_cb_fn)(struct rdma_cm_id *id);
typedef void (*connect_cb_fn)(struct rdma_cm_id *id);
typedef void (*completion_cb_fn)(struct ibv_wc *wc);
typedef void (*disconnect_cb_fn)(struct rdma_cm_id *id);

void rc_init(pre_conn_cb_fn, connect_cb_fn, completion_cb_fn, disconnect_cb_fn);
void rc_client_loop(const char *host, const char *port, void *context);
void rc_disconnect(struct rdma_cm_id *id);
void rc_die(const char *message);
struct ibv_pd *rc_get_pd(struct SessionContext *ctx);
void rc_server_loop(const char *port);

#endif
