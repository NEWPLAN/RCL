#ifndef __NEWPLAN_RDMA_SESSION_H__
#define __NEWPLAN_RDMA_SESSION_H__

#include <glog/logging.h>
#include <functional>
#include "pre_connector.h"
#include "rdma_channel.h"

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

    RDMAChannel *get_channel(int ctype);

public:
    bool set_pre_connector(PreConnector *pre);
    PreConnector *get_pre_connector();
    static RDMASession *new_rdma_session(PreConnector *pre);

private:
    RDMAChannel *data_channel = nullptr;
    RDMAChannel *ctrl_channel = nullptr;
    PreConnector *pre_connector = nullptr;
    std::function<void(void *)> connect_by_endpoint = nullptr;
};
#endif