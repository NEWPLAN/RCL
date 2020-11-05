#ifndef __NEWPLAN_TP_RDMA_CHANNEL_H__
#define __NEWPLAN_TP_RDMA_CHANNEL_H__
#include "rdma_adapter.h"
#include "rdma_session.h"

class RDMASession;

class RDMAChannel
{
public:
    explicit RDMAChannel(RDMASession *, uint8_t);
    virtual ~RDMAChannel();

    RDMASession *get_session() { return session_; }
    RDMAAdapter *get_context() { return ctx_; }

private:
    void init_channel();

private:
    RDMAAdapter *ctx_ = nullptr;
    RDMASession *session_ = nullptr;

    uint8_t prio_ = 0;
};
#endif