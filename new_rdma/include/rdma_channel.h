#ifndef __NEWPLAN_RDMA_CHANNEL_H__
#define __NEWPLAN_RDMA_CHANNEL_H__
#include <glog/logging.h>
#include <infiniband/verbs.h>
#include "rdma_adapter.h"
#include "pre_connector.h"
#include "tmp_rdma_adapter.h"

class RDMAAdapter;
class PreConnector;

struct ChannelParams
{
    uint8_t port_num;
    uint8_t sgid_index;
    uint8_t pkey_index;
    uint32_t queue_depth;
    uint8_t timeout;
    uint8_t retry_cnt;
    uint8_t sl;
    enum ibv_mtu mtu;
    uint8_t traffic_class;
};

class RDMAChannel
{
public:
    explicit RDMAChannel(RDMAAdapter *rdma_adapter,
                         std::string info = "")
        : rdma_adapter_(rdma_adapter)
    {
        if (!info.empty())
        {
            LOG(INFO) << "Building RDMA Channel for " << info;
        }
        channel_info = info;

        memset(&res_, 0, sizeof(struct ChannelParams));
        memset(&self_, 0, sizeof(struct RDMAAddress));
        memset(&peer_, 0, sizeof(struct RDMAAddress));
        LOG(INFO) << "Creating RDMAChannel...";
        if (rdma_adapter_ == nullptr ||
            rdma_adapter_ == NULL ||
            rdma_adapter_ == 0)
        {
            rdma_adapter_ = new RDMAAdapter();
        }
    }

    ~RDMAChannel()
    {
        LOG(INFO) << "Destroying RDMAChannel...";
    }

    void connect(PreConnector *pre_con);

    RDMAAddress &self() { return self_; }
    RDMAAddress &peer() { return peer_; }

    inline RDMAAdapter *get_context() { return this->rdma_adapter_; }

    bool assign_adapter(NPRDMAAdapter *adapter)
    {
        if (this->adapter_ != nullptr)
            return false;
        this->adapter_ = adapter;
        return true;
    }

    inline NPRDMAAdapter *get_adapter()
    {
        return this->adapter_;
    }

    void rdma_connect(PreConnector *pre_con);

    bool recv_msg(int index);
    void write_msg(char *buf);

    bool set_up_control_plane() { return rdma_adapter_->post_ctrl_recv(); }

    bool exchange_mem_info() { return rdma_adapter_->exchange_mem_info(); }

private:
    void show_adapter_info(std::string info, RDMAAddress *np);

private:
    RDMAAdapter *rdma_adapter_ = nullptr;
    ChannelParams res_;
    RDMAAddress self_, peer_;
    PreConnector *pre_connector = nullptr;

    NPRDMAAdapter *adapter_ = nullptr;

    std::string channel_info;
};
#endif