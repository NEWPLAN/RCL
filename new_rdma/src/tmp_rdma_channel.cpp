#include "tmp_rdma_channel.h"
#include <glog/logging.h>

NPRDMAChannel::NPRDMAChannel(NPRDMASession *sess) : session_(sess)
{
    LOG(INFO) << "Creating rdma channel";
    this->init_channel();
}

NPRDMAChannel::~NPRDMAChannel()
{
    LOG(INFO) << "Destroying rdma channel";
}

void NPRDMAChannel::init_channel()
{
    if (ctx_ != nullptr)
    {
        LOG(WARNING) << "Context has already been initialized";
        return;
    }
    ctx_ = new NPRDMAAdapter();
    if (!ctx_->init_ctx())
    {
        LOG(WARNING) << "Failed to init the rdma context";
    }
    LOG(INFO) << "Initialize IB resources";
}