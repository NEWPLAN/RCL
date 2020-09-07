#ifndef __NEWPLAN_RDMA_ADAPTOR_H__
#define __NEWPLAN_RDMA_ADAPTOR_H__
#include <string>
#include "pre_connector.h"
#include <infiniband/verbs.h>

struct RDMAContext
{
    int dev_port;
    int gid_index;

    // IB device context
    struct ibv_context *ctx;
    // Completion channel
    struct ibv_comp_channel *channel;
    // Protection domain
    struct ibv_pd *pd;

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

    char *ctrl_buf, *data_buf;

    uint32_t ctrl_buf_size, data_buf_size;

    // Work request send flags
    bool inline_msg;
    int send_flags;

    // If use completion channel (event driven)
    bool use_event;
};

// structure to save the address of remote channels.
struct RDMAAddress
{
    uint32_t lid;
    uint32_t qpn;
    uint32_t psn;
    uint64_t snp;
    uint64_t iid;
    // Global identifier
    union ibv_gid gid;
};

// Memory information
struct MemInfo
{
    uint64_t addr;
    uint32_t key;
} __attribute__((packed));

#define DEFAULT_GID_INDEX 3
class RDMAAdapter
{
public:
    RDMAAdapter();
    ~RDMAAdapter();

    RDMAAdapter *init_adapter();

    void register_mem(char *buf, int size);
    void deregister_mem(char *buf);

    RDMAAddress *get_adapter_info();

    void create_QP();

    void qp_do_connect(RDMAAddress *peer_addr, PreConnector *pre_con);

    bool post_data_recv(uint64_t token = 0xffffff);
    bool post_send(int index);
    bool post_write(int index);

    bool post_ctrl_recv(void);
    bool post_ctrl_send(void);

    bool exchange_mem_info();

    bool process_CQ(int max_cqe = 1);

private:
    int modify_qp_to_init();
    int modify_qp_to_rtr();
    int modify_qp_to_rts();

    int get_rocev2_gid_index();

private:
    std::string dev_name;
    RDMAContext ctx;

    RDMAAddress *local_addr = nullptr;
    RDMAAddress *peer_addr = nullptr;

    struct ibv_qp_attr attr;
};

#endif //