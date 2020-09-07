#ifndef __NP_TMP_RDMA_SERVER_H__
#define __NP_TMP_RDMA_SERVER_H__
#include <string>

class NPEndPoint
{
public:
    NPEndPoint() : peer_addr_(""), port_(0), sockfd_(0) {}
    virtual ~NPEndPoint() {}

public:
    virtual bool connect_with_peer(std::string peer_addr, short port) = 0;

    bool is_connected() { return !peer_addr_.empty() && port_ != 0; }

private:
    std::string peer_addr_;
    short port_;
    short sockfd_;
};

#endif