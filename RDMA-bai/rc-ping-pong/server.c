#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <rdma/rdma_cma.h>
#include "common.h"

#define PORT 12345

typedef enum { false, true } bool;

// Parse arguments 
bool parse_arg(int argc, char **argv);
// Print the usage of the program
void usage(char *program);

// To handle SIGINT signal
void sigint_handler(int sig);
void free_resource();

static void build_context(struct ibv_context *verbs);
static void destroy_context();
static void* poll_cq(void *);

static void on_completion(struct ibv_wc *wc);
static int on_connect_request(struct rdma_cm_id *id);
static int on_connection(void *context);
static int on_disconnect(struct rdma_cm_id *id); 
static int on_event(struct rdma_cm_event *event);

// If busy polling completion queue 
static bool busy_poll_cq = false;
static bool verbose_mode = false;

static struct context *s_ctx = NULL;
struct rdma_event_channel *ec = NULL;  
// Conceptually equivalent to a socket for RDMA communication
struct rdma_cm_id *listener = NULL;     

int main(int argc, char **argv)
{
        struct sockaddr_in addr;
        struct rdma_cm_event *event = NULL;
        uint16_t port = 0;
        
        if (!parse_arg(argc, argv)) {
                return EXIT_FAILURE;
        } 

        signal(SIGINT, sigint_handler);

        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(PORT);

        // Open an event channel used to report communication events. 
        TEST_Z(ec = rdma_create_event_channel());

        // Create an identifier that is used to track communication information
        // RDMA_PS_TCP means connection-oriented, reliable queue                        
        TEST_NZ(rdma_create_id(ec, &listener, NULL, RDMA_PS_TCP));
        
        // Associate a source address with an rdma_cm_id
        TEST_NZ(rdma_bind_addr(listener, (struct sockaddr *)&addr));

        // Listen for incoming connection requests
        TEST_NZ(rdma_listen(listener, 10));                             
 
        port = ntohs(rdma_get_src_port(listener));
        printf("listen on port %d\n", port);
        
        // Retrieve a communication event. 
        while (rdma_get_cm_event(ec, &event) == 0) {
                struct rdma_cm_event event_copy;
                memcpy(&event_copy, event, sizeof(*event));

                // Acknowledge and free a communication event
                rdma_ack_cm_event(event);

                // Process the event
                if (on_event(&event_copy)) {
                        break;
                }
        }
  
        return EXIT_SUCCESS;
}

// Parse arguments 
bool parse_arg(int argc, char **argv)
{
        int i = 1;

        while (i < argc) {
                if (strcmp(argv[i], "-h") == 0) {
                        usage(argv[0]);
                        return false;

                } else if (strcmp(argv[i], "-poll") == 0) {
                        busy_poll_cq = true;

                } else if (strcmp(argv[i], "-v") == 0) {
                        verbose_mode = true;
        
                } else {
                        fprintf(stderr, "Unkown option: %s\n", argv[i]);
                        usage(argv[0]);
                        return false;
                }
                i++;
        }

        return true;                
}

// Print the usage of the program
void usage(char *program)
{
        fprintf(stderr, "Usage: %s [-poll] [-v] [-h]\n", program);
        fprintf(stderr, "    -poll: polling completion queue\n");
        fprintf(stderr, "    -h:    help infomration\n");
        fprintf(stderr, "    -v:    verbose mode\n");
}

// To handle SIGINT signal
void sigint_handler(int sig)
{
        free_resource();
        exit(0);
}

void free_resource()
{        
        destroy_context();
        printf("desotry context\n");

        rdma_destroy_id(listener);
        rdma_destroy_event_channel(ec);
        printf("free resource\n");
}

