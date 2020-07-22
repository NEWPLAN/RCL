#ifndef COMMON_H
#define COMMON_H

#include <pthread.h>
#include <rdma/rdma_cma.h>

#define BUFFER_SIZE 1024

#define TEST_NZ(x) do { if ((x)) die("error: " #x " failed (returned non-zero)." ); } while (0)
#define TEST_Z(x)  do { if (!(x)) die("error: " #x " failed (returned zero/null)."); } while (0)

struct context 
{
        struct ibv_context *ctx;
        struct ibv_pd *pd;                      // protection domain
        struct ibv_cq *cq;                      // completion queue
        struct ibv_comp_channel *comp_channel;  // completion channel

        pthread_t cq_poller_thread;             // thread to poll completion events
};

// structure of a RDMA connection
struct connection 
{
        struct rdma_cm_id *id;
        struct ibv_qp *qp;              // queue pair

        struct ibv_mr *recv_mr;         // receive memory region
        struct ibv_mr *send_mr;         // send memory region

        char *recv_region;              // receive buffer
        char *send_region;              // send buffer

        int num_completed_send;         // # of completed SEND operations 
        int num_completed_recv;         // # of completed RECEIVE operations

        int msg_size;                   // # of message size exchanged in this connection
};

// print error information and exit
void die(const char *reason);

// Initialize Queue Pair Attributions. We need the desired completion queue as the argument.
void build_qp_attr(struct ibv_qp_init_attr *qp_attr, struct ibv_cq *cq);

// Register send/receive memory of a connection. We need the protection domain as the argument.
void register_memory(struct connection *conn, struct ibv_pd *pd);

// Post a send operation to the send queue
void post_send(struct connection *conn);

// Post a receive operation to the receive queue
void post_receive(struct connection *conn);

#endif