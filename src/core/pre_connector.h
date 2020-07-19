#ifndef __NEWPLAN_PRECONNECTOR_H__
#define __NEWPLAN_PRECONNECTOR_H__
#include <functional>

/* structure of test parameters */
struct config_t
{
    char *dev_name;    /*device name for local devices*/
    char *server_name; /* daemon host name */
    int tcp_port;      /* daemon TCP port */
    int ib_port;       /* local IB port to work with */
    int gid_index;     /* gid index */
};

/* structure to exchange data which is needed to connect the QPs */
struct cm_con_data_t
{
    uint64_t addr;   /* Buffer address */
    uint32_t rkey;   /* Remote key */
    uint32_t qp_num; /* QP number */
    uint16_t lid;    /* LID of the IB port */
    uint8_t gid[16]; /* gid */
} __attribute__((packed));

class PreConnector
{
public:
    PreConnector() {}
    virtual ~PreConnector() {}

    virtual void pre_connect() = 0;

    virtual void print_config() = 0; //show configure

    // When new connection is estiblished, run connect_callback() to do RDMA init
    virtual void daemon_connect(std::function<void()> connect_callback) = 0;

    virtual void close_daemon() = 0;

    // Exchange QP info to establish the connection
    virtual struct cm_con_data_t exchange_qp_data(struct cm_con_data_t local_con_data) = 0;

    config_t config = {
        //default configure
        ((char *)0), //device name;
        ((char *)0), // server_name
        23333,       // tcp_port
        1,           // ib_port
        3            // gid index, default for RoCEv2
    };
};

#include <thread>
#include <functional>

#define LOCALHOST ((char *)"localhost")

class TCPSockPreConnector : public PreConnector
{
public:
    TCPSockPreConnector();
    virtual ~TCPSockPreConnector();

    //********************inherit from PreConnector************************//
    void pre_connect();
    void daemon_connect(std::function<void()> connect_callback);
    void close_daemon();
    void print_config();
    // Exchange QP info to establish the connection
    struct cm_con_data_t exchange_qp_data(struct cm_con_data_t local_con_data);

private:
    // tcp_socket
    int sock_daemon_connect(int port, std::function<void()> connect_callback);
    int sock_server_connect(int port);
    int sock_client_connect(const char *server_name, int port);
    int sock_recv(int sock_fd, size_t size, void *buf);
    int sock_send(int sock_fd, size_t size, const void *buf);
    int sock_sync_data(int sock_fd, int is_daemon, size_t size, const void *out_buf, void *in_buf);
    int sock_sync_ready(int sock_fd, int is_daemon);
    // Listen socket
    int listen_sock_;
    // Remote socket
    int remote_sock_;
    // Thread used to be as daemon
    std::unique_ptr<std::thread> daemon_thread_;
    bool daemon_run_;
};

#endif