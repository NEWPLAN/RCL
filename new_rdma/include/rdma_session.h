#ifndef __NEWPLAN_RDMA_SESSION_H__
#define __NEWPLAN_RDMA_SESSION_H__

#include <glog/logging.h>
#include <vector>
#include "tmp_rdma_adapter.h"
#include "rdma_channel.h"

class RDMAChannel;
class RDMAAdapter;
class PreConnector;

class RDMASession
{
public:
    RDMASession();
    ~RDMASession();

    static RDMASession *build_RDMA_session(PreConnector *pre_connector);

    inline NPRDMAAdapter *get_adapter()
    {
        if (!channel_group.empty())
            return channel_group[0]->get_adapter();
        else
            LOG(WARNING) << "empty for channel_group";
        return nullptr;
    }

private:
    void set_pre_connector(PreConnector *pre_connector);

    void build_rdma_channels();

private:
    std::vector<RDMAChannel *> channel_group; //0 is the control channel, others are data channel;

    PreConnector *pre_connector_ = nullptr;
};

#endif