#ifndef __NEWPLAN_RDMA_ENDPOINT_H__
#define __NEWPLAN_RDMA_ENDPOINT_H__

#include <infiniband/verbs.h>
#include <set>
#include "rdma_channel.h"
#include "rdma_mem_mgr.h"

struct RDMAEndpointInfo
{

    uint8_t gid[16]; /* gid */
    uint32_t lid;    /* LID of the IB port */
    uint32_t qpn;    /* QP number */
    uint32_t psn;
    // uint64_t addr;   /* Buffer address */
    // uint32_t rkey;   /* Remote key */
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
    RDMAEndpoint(ibv_pd *pd, ibv_cq *cq,
                 ibv_context *context,
                 int ib_port, int cq_size,
                 RDMAMemMgr *mem_mgr);
    virtual ~RDMAEndpoint() {}

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
    // int modify_qp_to_init();
    // int modify_qp_to_rtr();
    // int modify_qp_to_rts();

    int ib_port_;

    // Protection domain
    ibv_pd *pd_;
    // Queue pair
    ibv_qp *qp_;

    // Endpoint info used for exchange with remote
    RDMAEndpointInfo self_, remote_;
    // Message channel
    RDMAChannel *channel_ = NULL;
    // MemoryPool
    RDMAMemMgr *mempool_ = NULL;
};
#endif