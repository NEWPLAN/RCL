#include "rdma_session.h"
#include <glog/logging.h>
#include "rdma_channel.h"
#include "tmp_rdma_adapter.h"

RDMASession::RDMASession()
{
}
RDMASession::~RDMASession()
{
}

RDMASession *RDMASession::build_RDMA_session(PreConnector *pre_connector)
{
    if (pre_connector == nullptr)
    {
        LOG(FATAL) << "Find problem on pre-connector";
    }

    RDMASession *rdma_sess = new RDMASession();

    rdma_sess->set_pre_connector(pre_connector);
    rdma_sess->build_rdma_channels();

    return rdma_sess;
}

void RDMASession::build_rdma_channels()
{
    //control channel
    LOG(INFO) << "Build control channel";
    RDMAChannel *control_channel = new RDMAChannel(nullptr, "ControlPlane");
    NPRDMAAdapter *tmp_adapter = new NPRDMAAdapter();
    control_channel->assign_adapter(tmp_adapter);

    control_channel->rdma_connect(this->pre_connector_);

    channel_group.push_back(control_channel);

    // control_channel->connect(pre_connector_);

    // //data channel
    // LOG(INFO) << "Build data channel";
    // RDMAChannel *data_channel = new RDMAChannel(nullptr);

    // data_channel->connect(pre_connector_);

    // if (!control_channel->set_up_control_plane())
    //     LOG(FATAL) << "Error of post ctrl recv";

    // char tmp_local[1024] = "hello again";
    // char tmp_remote[1024] = {0};
    // pre_connector_->sock_sync_data(tmp_local, tmp_remote, strlen(tmp_local));
    // if (strncmp(tmp_local, tmp_remote, strlen(tmp_local)) != 0)
    // {
    //     LOG(FATAL) << "Error of exchanging data for the last time";
    // }
    // LOG(INFO) << "All channel has been established";
    // control_channel->exchange_mem_info();
    // LOG(INFO) << "Has exchange mem info";
}

void RDMASession::set_pre_connector(PreConnector *pre_connector)
{
    this->pre_connector_ = pre_connector;
}