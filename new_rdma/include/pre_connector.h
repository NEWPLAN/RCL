#ifndef __NEWPLAN_PRE_CONNECTOR_H__
#define __NEWPLAN_PRE_CONNECTOR_H__
#include <glog/logging.h>
#include <string>

#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#include <thread>
#include <chrono>

class PreConnector
{
public:
    explicit PreConnector(int sock_fd, std::string addr, int port)
        : socket_fd(sock_fd)
    {
        this->peer_addr = addr;
        this->peer_port = port;
        LOG(INFO) << "Building preconnector...";
    };
    virtual ~PreConnector()
    {
        this->close();
        LOG(INFO) << "Destroying preconnector...";
    };
    int sock_sync_data(void *local_data, void *remote_data, int num_data)
    {
        //LOG(INFO) << "Exchange data with peer remote...";
        int rc;
        int read_bytes = 0;
        int total_read_bytes = 0;

        char *remote_data_tmp = (char *)remote_data;

        rc = write(this->socket_fd, local_data, num_data);

        if (rc < num_data)
        {

            printf("Failed writing data during sock_sync_data\n");
            std::this_thread::sleep_for(std::chrono::seconds(1));
            exit(0);
            //LOG(FATAL) << "Failed writing data during sock_sync_data";
        }
        else
            rc = 0;
        while (!rc && total_read_bytes < num_data)
        {
            read_bytes = read(this->socket_fd, remote_data_tmp, num_data);
            if (read_bytes > 0)
            {
                total_read_bytes += read_bytes;
                remote_data_tmp += read_bytes;
            }
            else
                rc = read_bytes;
        }

        if (rc < 0)
        {
            if (errno == ECONNRESET)
                LOG(INFO) << "Connection was reset by peer";
            else
                LOG(INFO) << "error: " << strerror(errno);
        }

        return rc;
    }

    void close()
    {
        if (socket_fd != -1)
        {
            LOG(INFO) << "Saying goodbye with :" << peer_addr << ":" << peer_port;
            ::close(socket_fd);
            socket_fd = -1;
        }
    }

    std::string get_peer_addr() { return peer_addr; }
    int get_peer_port() { return peer_port; }

    size_t read_exact(char *buf, size_t count)
    {
        // current buffer loccation
        char *cur_buf = NULL;
        // # of bytes that have been read
        size_t bytes_read = 0;
        int n;

        if (!buf)
        {
            return 0;
        }

        cur_buf = buf;

        while (count > 0)
        {
            //n = read(socket_fd, cur_buf, count);
            LOG(INFO) << "Before recv";
            n = recv(socket_fd, cur_buf, count, 0);
            LOG(INFO) << "After recv: " << n;
            if (n <= 0)
            {
                fprintf(stderr, "read error\n");
                break;
            }
            else
            {
                bytes_read += n;
                count -= n;
                cur_buf += n;
            }
        }

        if (n < 0)
        {
            if (errno == ECONNRESET)
            {
                LOG(INFO) << "Connection was reset by peer";
                do
                {
                    std::this_thread::sleep_for(std::chrono::seconds(10));
                } while (true);
            }
        }

        return bytes_read;
    }

    // Read exactly 'count' bytes from the file descriptor 'fd'
    // and store the bytes into buffer 'buf'.
    // Return the number of bytes successfully read.
    size_t write_exact(char *buf, size_t count)
    {
        // current buffer loccation
        char *cur_buf = NULL;
        // # of bytes that have been written
        size_t bytes_wrt = 0;
        int n;

        if (!buf)
        {
            return 0;
        }

        cur_buf = buf;

        while (count > 0)
        {
            LOG(INFO) << "Before write";

            //n = write(socket_fd, cur_buf, count);
            n = send(socket_fd, cur_buf, count, MSG_CONFIRM);

            LOG(INFO) << "After write: " << n;

            if (n <= 0)
            {
                LOG(INFO) << "failed to send data";
                fprintf(stderr, "write error\n");
                break;
            }
            else
            {
                bytes_wrt += n;
                count -= n;
                cur_buf += n;
            }
        }

        if (n < 0)
        {
            if (errno == ECONNRESET)
            {
                LOG(INFO) << "Connection was reset by peer";
                do
                {
                    std::this_thread::sleep_for(std::chrono::seconds(10));
                } while (true);
            }
        }

        return bytes_wrt;
    }

private:
    int socket_fd = -1;
    std::string peer_addr;
    int peer_port;
};

#endif