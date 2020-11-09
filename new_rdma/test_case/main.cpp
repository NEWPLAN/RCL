#include "rdma_endpoint.h"
#include "rdma_server.h"
#include "rdma_client.h"

int main(int argc, char *argv[])
{

    if (argc < 2)
    {
        newplan::RDMAEndpoint *r_server = new newplan::RDMAServer(1251);
        LOG(INFO) << "Server: "
                  << newplan::RDMAEndpoint::get_rdma_type_str(r_server->get_type());

        r_server->run_async();
    }
    else
    {
        newplan::RDMAEndpoint *r_client = new newplan::RDMAClient(1251, argv[1]);
        LOG(INFO) << "Client: "
                  << newplan::RDMAEndpoint::get_rdma_type_str(r_client->get_type());

        r_client->run();
    }
}