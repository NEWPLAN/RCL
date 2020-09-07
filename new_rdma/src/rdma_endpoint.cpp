#include "rdma_endpoint.h"
#include <glog/logging.h>
namespace newplan
{
void RDMAServer::run_async()
{
    LOG(INFO) << "Server runing async";
}
void RDMAServer::run()
{
    LOG(INFO) << "Server runing ";
}

void RDMAClient::run_async()
{
    LOG(INFO) << "Client runing async";
}
void RDMAClient::run()
{
    LOG(INFO) << "Client runing";
}

}; // namespace newplan