#ifndef __NEWPLAN_RDMA_CLIENT_H__
#define __NEWPLAN_RDMA_CLIENT_H__
#include <string>
#include <functional>
#include "rdma_session.h"

#define DEFAULT_LISTEN_SOCK 0
#define DEFAULT_LISTEN_PORT 2333
#define DEFAULT_LISTEN_IP "0.0.0.0"

class RDMAClient
{
public:
    RDMAClient();
    virtual ~RDMAClient(); //

    void connect(std::string server_ip = DEFAULT_LISTEN_IP,
                 uint16_t server_port = DEFAULT_LISTEN_PORT);

    void register_cb_after_pre_connect(std::function<void(void *)> cb)
    {
        cb_after_pre_connect_ = cb;
    }

    void exit_on_error();

private:
    uint16_t server_port_ = DEFAULT_LISTEN_PORT;
    std::string server_ip_ = DEFAULT_LISTEN_IP;
    uint32_t server_sock_ = DEFAULT_LISTEN_SOCK;

    RDMASession *rdma_session_ = nullptr;
    std::function<void(void *)> cb_after_pre_connect_ = nullptr;

    bool is_connected_ = false;
};

#endif