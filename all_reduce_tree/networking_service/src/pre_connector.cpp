#include "pre_connector.h"
#include "ip_qos_helper.h"
#include <glog/logging.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/ip.h>

NPRDMAPreConnector::NPRDMAPreConnector(int sock, std::string ip, int port)
{
    this->root_sock = sock;
    this->peer_ip = ip;
    this->peer_port = port;
}

int NPRDMAPreConnector::get_or_build_socket()
{
    if (this->root_sock == 0)
    {
        this->root_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (this->root_sock <= 0)
            LOG(FATAL) << "Error of creating socket...";
        LOG(INFO) << "Creating root socket: " << this->root_sock;
    }
    return this->root_sock;
}
int NPRDMAPreConnector::sock_sync_data(int xfer_size,
                                       char *local_data,
                                       char *remote_data)
{
    int write_msg, recv_msg;

    write_msg = send_data(xfer_size, local_data);
    recv_msg = recv_data_force(xfer_size, remote_data);

    //LOG(INFO) << "Recv_msg: " << recv_msg << ", write msg_" << write_msg;

    return recv_msg - write_msg;
}

int NPRDMAPreConnector::send_data(int msg_size, char *data_placeholder)
{
    int sock = get_or_build_socket();
    int write_ret = write(sock, data_placeholder, msg_size);
    if (write_ret < msg_size)
        LOG(FATAL) << "Failed writing data to remote";
    return write_ret;
}
int NPRDMAPreConnector::recv_data(int msg_size, char *data_placeholder)
{
    int sock = get_or_build_socket();
    int rc = 0;
    int total_read_bytes = 0;

    while (!rc && total_read_bytes < msg_size)
    {
        int read_bytes = read(sock, data_placeholder, msg_size);
        if (read_bytes > 0)
            total_read_bytes += read_bytes;
        else
            rc = read_bytes;
    }
    return rc;
}

int NPRDMAPreConnector::recv_data_force(int msg_size, char *data_placeholder)
{
    int sock = get_or_build_socket();
    int total_read_bytes = 0;
    int loops = 0;
    while (total_read_bytes < msg_size)
    {
        int read_bytes = read(sock, data_placeholder + total_read_bytes,
                              msg_size - total_read_bytes);
        if (read_bytes > 0)
            total_read_bytes += read_bytes;
        else if (read_bytes < 0)
        {
            LOG(FATAL) << "read socket failed: " << strerror(errno);
        }
        else
        {
            if (++loops >= 100000)
            {
                LOG(WARNING) << "Cannot receive any data from socket";
            }
            continue;
        }
        loops = 0;
    }
    return total_read_bytes;
}
