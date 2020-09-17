#ifndef __NEWPLAN_TP_RDMA_CHANNEL_H__
#define __NEWPLAN_TP_RDMA_CHANNEL_H__
#include "tmp_rdma_adapter.h"
#include "tmp_rdma_session.h"
class NPRDMASession;

class NPRDMAChannel
{
public:
    explicit NPRDMAChannel(NPRDMASession *sess);
    virtual ~NPRDMAChannel();

    NPRDMASession *get_session() { return session_; }
    NPRDMAAdapter *get_context() { return ctx_; }

private:
    void init_channel();

private:
    NPRDMAAdapter *ctx_ = nullptr;
    NPRDMASession *session_ = nullptr;
};
#endif