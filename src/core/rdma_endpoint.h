#ifndef __NEWPLAN_RDMA_ENDPOINT_H__
#define __NEWPLAN_RDMA_ENDPOINT_H__

#include <infiniband/verbs.h>
#include <set>
#include "rdma_channel.h"
#include "rdma_mem_mgr.h"
#include "pre_connector.h"

struct RDMAEndpointInfo
{

    uint8_t gid[16]; /* gid */
    uint32_t lid;    /* LID of the IB port */
    uint32_t qpn;    /* QP number */
    uint32_t psn;
    // uint64_t addr;   /* Buffer address */
    // uint32_t rkey;   /* Remote key */
};

/* structure of system resources */
struct ChannelResource
{
    int MAX_S_WR, MAX_R_WR;             /* maximum send wr and receive wr */
    int ib_port;                        //which port to use in RDMA
    struct ibv_device_attr device_attr; /* Device attributes */
    struct ibv_device **dev_list;       /*dev list*/
    struct ibv_port_attr port_attr;     /* IB port attributes */
    struct cm_con_data_t remote_props;  /* values to connect to remote side */
    struct ibv_context *ib_ctx;         /* device handle */
    struct ibv_pd *pd;                  /* PD handle */
    struct ibv_cq *cq;                  /* CQ handle */
    struct ibv_qp *qp;                  /* QP handle */
    struct ibv_mr *mr;                  /* MR handle for buf */
    char *buf;                          /* memory buffer pointer, used for RDMA and send ops */
    PreConnector *pre;                  /* preconnector descriptor */
};

// // structure to exchange data which is needed to connect the QPs
// struct cm_con_data_t
// {
//     uint8_t gid[16]; /* gid */
//     uint64_t maddr;  // Buffer address
//     uint32_t mrkey;  // Remote key
//     uint32_t qpn;    // QP number
//     uint32_t lid;    // LID of the IB port
//     uint32_t psn;
// } __attribute__((packed));

class RDMAEndpoint
{
public:
    virtual ~RDMAEndpoint() {}

    RDMAEndpoint(PreConnector *pre);

    void create_resources();
    void release_resources(std::string error_info);
    void connect();

    int process_CQ();

    int write_remote(int msg_size);
    int send_remote(int msg_size);

    int post_receive(int msg_size);
    int post_send(int opcode, int msg_size);

    inline int max_to_send() { return connector_resource_.MAX_S_WR - data_in_flight - 1; }

    /*
    struct cm_con_data_t get_local_con_data();
    void connect(struct cm_con_data_t remote_con_data);
    void close();

    RDMABuffer *bufferalloc(int size);

    void send_data(RDMABuffer *buffer);
    void send_rawdata(void *addr, int size);

    void set_sync_barrier(int size);
    void wait_for_sync();

    // ----- Private To Public -----
    inline RDMAChannel *channel() const { return channel_; }
    inline RDMAMemMgr *mem_mgr() const { return mempool_; }
    
*/
    bool connected_;

private:
    int modify_qp_to_init();
    int modify_qp_to_rtr();
    int modify_qp_to_rts();

    struct ChannelResource connector_resource_;

    // Endpoint info used for exchange with remote
    RDMAEndpointInfo self_, remote_;
    // Message channel
    RDMAChannel *channel_ = NULL;
    // MemoryPool
    RDMAMemMgr *mempool_ = NULL;
    uint32_t data_in_flight = 0;
};
#endif