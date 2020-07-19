

#include "pre_connector.h"
#include "util.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <chrono>
#include <thread>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

/*
static std::string make_string(const char *fmt, ...)
{
    char *sz;
    va_list ap;
    va_start(ap, fmt);
    if (vasprintf(&sz, fmt, ap) == -1)
        throw std::runtime_error("memory allocation failed\n");
    va_end(ap);
    std::string str(sz);
    free(sz);

    return str;
}
*/

TCPSockPreConnector::TCPSockPreConnector()
{
    LOG(INFO) << "TCPSockPreConnector Created";
}

TCPSockPreConnector::~TCPSockPreConnector()
{
    if (daemon_thread_)
        daemon_thread_->join();

    LOG(INFO) << "TCPreConneSockPctor Deleted";
}

void TCPSockPreConnector::print_config()
{
    //log_func();
    fprintf(stdout, " ------------------------------------------------\n");
    if (config.server_name)
        fprintf(stdout, " Remote IP                    : %s\n", config.server_name);
    fprintf(stdout, " IB port                      : %u\n", config.ib_port);
    fprintf(stdout, " TCP port                     : %u\n", config.tcp_port);
    fprintf(stdout, " ------------------------------------------------\n");
}

cm_con_data_t TCPSockPreConnector::exchange_qp_data(cm_con_data_t local_con_data)
{
    cm_con_data_t remote_con_data;

    if (sock_sync_data(remote_sock_, !config.server_name,
                       sizeof(cm_con_data_t), &local_con_data,
                       &remote_con_data) < 0)
    {
        LOG(ERROR) << "failed to exchange connection data between sides";
    }

    // close(remote_sock_);

    return remote_con_data;
}

/*****************************************
* Function: tcp_sock_connect
*****************************************/
void TCPSockPreConnector::pre_connect()
{
    print_config();

    // Client Side
    if (config.server_name)
    {
        remote_sock_ = sock_client_connect(config.server_name, config.tcp_port);
        if (remote_sock_ < 0)
        {
            LOG(INFO) << "failed to establish TCP connection to server " << config.server_name << ":" << config.tcp_port;
            return;
        }
    }
    else
    // Server Side
    {
        LOG(INFO) << "waiting on port " << config.tcp_port << " for TCP connection";
        remote_sock_ = sock_server_connect(config.tcp_port);
        if (remote_sock_ < 0)
        {
            LOG(FATAL) << "failed to establish TCP connection with client on port %d" << config.tcp_port;
            return;
        }
    }
    LOG(INFO) << "TCP connection was established";
}

void TCPSockPreConnector::daemon_connect(std::function<void()> connect_callback)
{
    print_config();

    if (config.server_name)
    {
        LOG(FATAL) << "Daemon should be used in server side";
        return;
    }
    LOG(INFO) << "Daemon waiting for TCP connection on port: " << config.tcp_port;

    // Use a new thread to do the CQ processing
    daemon_thread_.reset(new std::thread(std::bind(
        &TCPSockPreConnector::sock_daemon_connect, this, config.tcp_port, connect_callback)));
}

void TCPSockPreConnector::close_daemon()
{
    daemon_run_ = false;
    write(listen_sock_, NULL, 0);
}

/*****************************************
* Function: sock_daemon_connect
*****************************************/

#define MAX_EVENTS 10

int TCPSockPreConnector::sock_daemon_connect(int port, std::function<void()> connect_callback)
{
    struct addrinfo *res, *t;
    struct addrinfo hints = {
        .ai_flags = AI_PASSIVE,
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM};
    char *service;
    int n;
    int sockfd = -1, connfd, epfd;
    struct epoll_event ev, events[MAX_EVENTS];

    if (asprintf(&service, "%d", port) < 0)
    {
        LOG(FATAL) << "asprintf failed";
        return -1;
    }

    n = getaddrinfo(NULL, service, &hints, &res);
    if (n < 0)
    {
        LOG(FATAL) << gai_strerror(n) << "for port " << port;
        free(service);
        return -1;
    }

    for (t = res; t; t = t->ai_next)
    {
        sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol);
        if (sockfd >= 0)
        {
            n = 1;

            setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &n, sizeof n);

            if (!bind(sockfd, t->ai_addr, t->ai_addrlen))
                break;
            close(sockfd);
            sockfd = -1;
        }
    }

    freeaddrinfo(res);
    free(service);

    if (sockfd < 0)
    {
        LOG(FATAL) << "couldn't listen to port " << port;
        return -1;
    }

    int opts = fcntl(sockfd, F_GETFL);
    if (opts < 0)
    {
        LOG(FATAL) << "Fcntl get error";
        return -1;
    }
    if (fcntl(sockfd, F_SETFL, opts | O_NONBLOCK) < 0)
    {
        LOG(FATAL) << "Fcntl set error";
        return -1;
    }

    listen(sockfd, 10);

    epfd = epoll_create(MAX_EVENTS);
    if (epfd < 0)
    {
        LOG(FATAL) << "Epoll create error";
        return -1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = sockfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev) == -1)
    {
        LOG(FATAL) << "Epoll add error";
        return -1;
    }

    listen_sock_ = sockfd;

    daemon_run_ = true;
    while (1)
    {
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (!daemon_run_)
        {
            close(listen_sock_);
            return -1;
        }
        if (nfds == -1)
        {
            LOG(FATAL) << "epoll_wait error";
            return -1;
        }
        for (int i = 0; i < nfds; i++)
        {
            int fd = events[i].data.fd;
            if (fd == sockfd)
            {
                while ((connfd = accept(sockfd, NULL, 0)) > 0)
                {
                    remote_sock_ = connfd;

                    connect_callback();
                }
            }
            else
            {
                LOG(FATAL) << "Unknown epoll sock event get";
                return -1;
            }
        }
    }
    close(sockfd);
}

