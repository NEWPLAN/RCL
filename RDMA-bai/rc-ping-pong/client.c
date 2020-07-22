#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <rdma/rdma_cma.h>
#include "common.h"

typedef enum { false, true } bool;

// Parse arguments 
bool parse_arg(int argc, char **argv);
// Print the usage of the program
void usage(char *program);

static void build_context(struct ibv_context *verbs);
static void destroy_context();
static void * poll_cq(void *);

static int on_addr_resolved(struct rdma_cm_id *id);
static int on_connection(void *context);
static int on_disconnect(struct rdma_cm_id *id);
static int on_event(struct rdma_cm_event *event);
static int on_route_resolved(struct rdma_cm_id *id);
static void on_completion(struct ibv_wc *wc);

static bool busy_poll_cq = false;
static bool verbose_mode = false;

static struct context *s_ctx = NULL;
static int TIMEOUT_IN_MS = 500; 
static char *ip;                                // IP address of destination server
static char *port;                              // Port number of destination server
static int count = 0;                           // # of message exchanges
static int msg_size = 0;                        // size of message to exchange 
static struct timeval tv_ping_sent;             // time when the ping message is sent
static struct timeval tv_ping_acked;            // time when the ping message is acked
static struct timeval tv_pong_recv;             // time when the pong message is received
static unsigned int ping_rtt_sum;               // sum of ping RTTs (in microsecond)             
static unsigned int pingpong_rtt_sum;           // sum of ping pong RTTs (in microsecond)

int main(int argc, char **argv)
{
        struct addrinfo *addr;
        struct rdma_cm_event *event = NULL;
        struct rdma_cm_id *conn= NULL;
        struct rdma_event_channel *ec = NULL;
        ping_rtt_sum = 0;
        pingpong_rtt_sum = 0;

        if (!parse_arg(argc, argv)) {
                return EXIT_FAILURE;
        } 

        TEST_NZ(getaddrinfo(ip, port, NULL, &addr));

        // Open an event channel used to report communication events. 
        TEST_Z(ec = rdma_create_event_channel());

        TEST_NZ(rdma_create_id(ec, &conn, NULL, RDMA_PS_TCP));

        // Resolve destination and optional source addresses from IP addresses to an RDMA address. 
        // If successful, conn will be bound to a local device.
        TEST_NZ(rdma_resolve_addr(conn, NULL, addr->ai_addr, TIMEOUT_IN_MS));
        freeaddrinfo(addr);

        // Retrieve a communication event. 
        while (rdma_get_cm_event(ec, &event) == 0) {
                struct rdma_cm_event event_copy;
                memcpy(&event_copy, event, sizeof(*event));

                // Acknowledge and free a communication event
                rdma_ack_cm_event(event);

                // Process the event
                if (on_event(&event_copy) != 0)
                        break;
        }

        rdma_destroy_event_channel(ec);
        destroy_context();   
        
        printf("Average ping RTT:        %.2f us\n", (double)ping_rtt_sum / count);
        printf("Average ping pong RTT:   %.2f us\n", (double)pingpong_rtt_sum / count);
        
        return 0;
}

// Parse arguments 
bool parse_arg(int argc, char **argv)
{
        bool result = true;

        if (argc < 5) {
                usage(argv[0]);
                return false;
        }

        for (int i = 1; i < argc - 4; i++) {
                if (strcmp(argv[i], "-h") == 0) {
                        usage(argv[0]);
                        return false;

                } else if (strcmp(argv[i], "-poll") == 0) {
                        busy_poll_cq = true;
                
                } else if (strcmp(argv[i], "-v") == 0) {
                        verbose_mode = true;

                } else {
                        fprintf(stderr, "Unkown option: %s\n", argv[1]);
                        usage(argv[0]);
                        return false;
                }
        }
       
        ip = argv[argc - 4];
        port = argv[argc - 3];
        count = atoi(argv[argc - 2]);
        msg_size = atoi(argv[argc - 1]);

        if (count <= 0) {
                fprintf(stderr, "Count should be larger than 0\n");
                result = false;
        }

        if (msg_size <= 0 || msg_size > BUFFER_SIZE) {
                fprintf(stderr, "Message size should be between 1 and %d\n", BUFFER_SIZE);
                result = false;
        }

        if (!result) {
                usage(argv[0]);
        }

        return result;                
}

void usage(char *program)
{
        fprintf(stderr, "Usage: %s [-poll] [-h] [-v] server_ip server_port count message_size\n", program);
        fprintf(stderr, "    -poll: polling completion queue\n");
        fprintf(stderr, "    -h:    help infomration\n");
        fprintf(stderr, "    -v:    verbose mode\n");

}

int on_event(struct rdma_cm_event *event)
{
        int r = 0;

        if (event->event == RDMA_CM_EVENT_ADDR_RESOLVED) {
                r = on_addr_resolved(event->id);
        
        } else if (event->event == RDMA_CM_EVENT_ROUTE_RESOLVED) {
                r = on_route_resolved(event->id);
        
        // A connection has been established with the remote end point        
        } else if (event->event == RDMA_CM_EVENT_ESTABLISHED) {
                r = on_connection(event->id->context);

        // The connection between the local and remote devices has been disconnected.        
        } else if (event->event == RDMA_CM_EVENT_DISCONNECTED) {
                r = on_disconnect(event->id);

        } else {
                die("on_event: unknown event.");
        }

        return r;
}