// Process the incoming event
int on_event(struct rdma_cm_event *event)
{
        int r = 0;
        
        // A connection request has been received.
        if (event->event == RDMA_CM_EVENT_CONNECT_REQUEST) {
                r = on_connect_request(event->id);

        // A connection has been established with the remote end point.
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

int on_connect_request(struct rdma_cm_id *id)
{
        struct ibv_qp_init_attr qp_attr;
        struct rdma_conn_param cm_params;
        struct connection *conn;
 
        printf("\nreceive connection request\n");
        
        build_context(id->verbs);   
        build_qp_attr(&qp_attr, s_ctx->cq);                
        TEST_NZ(rdma_create_qp(id, s_ctx->pd, &qp_attr));       
 
        id->context = conn = (struct connection *)malloc(sizeof(struct connection));   
        conn->qp = id->qp;
        conn->num_completed_recv = 0;
        conn->num_completed_send = 0;
        // We use a upper bound here as we don't know the exact message size now. 
        conn->msg_size = BUFFER_SIZE;   

        register_memory(conn, s_ctx->pd);
        post_receive(conn);
        
        memset(&cm_params, 0, sizeof(cm_params));
        // Accept the connection
        TEST_NZ(rdma_accept(id, &cm_params));
 
        return 0;
}

void build_context(struct ibv_context *verbs)
{
        if (s_ctx) { 
                if (s_ctx->ctx != verbs) {
                        die("cannot handle events in more than one context.");
                }

                return;
        }
 
        s_ctx = (struct context *)malloc(sizeof(struct context));
        s_ctx->ctx = verbs;
        
        // Create a protection domain
        TEST_Z(s_ctx->pd = ibv_alloc_pd(s_ctx->ctx));

        // For busy polling mode, we don't need a completion channel
        if (busy_poll_cq) {
                TEST_Z(s_ctx->cq = ibv_create_cq(s_ctx->ctx, 10, NULL, NULL, 0));   
        } else {
                TEST_Z(s_ctx->comp_channel = ibv_create_comp_channel(s_ctx->ctx));            
                TEST_Z(s_ctx->cq = ibv_create_cq(s_ctx->ctx, 10, NULL, s_ctx->comp_channel, 0));
                TEST_NZ(ibv_req_notify_cq(s_ctx->cq, 0));
        }
        
        // Start a thread to poll the completion queue using function poll_cq 
        TEST_NZ(pthread_create(&s_ctx->cq_poller_thread, NULL, poll_cq, NULL)); 
}

void destroy_context()
{
        if (!s_ctx) {
                return;
        }
        
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

// Poll completion queue
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
                        // Wait for a notification to be sent on the indicated completion channel (CC)
                        // This is only a notification. It does not actually process the CQEs
                        TEST_NZ(ibv_get_cq_event(s_ctx->comp_channel, &cq, &ctx));

                        // Acknowledge an event received from ibv_get_cq_event
                        ibv_ack_cq_events(cq, 1);

                        // Re-arm the notification mechanism for the indicated completion queue
                        // Otherwise, the completion queue is “disarmed” and will not send any more notifications to the CC
                        TEST_NZ(ibv_req_notify_cq(cq, 0));

                        // Keep pulling events from the queue until none are left
                        while (ibv_poll_cq(cq, 1, &wc)) {
                                on_completion(&wc);
                        }
                }
        }

        return NULL;
}

// Print information of completion requests
void on_completion(struct ibv_wc *wc)
{
        struct connection *conn = (struct connection *)(uintptr_t)wc->wr_id;
        if (wc->status != IBV_WC_SUCCESS) {
                die("on_completion: status is not IBV_WC_SUCCESS.");
        }

        if (wc->opcode & IBV_WC_RECV) {
                conn->num_completed_recv++;
                
                if (verbose_mode) {
                        printf("complete receive operation %d\n", conn->num_completed_recv);
                }
                
                conn->msg_size = wc-> byte_len;
                //printf("set message size to %d\n", conn->msg_size);
                post_receive(conn);
                post_send(conn);    
        
        } else if (wc->opcode == IBV_WC_SEND) {
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
        printf("connection established\n");
        return 0;
}

int on_disconnect(struct rdma_cm_id *id)
{
        struct connection *conn = (struct connection *)id->context;
        printf("peer disconnected\n");
        
        // Destroy a QP allocated on the rdma_cm_id
        rdma_destroy_qp(id);

        // Free memory regions (MR)
        ibv_dereg_mr(conn->send_mr);
        ibv_dereg_mr(conn->recv_mr);
 
        free(conn->send_region);
        free(conn->recv_region);
 
        free(conn);

        // Destroy the specified rdma_cm_id 
        rdma_destroy_id(id);
 
        return 0;
}