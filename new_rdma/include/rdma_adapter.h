#ifndef __NEWPLAN_TEMP_RDMA_ADAPTER_H__
#define __NEWPLAN_TEMP_RDMA_ADAPTER_H__

#include <infiniband/verbs.h>
#include <stdint.h>
#include <stdbool.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/time.h>
#include <getopt.h>
#include <string.h>

#define DEFAULT_IB_PORT 1

#define DEFAULT_MSG_SIZE 64000000

#define DEFAULT_CTRL_BUF_SIZE 16

#define DEFAULT_ITERS 100000

#define DEFAULT_PORT 12345

#define DEFAULT_GID_INDEX 3

#define READY_MSG "Ready"

enum
{
    RECV_WRID = 1,
    SEND_WRID = 2,
    WRITE_WRID = 3,
    WRITE_WITH_IMM_WRID = 4,
};

#define TEST_COMPLETION 12345

struct write_lat_context
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
    // Memory region for data plane messages
    struct ibv_mr *data_mr;
    // Memory region for control plane messages
    struct ibv_mr *ctrl_mr;
    // Completion queue
    struct ibv_cq *cq;
    // Queue pair
    struct ibv_qp *qp;
    // IB device attribute
    struct ibv_device_attr dev_attr;
    // IB port attribute
    struct ibv_port_attr port_attr;

    // Memory for data plane messages
    char *data_buf;
    uint32_t data_buf_size;

    // Memory for control plane messages
    char *ctrl_buf;
    uint32_t ctrl_buf_size;

    // Work request send flags
    bool inline_msg;
    uint32_t send_flags;

    // If use completion channel (event driven)
    bool use_event;

    uint32_t max_cq_size;
    uint32_t max_sq_size;
    uint32_t max_rq_size;
};

// Destination information
struct write_lat_dest
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

// Memory information
struct write_lat_mem
{
    uint64_t addr;
    uint32_t key;
} __attribute__((packed));

#include <string>
class RDMAAdapter
{
public:
    RDMAAdapter();
    virtual ~RDMAAdapter();

    // Initialize IB context
    bool init_ctx();

    // Destroy IB context
    void destroy_ctx();

    // Get the index of GID whose type is RoCE V2
    int get_rocev2_gid_index();

    // Initialize destination information
    bool init_dest(struct write_lat_dest *dest);

    void print_dest(struct write_lat_dest *dest);

    // Initialize data plane memory information
    bool init_data_mem(struct write_lat_mem *mem);

    void print_mem(struct write_lat_mem *mem);

    // Post a receive request to receive a control plane message.
    // Return true on success.
    bool post_ctrl_recv();

    // Post a send request to send a control plane message.
    // Return true on success.
    bool post_ctrl_send();

    // Post a write request to send a data plane message
    // Return true on success.
    bool post_data_write(struct write_lat_mem *rem_mem);

    // Post a write with immediate request to send a data plane message
    // Return true on success
    bool post_data_write_with_imm(struct write_lat_mem *rem_mem,
                                  unsigned int imm_data);

    // Connect my QP with a remote QP.
    // Return true on success.
    bool connect_qp(struct write_lat_dest *my_dest,
                    struct write_lat_dest *rem_dest);

    // Parse a receive work completion
    bool parse_recv_wc(struct ibv_wc *wc);

    // Pasrse a receive with immediate work completion
    bool parse_recv_with_imm_wc(struct ibv_wc *wc);

    // Parse a send work completion
    bool parse_send_wc(struct ibv_wc *wc);

    // Parse a RDMA write completion
    bool parse_write_wc(struct ibv_wc *wc);

    // Wait for a completed work request.
    // Return the completed work request in @wc.
    // Return true on success.
    bool wait_for_wc(struct ibv_wc *wc);

    static void wire_gid_to_gid(const char *wgid, union ibv_gid *gid)
    {
        char tmp[9];
        uint32_t v32;
        int i;

        for (tmp[8] = 0, i = 0; i < 4; ++i)
        {
            memcpy(tmp, wgid + i * 8, 8);
            sscanf(tmp, "%x", &v32);
            *(uint32_t *)(&gid->raw[i * 4]) = ntohl(v32);
        }
    }
    static void gid_to_wire_gid(const union ibv_gid *gid, char wgid[])
    {
        int i;

        for (i = 0; i < 4; ++i)
            sprintf(&wgid[i * 8], "%08x", htonl(*(uint32_t *)(gid->raw + i * 4)));
    }

private:
    int modify_qp_to_init(struct ibv_qp_attr &);
    bool modify_qp_to_rtr(struct write_lat_dest *);
    bool modify_qp_to_rts(struct write_lat_dest *);

private:
    std::string endpoint_name;

public:
    write_lat_context *ctx = nullptr;
    char *default_device = (char *)"mlx5_0";
};

#endif