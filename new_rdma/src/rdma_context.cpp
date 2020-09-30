#include "rdma_context.h"
#include <glog/logging.h>

RDMAContext::RDMAContext()
{
    LOG(INFO) << "Building RDMAContext";
}
RDMAContext::~RDMAContext()
{
    LOG(INFO) << "Destroying RDMAContext";
}

bool RDMAContext::write_remote(void *data_holder)
{
    return false;
}
bool RDMAContext::read_remote(void *data_holder)
{
    return false;
}
bool RDMAContext::send_remote(void *data_holder)
{
    return false;
}
bool RDMAContext::recv_remote(void *data_holder)
{
    return false;
}
bool RDMAContext::query_event(uint64_t event_token)
{
    return false;
}
