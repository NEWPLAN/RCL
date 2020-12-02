#include <iostream>
#include <glog/logging.h>

#include "rdma_base.h"
#include "rdma_adapter.h"
#include "config.h"

#include "endnode.h"
#include "rdma_client.h"
#include "rdma_server.h"
#include <vector>
#include <thread>
#include "utils/net_util.h"
#include "network_service.h"

int main(int argc, char *argv[])
{
    using NetService = communication::NetworkingService;
    NetService *ns = new NetService();
    ns->init_service(argc, argv);
    return ns->start_service_async();
}
