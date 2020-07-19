#ifndef __NEWPLAN_RDMA_SESSION_H__
#define __NEWPLAN_RDMA_SESSION_H__

#include <infiniband/verbs.h>
#include <thread>
#include <vector>

#include "rdma_endpoint.h"
#include "util.h"
#include "pre_connector.h"

enum SessionStatus
{
    WORK,
    CLOSING,
    CLOSED
};

class RDMASession
{
public:
    RDMASession(char *dev_name = nullptr) {}
    virtual ~RDMASession() {}

    //RDMAEndpoint *new_endpoint(PreConnector *pre);
    //void stop_process();
    RDMAEndpoint *pre_connect(PreConnector *pre);
    RDMAEndpoint *new_endpoint(PreConnector *pre);
    void delete_endpoint(RDMAEndpoint *endpoint);

    inline SessionStatus get_session_status() { return status_; }
    inline void set_session_status(SessionStatus state) { status_ = state; }

    bool send_request() { return pre_->config.server_name != 0; }

    const static int CQ_SIZE = 1024;

private:
    int open_ib_device();
    void session_processCQ();

private:
    SessionStatus status_;
    char *dev_name_;
    // device handle
    ibv_context *context_;
    // ibverbs protection domain
    ibv_pd *pd_;
    // Completion event endpoint, to wait for work completions
    ibv_comp_channel *event_channel_;
    // Completion queue, to poll on work completions
    ibv_cq *cq_;
    // Pre connection used to establish RDMA link
    PreConnector *pre_ = NULL;
    // MemoryPool
    //RDMA_MemoryPool *mempool_ = NULL;
    // List of endpoints
    std::vector<RDMAEndpoint *> endpoint_list_;
    // Thread used to process CQ
    std::unique_ptr<std::thread> process_thread_;
};
#endif