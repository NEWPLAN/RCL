#ifndef COMMON_H
#define COMMON_H

#include <pthread.h>
#include <rdma/rdma_cma.h>
#include <sys/time.h>

#define BUFFER_SIZE 10485760    
#define MAX_RECV_WR 1000
#define MAX_SEND_WR 1

#define TEST_NZ(x) do { if ((x)) die("error: " #x " failed (returned non-zero)." ); } while (0)
#define TEST_Z(x)  do { if (!(x)) die("error: " #x " failed (returned zero/null)."); } while (0)


struct context 
{
	struct ibv_context *ctx;
	struct ibv_pd *pd;                      // protection domain
	struct ibv_cq *cq;			// completion queue
	struct ibv_comp_channel *comp_channel;	// completion channel

	pthread_t cq_poller_thread;             // thread to poll completion events
};

// structure of a RDMA connection
struct connection 
{
        struct rdma_cm_id *id;
        struct ibv_qp *qp;			// queue pair

        struct ibv_mr *recv_mr;         	// receive memory region
        struct ibv_mr *send_mr;         	// send memory region

        char *recv_region;              	// receive buffer
        char *send_region;              	// send buffer

        unsigned int msg_size;      
        unsigned long long bytes_sent;  
        unsigned long long bytes_recv;  

	struct timeval start_time;
        struct timeval last_update_time;
};

// print error information and exit
void die(const char *reason);

// Initialize Queue Pair Attributions. We need the desired completion queue as the argument.
void build_qp_attr(struct ibv_qp_init_attr *qp_attr, struct ibv_cq *cq);

// Register send memory of a connection. We need the protection domain as the argument.
void register_send_memory(struct connection *conn, struct ibv_pd *pd);

// Register receive memory of a connection. We need the protection domain as the argument.
void register_recv_memory(struct connection *conn, struct ibv_pd *pd);

// Free send memory of a connection
void free_send_memory(struct connection *conn);

// Free receive memory of a connection
void free_recv_memory(struct connection *conn);

// Post a send operation to the send queue
void post_send(struct connection *conn);

// Post a receive operation to the receive queue
void post_receive(struct connection *conn);

#endif