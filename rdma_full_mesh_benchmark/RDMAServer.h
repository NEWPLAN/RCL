#ifndef __RDMA_SERVER_H__
#define __RDMA_SERVER_H__
#include <thread>
#include <vector>
#include <rdma/rdma_cma.h>
#include "RDMABase.h"
#include <mutex>
#include <condition_variable>

class RDMAServer : public RDMABase
{
public:
    RDMAServer(RDMAAdapter &rdma_adapter);
    ~RDMAServer();

    void setup();
    void start_service();

protected:
    virtual void *poll_cq(void *_id);
    virtual void on_connection(struct rdma_cm_id *id);
    virtual void build_context(struct rdma_cm_id *id);
    virtual void on_pre_conn(struct rdma_cm_id *id);
    virtual void on_disconnect(struct rdma_cm_id *id);

private:
    void post_receive(struct rdma_cm_id *id);
    void send_message(struct rdma_cm_id *id,
                      uint32_t token_id,
                      uint32_t imm_data = 666);
    void on_completion(struct ibv_wc *wc);
    void process_message(struct RDMAContext *ctx, uint32_t token,
                         uint8_t *buf, uint32_t len);

private:
    void server_event_loops();
    void _init();

    void sync_thread_func();

private:
    std::thread *aggregator_thread = nullptr;
    RDMAAdapter rdma_adapter_;
    std::vector<std::thread *> recv_threads;

    std::vector<struct RDMAContext *> ctx_group;

    std::mutex mtx;
    std::condition_variable cv;
    bool ready = false;
};

#endif