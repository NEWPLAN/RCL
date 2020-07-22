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
static int msg_size = 0;                        // size of message (in byte)
static int duration = 0;                        // transmission duration in second            

int main(int argc, char **argv)
{
        struct addrinfo *addr;
        struct rdma_cm_event *event = NULL;
        struct rdma_cm_id *conn= NULL;
        struct rdma_event_channel *ec = NULL;

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
        msg_size = atoi(argv[argc - 2]);
        duration = atoi(argv[argc - 1]);

        if (msg_size <= 0 || msg_size > BUFFER_SIZE) {
                fprintf(stderr, "Message size should be between 1 and %d\n", BUFFER_SIZE);
                result = false;
        }

        if (duration <= 0) {
                fprintf(stderr, "Transmittion duration (second) must be larger than 0\n");
                result = false;
        }

        if (!result) {
                usage(argv[0]);
        }

        return result;                
}

void usage(char *program)
{
        fprintf(stderr, "Usage: %s [-poll] [-h] [-v] server_ip server_port message_size duration\n", program);
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

        conn->recv_mr = NULL;
        conn->send_mr = NULL;
        conn->recv_region = NULL;
        conn->send_region = NULL;

        conn->msg_size = msg_size;
        conn->bytes_sent = 0;
        conn->bytes_recv = 0;

        register_send_memory(conn, s_ctx->pd);
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
        struct timeval now, elapsed_last_update, elapsed_start;
        struct connection *conn = (struct connection *)(uintptr_t)wc->wr_id;

        if (wc->status != IBV_WC_SUCCESS) {
                fprintf(stderr, "on_completion: ERROR\n");
                fprintf(stderr, "       WC status %d (%s)\n", wc->status, ibv_wc_status_str(wc->status));
                fprintf(stderr, "       WC opcode %d\n", wc->opcode);
                exit(EXIT_FAILURE);
        }

        if (wc->opcode == IBV_WC_SEND) {
                conn->bytes_sent += conn->msg_size;
                if (verbose_mode) {
                        printf("Send %d bytes\n", conn->msg_size);
                }

                gettimeofday(&now, NULL);
                timersub(&now, &(conn->last_update_time), &elapsed_last_update);
                
                // More than 1 second since last update        
                if (elapsed_last_update.tv_sec >= 1) {
                        double throughput = (double)conn->bytes_sent * 8 / 1000 / 
                        (elapsed_last_update.tv_sec * 1000000 + elapsed_last_update.tv_usec);
                        
                        printf("Throughput: %.2f Gbps\n", throughput);
                        conn->bytes_sent = 0;
                        conn->last_update_time = now;
                }

                timersub(&now, &(conn->start_time), &elapsed_start);

                // Exceed transmission duration time
                if (elapsed_start.tv_sec >= duration) {

                        // Print the last second throughput result
                        if (conn->bytes_sent != 0) {
                                double throughput = (double)conn->bytes_sent * 8 / 1000 / 
                                (elapsed_last_update.tv_sec * 1000000 + elapsed_last_update.tv_usec);
                                
                                printf("Throughput: %.2f Gbps\n", throughput);
                                conn->bytes_sent = 0;
                        }

                        rdma_disconnect(conn->id);                
                } else {
                        post_send(conn);    
                }

        } else {
                die("on_completion: opcode is not IBV_WC_SEND.");
        }
}

int on_connection(void *context)
{       
        struct connection *conn = (struct connection*)context;
        struct timeval now;
        
        //sleep(1);
        gettimeofday(&now, NULL);
        post_send(conn);
        
        conn->last_update_time = now;
        conn->start_time = now;
        printf("connection established\n");
        return 0;
}

int on_disconnect(struct rdma_cm_id *id)
{
        struct connection *conn = (struct connection *)id->context;

        printf("disconnect\n");

        rdma_destroy_qp(id);
        // As the sender, we only need to free send memory
        free_send_memory(conn);

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