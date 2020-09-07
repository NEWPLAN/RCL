#include "rdma_server.h"
#include <glog/logging.h>

#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#include "pre_connector.h"
#include "tmp_rdma_adapter.h"
#include "rdma_channel.h"

#include <chrono>

RDMAServer::RDMAServer()
{
    LOG(INFO) << "Building RDMAServer...";
}

RDMAServer::~RDMAServer()
{
    is_serving_ = false;
    listen_port_ = 0;
    listen_ip_ = "";
    if (listen_thread != nullptr)
    {
        delete listen_thread;
        listen_thread = nullptr;
    }
    LOG(INFO) << "Destroying RDMAServer...";
}

void RDMAServer::start_service(std::string listen_ip,
                               uint16_t listen_port)
{
    if (is_serving_) //exit  if already enabled
    {
        LOG(WARNING) << "Has been servicing";
        return;
    }

    { // loading and saving parameters
        this->listen_ip_ = listen_ip;
        this->listen_port_ = listen_port;
        LOG(INFO) << "Server " << listen_ip_
                  << " is listening on " << listen_port_;
    }

    is_serving_ = true;

    {
        this->build_socket();
        this->accept_new_connection();
    }
    LOG(INFO) << "exit of service";
}

void RDMAServer::start_service_async(std::string listen_ip,
                                     uint16_t listen_port)
{
    if (listen_thread != nullptr)
    {
        LOG(WARNING) << "the listen thread has allready been servicing...";
        return;
    }

    listen_thread = new std::thread([&, listen_ip, listen_port]() {
        this->start_service(listen_ip, listen_port);
    });
    LOG(INFO) << "exit of service_async";
}

void RDMAServer::accept_new_connection()
{

    do //loop wait and build new rdma connections
    {
        struct sockaddr_in cin;
        socklen_t len = sizeof(cin);
        int client_fd;

        if ((client_fd = accept(listen_sock_, (struct sockaddr *)&cin,
                                &len)) == -1)
        {
            LOG(FATAL) << "Error of accepting new connection";
        }

        std::string client_ip = std::string(inet_ntoa(cin.sin_addr));
        int client_port = htons(cin.sin_port);
        LOG(INFO) << "receive a connecting request from " << client_ip << ":" << client_port;
        PreConnector *pre_connector = new PreConnector(client_fd, client_ip, client_port); //building new pre_connector here;

        RDMASession *r_session = RDMASession::build_RDMA_session(pre_connector);
        rdma_session_.push_back(r_session);

        if (cb_after_pre_connect_ != nullptr)
        {
            cb_after_pre_connect_((void *)r_session->get_adapter());
        }
    } while (true);
    LOG(INFO) << "exit of accepting new connection";
}

void RDMAServer::build_socket()
{
    if (listen_sock_ != DEFAULT_LISTEN_SOCK)
    {
        LOG(WARNING) << "socket has already been inited: " << listen_sock_;
        return;
    }

    listen_sock_ = socket(AF_INET, SOCK_STREAM, 0);

    if (listen_sock_ < 0)
    {
        LOG(FATAL) << "Error in creating socket";
    }

    int one = 1;
    if (setsockopt(listen_sock_, SOL_SOCKET,
                   SO_REUSEADDR, &one, sizeof(one)) < 0)
    {
        close(listen_sock_);
        LOG(FATAL) << "reusing socket failed";
    }

    { // bind to socket and accept new connection requests;
        struct sockaddr_in sin;
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = inet_addr(listen_ip_.c_str());
        sin.sin_port = htons(listen_port_);
        if (bind(listen_sock_, (struct sockaddr *)&sin, sizeof(sin)) < 0)
        {
            LOG(FATAL) << "Error when binding to socket";
        }
        if (listen(listen_sock_, 1024) < 0)
        {
            LOG(FATAL) << "Error when listen on socket";
        }
        LOG(INFO) << "all parameters are set, and waiting for connections";
    }
}

void RDMAServer::exit_on_error()
{
    do
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    } while (true);
    LOG(INFO) << "exit of error";
}