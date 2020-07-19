#include "util.h"
#include "pre_connector.h"
#include <iostream>
#include <string>
#include "rdma_session.h"

int main(int argc, char *argv[])
{
    if (argc == 1 ||
        strcmp(argv[1], "--h") == 0 ||
        strcmp(argv[1], "--help") == 0)
    {
        LOG(INFO) << "Missing Start Up Config" << std::endl
                  << "\t Usage:" << std::endl
                  << "\t " << argv[0] << " -s \t#to start the server" << std::endl
                  << "\t " << argv[0] << " -c <sip>\t#to start the client " << std::endl;
        exit(0);
    }
    TCPSockPreConnector *tcp_sock = new TCPSockPreConnector();
    if (strcmp(argv[1], "-s") == 0) //server side
    {
        LOG(INFO) << "Server start...";
    }
    else if (strcmp(argv[1], "-c") == 0)
    {
        LOG(INFO) << "Client start...";
        if (argc == 2)
            tcp_sock->config.server_name = LOCALHOST;
        else
            tcp_sock->config.server_name = argv[2];
        LOG(INFO) << "Connecting to " << tcp_sock->config.server_name << ":" << tcp_sock->config.tcp_port;
    }
    RDMASession *r_session = new RDMASession();
    RDMAEndpoint *r_endpoint = r_session->pre_connect(tcp_sock);

    while (!r_endpoint->connected_)
    {
        LOG_EVERY_N(INFO, 20) << "Main thread is waiting for connection from peers";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (r_session->send_request())
        r_endpoint->post_send(IBV_WR_SEND, 1024 * 1);
    while (1)
    {
        r_endpoint->process_CQ();
        if (r_session->send_request())
        {
            for (int index = 0; index < r_endpoint->max_to_send(); index++)
                r_endpoint->write_remote(1024 * 2);
        }
        //LOG(INFO) << "Waiting in the main threads";
        //std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    delete tcp_sock;
    return 0;
}