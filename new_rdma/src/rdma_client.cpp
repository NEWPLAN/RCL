#include "rdma_client.h"
#include <glog/logging.h>

#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#include "pre_connector.h"
#include "tmp_rdma_adapter.h"

#include "rdma_channel.h"

#include <chrono>
#include <thread>

RDMAClient::RDMAClient()
{
    LOG(INFO) << "Building RDMAClient...";
}

RDMAClient::~RDMAClient()
{
    LOG(INFO) << "Destroying RDMAClient...";
}

void RDMAClient::connect(std::string server_ip, uint16_t server_port)
{

    if (server_sock_ != DEFAULT_LISTEN_SOCK)
    {
        LOG(WARNING) << "connection has already been established";
        return;
    }
    this->server_ip_ = server_ip;
    this->server_port_ = server_port;

    server_sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock_ < 0)
    {
        LOG(FATAL) << "Error when creating socket";
    }

    struct sockaddr_in c_to_server;
    c_to_server.sin_family = AF_INET;
    c_to_server.sin_port = htons(this->server_port_);
    c_to_server.sin_addr.s_addr = inet_addr(this->server_ip_.c_str());

    if (is_connected_)
    {
        LOG(WARNING) << "Already connected to "
                     << server_ip << ":" << server_port_;
        return;
    }
    int count_try = 60 * 5 * 10;

    do
    {
        if (::connect(server_sock_, (struct sockaddr *)&c_to_server,
                      sizeof(c_to_server)) == 0)
            break; // break when successing

        LOG_EVERY_N(INFO, 10) << "[" << count_try / 10
                              << "] Failed when connecting to: "
                              << this->server_ip_ << ":" << this->server_port_;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } while (count_try-- > 0);

    is_connected_ = true;

    PreConnector *pre_connector = new PreConnector(server_sock_, this->server_ip_, this->server_port_);

    if (rdma_session_ == nullptr)
        rdma_session_ = RDMASession::build_RDMA_session(pre_connector);

    if (cb_after_pre_connect_ != nullptr)
    {
        cb_after_pre_connect_((void *)rdma_session_->get_adapter());
    }
}

void RDMAClient::exit_on_error()
{
    do
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    } while (true);
}
