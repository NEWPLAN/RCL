#ifndef __NEWPLAN_TP_RDMA_SESSION_H__
#define __NEWPLAN_TP_RDMA_SESSION_H__
#include "rdma_channel.h"
#include "pre_connector.h"

#include <glog/logging.h>
#include <functional>

class RDMAChannel;

enum ChannelType
{
    UNKNOWN_CHANNEL = 0,
    DATA_CHANNEL = 1,
    CTRL_CHANNEL = 2
};

class RDMASession
{
public:
    RDMASession();
    virtual ~RDMASession();

public:
    bool do_connect(bool);

    void connect_active();
    void connect_passive();

    RDMAChannel *get_channel(ChannelType ctype)
    {
        if (ctype == DATA_CHANNEL)
            return data_channel;
        else if (ctype == CTRL_CHANNEL)
            return ctrl_channel;
        else
        {
            LOG(FATAL) << "Error when getting channel: " << ctype;
            return nullptr;
        }
    }

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