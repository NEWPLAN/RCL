#include "tmp_rdma_end_point.h"

bool NPEndPoint::connect_with_peer(std::string peer_addr, short port)
{
    bool is_connected = false;
    { //assign params
        this->peer_addr_ = peer_addr;
        this->port_ = port;
        this->sockfd_ = 0;
    }

    return is_connected;
}