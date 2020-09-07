#include "rdma_channel.h"
#include <string>
#include "rdma_adapter.h"
#include <glog/logging.h>
#include <arpa/inet.h>
#include "tmp_rdma_adapter.h"

void RDMAChannel::show_adapter_info(std::string info, RDMAAddress *np)
{

    LOG(INFO) << "Showing " << info;
    printf("--------------------------------------\n");
    printf("LID 0x%04x, QPN 0x%06x, PSN 0x%06lx\n", np->lid, np->qpn, np->snp);
    char gid[33] = {0};
    inet_ntop(AF_INET6, &(np->gid), gid, sizeof(gid));
    printf("GID %s\n", gid);
    printf("--------------------------------------\n");
}

void RDMAChannel::connect(PreConnector *pre_con)
{
    pre_connector = pre_con;
    rdma_adapter_->init_adapter();
    self_ = *rdma_adapter_->get_adapter_info();

    show_adapter_info("local address", &self_);

    if (pre_connector->sock_sync_data((void *)(&self_),
                                      (void *)(&peer_),
                                      sizeof(self_)) == -1)
    {
        LOG(FATAL) << "error of exchanging data with peer";
    }

    show_adapter_info("peer address", &peer_);

    rdma_adapter_->qp_do_connect(&peer_, pre_connector);

    LOG(INFO) << "RDMAChannel has been established";
}

bool RDMAChannel::recv_msg(int index)
{
    return this->get_context()->process_CQ();
}
void RDMAChannel::write_msg(char *buf)
{
    this->get_context()->post_send(0);
    return;
}

void RDMAChannel::rdma_connect(PreConnector *pre_con)
{
    pre_connector = pre_con;
    if (!this->adapter_->init_ctx())
    {
        LOG(FATAL) << "Error of init ctx for RDMA adapter";
    }

    struct write_lat_dest my_dest, rem_dest;

    if (!this->adapter_->init_dest(&my_dest))
    {
        LOG(FATAL) << "Error of init dest";
    }

    printf("local address: ");
    this->adapter_->print_dest(&my_dest);

    if (pre_connector->sock_sync_data((void *)(&my_dest),
                                      (void *)(&rem_dest),
                                      sizeof(rem_dest)) == -1)
    {
        LOG(FATAL) << "error of exchanging data with peer";
    }

    printf("remote address: ");
    this->adapter_->print_dest(&rem_dest);

    if (!this->adapter_->connect_qp(&my_dest, &rem_dest))
    {
        LOG(FATAL) << "error of connecting QP";
    }

    if (pre_connector->sock_sync_data((void *)(&my_dest),
                                      (void *)(&rem_dest),
                                      sizeof(rem_dest)) == -1)
    { //  for synchronization...
        LOG(FATAL) << "error of exchanging data with peer";
    }

    if (!this->adapter_->post_ctrl_recv())
    {
        LOG(FATAL) << "post control recv failed";
    }

    if (pre_connector->sock_sync_data((void *)(&my_dest),
                                      (void *)(&rem_dest),
                                      sizeof(rem_dest)) == -1)
    { //  for synchronization...
        LOG(FATAL) << "error of exchanging data with peer";
    }

    // do
    // {
    //     LOG(INFO) << "RDMAChannel has been established";
    //     std::this_thread::sleep_for(std::chrono::seconds(1));
    // } while (1);
    //rdma_adapter_->qp_do_connect(&peer_, pre_connector);

    LOG(INFO) << "RDMAChannel has been established";
}
