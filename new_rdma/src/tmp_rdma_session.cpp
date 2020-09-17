#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/time.h>
#include <getopt.h>
#include <string.h>

#include "tmp_rdma_session.h"
#include <glog/logging.h>
#include <chrono>
#include "tmp_rdma_adapter.h"

NPRDMASession::NPRDMASession()
{

    LOG(INFO) << "Creating rdma session";
}

NPRDMASession::~NPRDMASession()
{

    LOG(INFO) << "Destroying rdma session";
}

bool NPRDMASession::set_pre_connector(PreConnector *pre)
{
    if (get_pre_connector() == nullptr)
    {
        pre_connector = pre;
        return true;
    }
    return false;
}

PreConnector *NPRDMASession::get_pre_connector()
{
    return pre_connector;
}

NPRDMASession *NPRDMASession::new_rdma_session(PreConnector *pre)
{
    NPRDMASession *rdma_session = new NPRDMASession();
    if (rdma_session->set_pre_connector(pre))
    {
        return rdma_session;
    }
    LOG(WARNING) << "Warning: error of creating new rdma session";
    return NULL;
}

bool NPRDMASession::do_connect(bool is_server)
{
    if (pre_connector == nullptr)
    {
        LOG(WARNING) << "Error: pre connector is null!";
        return false;
    }
    data_channel = new NPRDMAChannel(this);

    if (is_server)
    {
        connect_passive();
        LOG(INFO) << "Has connected to the client!";
    }
    else
    {
        connect_active();
        LOG(INFO) << "Has connected to the Server!";
    }
    return true;
}

void NPRDMASession::connect_active() // for client
{
    LOG(INFO) << "client actively connects to server";

    NPRDMAAdapter *ctx = data_channel->get_context();

    struct write_lat_dest my_dest, rem_dest;
    // Get my destination information
    if (!ctx->init_dest(&my_dest))
    {
        LOG(FATAL) << "Error of init the endpoint";
    }

    printf("local address: ");
    ctx->print_dest(&my_dest);

    { //write dest info

        // Message format: "LID : QPN : PSN : GID"
        char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
        char gid[33];

        NPRDMAAdapter::gid_to_wire_gid(&my_dest.gid, gid);
        sprintf(msg, "%04x:%06x:%06x:%s", my_dest.lid, my_dest.qpn, my_dest.psn, gid);

        if (pre_connector->write_exact(msg, sizeof(msg)) != sizeof(msg))
        {
            LOG(FATAL) << "Could not send the local address";
        }

        LOG(INFO) << "Sending the destination information to the server";
    }

    { //read destination info
        // Message format: "LID : QPN : PSN : GID"
        char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
        char gid[33];
        if (pre_connector->read_exact(msg, sizeof(msg)) != sizeof(msg))
        {
            LOG(FATAL) << "Could not receive the remote address";
        }

        sscanf(msg, "%x:%x:%x:%s", &rem_dest.lid, &rem_dest.qpn, &rem_dest.psn, gid);
        NPRDMAAdapter::wire_gid_to_gid(gid, &rem_dest.gid);

        LOG(INFO) << "Reading the destination information from the server";
    }

    printf("remote address: ");
    ctx->print_dest(&rem_dest);

    { // connect to qp;
        if (!ctx->connect_qp(&my_dest, &rem_dest))
        {
            LOG(FATAL) << "Fail to connect to the client";
        }
        LOG(INFO) << "Connect to the client";
    }

    // Post the receive request to receive memory information from the server
    if (!ctx->post_ctrl_recv())
    {
        LOG(FATAL) << "Fail to post the receive request";
    }

    // Tell the server that the client is ready to receive the memory information (via RDMA send)
    char ready_msg[] = READY_MSG;
    if (pre_connector->write_exact(ready_msg, sizeof(ready_msg)) != sizeof(ready_msg))
    {
        LOG(FATAL) << "Fail to tell the server that the client is ready";
    }

    // int index_round = 10;

    // do
    // {
    //     char write_data[1024] = "Hello, server!";
    //     char read_data[1024] = {0};

    //     pre_connector->write_exact(write_data, sizeof(write_data));
    //     pre_connector->read_exact(read_data, 1024);
    //     //pre_connector->sock_sync_data(write_data, read_data, 1024);
    //     LOG(INFO) << "Read from peer: " << read_data;
    //     std::this_thread::sleep_for(std::chrono::seconds(2));
    // } while (index_round-- > 0);
}

void NPRDMASession::connect_passive() // for server
{
    LOG(INFO) << "Server is connecting to client, passively";

    NPRDMAAdapter *ctx = data_channel->get_context();

    struct write_lat_dest my_dest, rem_dest;
    // Get my destination information
    if (!ctx->init_dest(&my_dest))
    {
        LOG(FATAL) << "Error of initing the endpoint";
    }
    LOG(INFO) << "Local address: ";

    ctx->print_dest(&my_dest);

    { //read destination info
        // Message format: "LID : QPN : PSN : GID"
        char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
        char gid[33];
        if (pre_connector->read_exact(msg, sizeof(msg)) != sizeof(msg))
        {
            LOG(FATAL) << "Could not receive the remote address";
        }

        sscanf(msg, "%x:%x:%x:%s", &rem_dest.lid, &rem_dest.qpn, &rem_dest.psn, gid);
        NPRDMAAdapter::wire_gid_to_gid(gid, &rem_dest.gid);
    }

    LOG(INFO) << "Remote address: ";
    ctx->print_dest(&rem_dest);

    { // connect to qp;
        if (!ctx->connect_qp(&my_dest, &rem_dest))
        {
            LOG(FATAL) << "Fail to connect to the client";
        }
        LOG(INFO) << "Connect to the client";
    }
    { //write dest info

        // Message format: "LID : QPN : PSN : GID"
        char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
        char gid[33];

        NPRDMAAdapter::gid_to_wire_gid(&my_dest.gid, gid);
        sprintf(msg, "%04x:%06x:%06x:%s", my_dest.lid, my_dest.qpn, my_dest.psn, gid);

        if (pre_connector->write_exact(msg, sizeof(msg)) != sizeof(msg))
        {
            LOG(FATAL) << "Could not send the local address";
        }
    }

    {
        // Wait for the ready information from the client
        char buf[sizeof(READY_MSG)] = {0};
        if (pre_connector->read_exact(buf, sizeof(buf)) != sizeof(buf) ||
            strcmp(buf, READY_MSG) != 0)
        {
            LOG(FATAL) << "Fail to receive the ready message from the client";
        }

        LOG(INFO) << "Receive the ready message from the client";
    }

    // do
    // {
    //     char write_data[1024] = "Hello, client!";
    //     char read_data[1024] = {0};

    //     //pre_connector->sock_sync_data(write_data, read_data, 1024);

    //     pre_connector->write_exact(write_data, sizeof(write_data));
    //     pre_connector->read_exact(read_data, 1024);
    //     LOG(INFO) << "Read from peer: " << read_data;

    // } while (true);
}