int on_addr_resolved(struct rdma_cm_id *id)
{
        struct ibv_qp_init_attr qp_attr;
        struct connection *conn;

        build_context(id->verbs);
        build_qp_attr(&qp_attr, s_ctx->cq);

        TEST_NZ(rdma_create_qp(id, s_ctx->pd, &qp_attr));
        id->context = conn = (struct connection *)malloc(sizeof(struct connection));
        
        conn->id = id;
        conn->qp = id->qp;
        conn->num_completed_send = 0;
        conn->num_completed_recv = 0;
        conn->msg_size = msg_size;

        register_memory(conn, s_ctx->pd);
        post_receive(conn);

        TEST_NZ(rdma_resolve_route(id, TIMEOUT_IN_MS));

        return 0;
}

void build_context(struct ibv_context *verbs)
{
        s_ctx = (struct context *)malloc(sizeof(struct context));
        s_ctx->ctx = verbs;

        TEST_Z(s_ctx->pd = ibv_alloc_pd(s_ctx->ctx));

        // For busy polling mode, we don't need a completion channel
        if (busy_poll_cq) {     
                TEST_Z(s_ctx->cq = ibv_create_cq(s_ctx->ctx, 10, NULL, NULL, 0));
        
        } else {
                TEST_Z(s_ctx->comp_channel = ibv_create_comp_channel(s_ctx->ctx));
                TEST_Z(s_ctx->cq = ibv_create_cq(s_ctx->ctx, 10, NULL, s_ctx->comp_channel, 0));
                TEST_NZ(ibv_req_notify_cq(s_ctx->cq, 0));
        }

        TEST_NZ(pthread_create(&s_ctx->cq_poller_thread, NULL, poll_cq, NULL));
}

void destroy_context()
{
        void *res;

        // Stop the polling thread
        TEST_NZ(pthread_cancel(s_ctx->cq_poller_thread));
        TEST_NZ(pthread_join(s_ctx->cq_poller_thread, &res));

        TEST_NZ(ibv_destroy_cq(s_ctx->cq));        
       
        if (!busy_poll_cq) {
                TEST_NZ(ibv_destroy_comp_channel(s_ctx->comp_channel));
        } 
        
        TEST_NZ(ibv_dealloc_pd(s_ctx->pd));
        free(s_ctx);
}

void* poll_cq(void *ctx)
{
        struct ibv_cq *cq;
        struct ibv_wc wc;
        int oldtype;

        TEST_NZ(pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldtype));

        if (busy_poll_cq) {
                cq = s_ctx->cq;
                while (1) {
                        if (ibv_poll_cq(cq, 1, &wc) > 0) {
                                on_completion(&wc); 
                        }        
                }
        } else {
                while (1) {
                        TEST_NZ(ibv_get_cq_event(s_ctx->comp_channel, &cq, &ctx));
                        ibv_ack_cq_events(cq, 1);
                        TEST_NZ(ibv_req_notify_cq(cq, 0));

                        while (ibv_poll_cq(cq, 1, &wc) > 0) {
                                on_completion(&wc); 
                        }
                }
        }

        return NULL;
}

void on_completion(struct ibv_wc *wc)
{
        unsigned int time_elapsed;
        struct connection *conn = (struct connection *)(uintptr_t)wc->wr_id;

        if (wc->status != IBV_WC_SUCCESS) {
                die("on_completion: status is not IBV_WC_SUCCESS.");
        }

        if (wc->opcode & IBV_WC_RECV) {
                // record the pong received time
                gettimeofday(&tv_pong_recv, NULL);
                conn->num_completed_recv++;
                
                if (verbose_mode) {
                        printf("complete receive operation %d\n", conn->num_completed_recv);
                }
                
                // calculate RTT result
                time_elapsed = (tv_pong_recv.tv_sec - tv_ping_sent.tv_sec) * 1000000;
                time_elapsed += (tv_pong_recv.tv_usec - tv_ping_sent.tv_usec);
                printf("ping pong RTT: %u us\n", time_elapsed);
                pingpong_rtt_sum += time_elapsed;

                time_elapsed = (tv_ping_acked.tv_sec - tv_ping_sent.tv_sec) * 1000000;
                time_elapsed += (tv_ping_acked.tv_usec - tv_ping_sent.tv_usec);
                printf("ping RTT: %u us\n", time_elapsed);
                ping_rtt_sum += time_elapsed;

                // We have received enough # of responses from the server. Terminate the connection!
                // Return 1 to terminate the polling thread.
                if (conn->num_completed_recv >= count) {
                        rdma_disconnect(conn->id);
                
                // Start the next round of communication
                } else {
                        post_receive(conn);
                        gettimeofday(&tv_ping_sent, NULL);
                        post_send(conn);    
                }

        } else if (wc->opcode == IBV_WC_SEND) {
                // record the ping acked time
                gettimeofday(&tv_ping_acked, NULL);
                conn->num_completed_send++;
                
                if (verbose_mode) {                
                        printf("complete send operation %d\n", conn->num_completed_send);
                }
        } else {
                die("on_completion: completion isn't a send or a receive.");
        }
}

int on_connection(void *context)
{       
        // record the ping sent time
        gettimeofday(&tv_ping_sent, NULL);
        // Post the first send operation
        post_send((struct connection *)context);
        return 0;
}

int on_disconnect(struct rdma_cm_id *id)
{
        struct connection *conn = (struct connection *)id->context;

        printf("disconnect\n");

        rdma_destroy_qp(id);

        ibv_dereg_mr(conn->send_mr);
        ibv_dereg_mr(conn->recv_mr);

        free(conn->send_region);
        free(conn->recv_region);

        free(conn);
        rdma_destroy_id(id);

        // To exit event loop
        return 1; 
}

int on_route_resolved(struct rdma_cm_id *id)
{
        struct rdma_conn_param cm_params;

        printf("route resolved\n");

        memset(&cm_params, 0, sizeof(cm_params));
        TEST_NZ(rdma_connect(id, &cm_params));

        return 0;
}