/*****************************************
* Function: sock_server_connect
*****************************************/
int TCPSockPreConnector::sock_server_connect(int port)
{
    struct sockaddr_in serv_addr;
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size = sizeof(clnt_addr);
    int sockfd = -1, connfd = -1;

    sockfd = socket(PF_INET, SOCK_STREAM, 0);

    if (sockfd == -1)
    {
        LOG(FATAL) << "Error in creating socket";
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    int opt = 1;
    // sockfd为需要端口复用的套接字
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt, sizeof(opt));

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        LOG(FATAL) << "Error in binding socket at port: " << port;
    }
    listen(sockfd, 1024);
    connfd = accept(sockfd, (struct sockaddr *)&clnt_addr, &clnt_addr_size);
    close(sockfd);
    if (connfd < 0)
    {
        LOG(FATAL) << "accept() failed";
        return -1;
    }
    LOG(INFO) << "Recv connection from: " << inet_ntoa(clnt_addr.sin_addr);

    return connfd;
}

/*****************************************
* Function: sock_client_connect
*****************************************/
int TCPSockPreConnector::sock_client_connect(const char *server_name, int port)
{
    int sockfd = -1;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0)
    {
        LOG(FATAL) << "Error in creating socket for clinet";
    }

    struct sockaddr_in client;
    client.sin_family = AF_INET;
    client.sin_addr.s_addr = inet_addr(server_name);
    client.sin_port = htons(port);

    int try_times = 0;
    do
    {
        LOG_EVERY_N(INFO, 10) << "[" << 60 * 10 * 2 - try_times
                              << "] Reconnecting to "
                              << server_name << ":" << port;

        if (connect(sockfd, (struct sockaddr *)&client, sizeof(client)) >= 0)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        try_times++;
    } while (try_times < 60 * 10 * 2); //loops for 2 mins, try to connect every 100ms

    if (try_times >= 60 * 10 * 2)
    {
        LOG(FATAL) << "Can not connect to server " << server_name << ":" << port;
    }

    // struct addrinfo *res, *t;
    // struct addrinfo hints = {
    //     .ai_flags = AI_PASSIVE,
    //     .ai_family = AF_UNSPEC,
    //     .ai_socktype = SOCK_STREAM};

    // char *service;
    // int n;
    // int sockfd = -1;

    // if (asprintf(&service, "%d", port) < 0)
    // {
    //     LOG(FATAL) << "asprintf failed";
    //     return -1;
    // }

    // n = getaddrinfo(server_name, service, &hints, &res);
    // if (n < 0)
    // {
    //     LOG(FATAL) << gai_strerror(n) << " for " << server_name << ":" << port;
    //     free(service);
    //     return -1;
    // }

    // for (t = res; t; t = t->ai_next)
    // {
    //     sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol);
    //     if (sockfd >= 0)
    //     {
    //         if (!connect(sockfd, t->ai_addr, t->ai_addrlen))
    //             break;
    //         close(sockfd);
    //         sockfd = -1;
    //     }
    // }
    // freeaddrinfo(res);
    // free(service);

    // if (sockfd < 0)
    // {
    //     LOG(FATAL) << "couldn't connect to " << server_name << ":" << port;
    //     return -1;
    // }

    return sockfd;
}

/*****************************************
* Function: sock_recv
*****************************************/
int TCPSockPreConnector::sock_recv(int sock_fd, size_t size, void *buf)
{
    int rc;

retry_after_signal:
    rc = recv(sock_fd, buf, size, MSG_WAITALL);
    if (rc != size)
    {
        fprintf(stderr, "recv failed: %s, rc=%d\n", strerror(errno), rc);

        if ((errno == EINTR) && (rc != 0))
            goto retry_after_signal; /* Interrupted system call */
        if (rc)
            return rc;
        else
            return -1;
    }

    return 0;
}

/*****************************************
* Function: sock_send
*****************************************/
int TCPSockPreConnector::sock_send(int sock_fd, size_t size, const void *buf)
{
    int rc;

retry_after_signal:
    rc = send(sock_fd, buf, size, 0);

    if (rc != size)
    {
        fprintf(stderr, "send failed: %s, rc=%d\n", strerror(errno), rc);

        if ((errno == EINTR) && (rc != 0))
            goto retry_after_signal; /* Interrupted system call */
        if (rc)
            return rc;
        else
            return -1;
    }

    return 0;
}

/*****************************************
* Function: sock_sync_data
*****************************************/
int TCPSockPreConnector::sock_sync_data(int sock_fd, int is_daemon, size_t size, const void *out_buf, void *in_buf)
{
    int rc;

    if (is_daemon)
    {
        rc = sock_send(sock_fd, size, out_buf);
        if (rc)
            return rc;

        rc = sock_recv(sock_fd, size, in_buf);
        if (rc)
            return rc;
    }
    else
    {
        rc = sock_recv(sock_fd, size, in_buf);
        if (rc)
            return rc;

        rc = sock_send(sock_fd, size, out_buf);
        if (rc)
            return rc;
    }

    return 0;
}

/*****************************************
* Function: sock_sync_ready
*****************************************/
int TCPSockPreConnector::sock_sync_ready(int sock_fd, int is_daemon)
{
    char cm_buf = 'a';
    return sock_sync_data(sock_fd, is_daemon, sizeof(cm_buf), &cm_buf, &cm_buf);
}