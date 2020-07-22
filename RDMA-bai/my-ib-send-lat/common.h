#ifndef COMMON_H
#define COMMON_H

#include <infiniband/verbs.h>
#include <stdint.h>
#include <stdbool.h>

#define DEFAULT_IB_PORT 1

#define DEFAULT_MSG_SIZE 4096

#define DEFAULT_ITERS 100

#define DEFAULT_PORT 12345

#define DEFAULT_RX_DEPTH 500

#define DEFAULT_GID_INDEX 3

enum
{
    RECV_WRID = 1,
    SEND_WRID = 2,
};

struct send_lat_context
{
    // IB device name
    char *ib_dev_name;
    // IB device port
    int dev_port;
    int gid_index;
    // IB device context
    struct ibv_context *ctx;
    // Completion channel
    struct ibv_comp_channel *channel;
    // Protection domain
    struct ibv_pd *pd;
    // Memory region
    struct ibv_mr *mr;
    // Completion queue
    struct ibv_cq *cq;
    // Queue pair
    struct ibv_qp *qp;
    // IB device attribute
    struct ibv_device_attr dev_attr;
    // IB port attribute
    struct ibv_port_attr port_attr;

    // Associated memory
    char *buf;
    int buf_size;

    // Work request send flags
    bool inline_msg;
    int send_flags;

    // If use completion channel (event driven)
    bool use_event;
};

struct send_lat_dest
{
    // Local identifier
    int lid;
    // Queue pair number
    int qpn;
    // Packet sequence number
    int psn;
    // Global identifier
    union ibv_gid gid;
};

// Initialize IB context
bool init_ctx(struct send_lat_context *ctx);

// Destroy IB context
void destroy_ctx(struct send_lat_context *ctx);

// Get the index of GID whose type is RoCE V2
int get_rocev2_gid_index(struct send_lat_context *ctx);

// Write my destination information to a remote server
bool write_dest_info(int fd, struct send_lat_dest *my_dest);

// Read remote destination informaiton from a remote server
bool read_dest_info(int fd, struct send_lat_dest *rem_dest);

// The client exchanges destination information with a server
bool client_exch_dest(char *server_ip,
                      int server_port,
                      struct send_lat_dest *my_dest,
                      struct send_lat_dest *rem_dest);

// The server exchanges destination information with a client
bool server_exch_dest(int server_port,
                      struct send_lat_context *ctx,
                      struct send_lat_dest *my_dest,
                      struct send_lat_dest *rem_dest);

// Initialize destination information
bool init_dest(struct send_lat_dest *dest,
               struct send_lat_context *ctx);

void print_dest(struct send_lat_dest *dest);

// Post 'n' receive requests.
// Return # of receive requests that are successfully posted.
int post_recv(struct send_lat_context *ctx, int n);

// Post a send request.
// Return true on success.
bool post_send(struct send_lat_context *ctx);

// Connect my QP with a remote QP.
// Return true on success.
bool connect_qp(struct send_lat_context *ctx,
                struct send_lat_dest *my_dest,
                struct send_lat_dest *rem_dest);
#endif