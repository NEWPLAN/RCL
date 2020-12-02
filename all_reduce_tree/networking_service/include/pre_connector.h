#ifndef __NEWPLAN_RDMA_PRE_CONNECTOR_H__
#define __NEWPLAN_RDMA_PRE_CONNECTOR_H__
#include <string>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>

class NPRDMAPreConnector
{
public:
    NPRDMAPreConnector(int, std::string, int);
    virtual ~NPRDMAPreConnector() {}

public:
    int get_or_build_socket();
    int sock_sync_data(int,
                       char *,
                       char *);
    int send_data(int, char *);
    int recv_data(int, char *);
    int recv_data_force(int, char *);

public:
    std::string &get_my_ip() { return this->my_ip; }
    std::string &get_peer_ip() { return this->peer_ip; }
    int get_peer_port() { return this->peer_port; }
    int get_my_port() { return this->my_port; }

private:
    int root_sock = 0;
    std::string peer_ip;
    int peer_port;
    std::string my_ip;
    int my_port;
};
#endif