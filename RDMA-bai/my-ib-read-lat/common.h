#ifndef COMMON_H
#define COMMON_H

#include <infiniband/verbs.h>
#include <stdint.h>
#include <stdbool.h>

#define DEFAULT_IB_PORT 1

#define DEFAULT_MSG_SIZE 4096 

#define DEFAULT_CTRL_BUF_SIZE 16

#define DEFAULT_ITERS 100

#define DEFAULT_PORT 12345

#define DEFAULT_GID_INDEX 0

#define READY_MSG "Ready"

enum {
    RECV_WRID = 1,
    SEND_WRID = 2,
    READ_WRID = 3,
    SEND_WITH_IMM_WRID = 4,
};

#define TEST_COMPLETION 12345

struct read_lat_context 
{
    // IB device name   
    char                    *ib_dev_name;
    // IB device port
    int                     dev_port;
    int                     gid_index;
    // IB device context
    struct ibv_context      *ctx;
    // Completion channel
    struct ibv_comp_channel *channel; 
    // Protection domain      
    struct ibv_pd           *pd;
    // Memory region for data plane messages        
    struct ibv_mr           *data_mr; 
    // Memory region for control plane messages
    struct ibv_mr           *ctrl_mr; 
    // Completion queue
    struct ibv_cq           *cq;
    // Queue pair
    struct ibv_qp           *qp;
    // IB device attribute
    struct ibv_device_attr  dev_attr;
    // IB port attribute
    struct ibv_port_attr    port_attr;
    
    // Memory for data plane messages
    char                    *data_buf;
    int                     data_buf_size;
    
    // Memory for control plane messages
    char                    *ctrl_buf;
    int                     ctrl_buf_size;

    // Work request send flags
    int                     send_flags;

    // If use completion channel (event driven)
    bool                    use_event;
};

// Destination information
struct read_lat_dest {
    // Local identifier
    int             lid;
    // Queue pair number
    int             qpn;
    // Packet sequence number 
    int             psn;
    // Global identifier 
    union ibv_gid   gid;
};

// Memory information
struct read_lat_mem {
    uint64_t addr;
    uint32_t key;
} __attribute__((packed));    

// Initialize IB context 
bool init_ctx(struct read_lat_context *ctx);

// Destroy IB context
void destroy_ctx(struct read_lat_context *ctx);

// Get the index of GID whose type is RoCE V2
int get_rocev2_gid_index(struct read_lat_context *ctx);

// Write exactly 'count' bytes storing in buffer 'buf' into 
// the file descriptor 'fd'.
// Return the number of bytes sucsessfully written.
size_t write_exact(int fd, char *buf, size_t count);

// Read exactly 'count' bytes from the file descriptor 'fd' 
// and store the bytes into buffer 'buf'.
// Return the number of bytes successfully read.
size_t read_exact(int fd, char *buf, size_t count);

// Write my destination information to a remote server
bool write_dest_info(int fd, struct read_lat_dest *my_dest);

// Read destination informaiton from the remote server
bool read_dest_info(int fd, struct read_lat_dest *rem_dest);
                      
// Initialize destination information
bool init_dest(struct read_lat_dest *dest, 
               struct read_lat_context *ctx);

void print_dest(struct read_lat_dest *dest);

// Initialize data plane memory information
bool init_data_mem(struct read_lat_mem *mem, 
                   struct read_lat_context *ctx);

void print_mem(struct read_lat_mem *mem);

// Post a receive request to receive a control plane message.
// Return true on success.
bool post_ctrl_recv(struct read_lat_context *ctx);

// Post a send request to send a control plane message.
// Return true on success.
bool post_ctrl_send(struct read_lat_context *ctx);

// Post a send request with immediate data.
// Return true on success.
bool post_ctrl_send_with_imm(struct read_lat_context *ctx,
                             unsigned int imm_data);

// Post a read request to PULL a data plane message
// Return true on success.
bool post_data_read(struct read_lat_context *ctx,
                    struct read_lat_mem *rem_mem);

// Connect my QP with a remote QP.
// Return true on success.
bool connect_qp(struct read_lat_context *ctx,
                struct read_lat_dest *my_dest,
                struct read_lat_dest *rem_dest);

// Parse a receive work completion 
bool parse_recv_wc(struct ibv_wc *wc);

// Parse a send work completion 
bool parse_send_wc(struct ibv_wc *wc);

// Parse a RDMA read completion
bool parse_read_wc(struct ibv_wc *wc);

// Wait for a completed work request. 
// Return the completed work request in @wc.
// Return true on success.
bool wait_for_wc(struct read_lat_context *ctx, struct ibv_wc *wc);

#endif 