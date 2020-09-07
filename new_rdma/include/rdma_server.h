#ifndef __NEWPLAN_RDMA_SERVER_H__
#define __NEWPLAN_RDMA_SERVER_H__
#include "rdma_session.h"
#include <vector>
#include <string>
#include <thread>
#include <functional>

#define DEFAULT_LISTEN_SOCK 0
#define DEFAULT_LISTEN_PORT 2333
#define DEFAULT_LISTEN_IP "0.0.0.0"

class RDMAServer
{
public:
    RDMAServer();
    virtual ~RDMAServer();

    void start_service(std::string listen_ip = DEFAULT_LISTEN_IP,
                       uint16_t listen_port = DEFAULT_LISTEN_PORT);

    void start_service_async(std::string listen_ip = DEFAULT_LISTEN_IP,
                             uint16_t listen_port = DEFAULT_LISTEN_PORT);
    void exit_on_error();

    void register_cb_after_pre_connect(std::function<void(void *)> cb)
    {
        cb_after_pre_connect_ = cb;
    }

private:
    void accept_new_connection();
    void build_socket();

private:
    std::vector<RDMASession *> rdma_session_;
    uint16_t listen_port_ = DEFAULT_LISTEN_PORT;
    std::string listen_ip_ = DEFAULT_LISTEN_IP;
    uint32_t listen_sock_ = DEFAULT_LISTEN_SOCK;

    std::thread *listen_thread = nullptr;

    std::function<void(void *)> cb_after_pre_connect_ = nullptr;

    bool is_serving_ = false;
};

#endif