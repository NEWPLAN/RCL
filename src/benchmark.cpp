#include "util.h"
#include "preconnector.h"
#include <iostream>
#include <string>

int main(int argc, char *argv[])
{
    if (argc == 1 ||
        strcmp(argv[1], "--h") == 0 ||
        strcmp(argv[1], "--help") == 0)
    {
        LOG(INFO) << "Missing Start Up Config" << std::endl
                  << "\t Usage:" << std::endl
                  << "\t " << argv[0] << " s \t\t#to start the server" << std::endl
                  << "\t " << argv[0] << " c <sip>\t#to start the client " << std::endl;
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
    delete tcp_sock;
    return 0;
}