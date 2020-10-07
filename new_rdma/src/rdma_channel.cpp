#include "rdma_channel.h"
#include <glog/logging.h>

RDMAChannel::RDMAChannel(RDMASession *sess) : session_(sess)
{
    LOG(INFO) << "Creating rdma channel";
    this->init_channel();
}

RDMAChannel::~RDMAChannel()
{
    LOG(INFO) << "Destroying rdma channel";
}

void RDMAChannel::init_channel()
{
    if (ctx_ != nullptr)
    {
        LOG(WARNING) << "Context has already been initialized";
        return;
    }
    ctx_ = new RDMAAdapter();
    if (!ctx_->init_ctx())
    {
        LOG(WARNING) << "Failed to init the rdma context";
    }
    LOG(INFO) << "Initialize IB resources";
}