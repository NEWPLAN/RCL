#ifndef __NEWPLAN_TP_RDMA_SESSION_H__
#define __NEWPLAN_TP_RDMA_SESSION_H__
#include "rdma_channel.h"
#include "pre_connector.h"

#include <glog/logging.h>
#include <functional>

class RDMAChannel;

class RDMASession
{
public:
    RDMASession();
    virtual ~RDMASession();

public:
    bool do_connect(bool);

    void connect_active();
    void connect_passive();

    RDMAChannel *get_channel() { return data_channel; }

public:
    bool set_pre_connector(PreConnector *pre);
    PreConnector *get_pre_connector();
    static RDMASession *new_rdma_session(PreConnector *pre);

private:
    RDMAChannel *data_channel = nullptr;
    PreConnector *pre_connector = nullptr;
    std::function<void(void *)> connect_by_endpoint = nullptr;
};
#endif