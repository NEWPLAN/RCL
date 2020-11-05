#ifndef __RDMA_BASE_H__
#define __RDMA_BASE_H__

#include <vector>
#include <string>
#include <pthread.h>
#include <functional>
#include <map>

#include <rdma/rdma_cma.h>
#include <glog/logging.h>
#include "blockQueue.h"

#define BUFFER_SIZE (16 * 1 * 1024)
#define WINDOWS_NUM 256
#define MAX_DATA_IN_FLIGHT 128

//#define WINDOW_SIZE (BUFFER_SIZE * MAX_DATA_IN_FLIGHT)
//****************************************************************
// |0|1|2|...|MAX_DATA-1|.................|0|1|2|...|MAX_DATA-1|
// |---- WIN_ID = 0 ----|-- WIN_ID = XX --|-- WIN_ID = 255 ----|
//****************************************************************

#define BATCH_MSG 48
#define MSG_NUM_OFFEST (BATCH_MSG + 1)
#define WINDOW_ID_OFFEST BATCH_MSG

//******switch*************
//#define DEBUG_REVERSE_ORDER

const unsigned short SERVER_PORT = 12345;
const unsigned short CONTROL_PORT = 12346;

enum message_id
{
    MSG_INVALID = 0,
    MSG_MR,
    MSG_READY,
    MSG_DONE
};

enum work_request_id
{
    WR_WRITE_LARGE_BLOCK = 1248,
    WR_SEND_ONLY_IMM,
    WR_WRITE_WITH_IMM
};

enum imm_id
{
    NO_IMM = 1500,
    IMM_TEST,
    IMM_MR,
    IMM_SHOW_CONNECTION_INFO,
    IMM_CLIENT_WRITE_START,
    IMM_CLIENT_SEND_DONE
};

struct comm_job
{
    enum _type
    {
        WRITE, SEND_IMM
    } type;
    uint32_t data;
    comm_job(const _type &t, const uint32_t &d): type(t), data(d){}
};

struct message
{
    int id;
    union {
        struct
        {
            uint64_t addr;
            uint32_t rkey;
        } mr;
        //short batch_index[4];
        uint8_t batch_index[BATCH_MSG + 2];
    } data;
};

struct ConnectionInfo
{
    char local_addr[48];
    char peer_addr[48];
    short local_port;
    short peer_port;
};

struct RDMAContext
{
    struct ibv_context *ibv_ctx = 0;
    struct ibv_pd *pd = 0;
    struct ibv_cq *cq = 0;
    struct ibv_comp_channel *comp_channel = 0;

    //register buffer for remote to write
    uint8_t *buffer = 0;
    struct ibv_mr *buffer_mr = 0;

    //register message mem is used for command channel
    struct message *msg = 0;
    struct ibv_mr *msg_mr = 0;

    //store peer information
    uint64_t peer_addr = 0;
    uint32_t peer_rkey = 0;

    // connection management handle
    struct rdma_cm_id *id = 0;
    size_t recv_bytes = 0;
    char connection_id[20] = {0};
    int client_index = -1;

    uint32_t window_id = 0;

    struct ConnectionInfo ctx_info;

    struct timeval start;
};

#include <iostream>
struct RDMAAdapter
{
    unsigned short server_port;
    struct rdma_event_channel *event_channel;
    struct rdma_cm_id *listener;
    std::vector<rdma_cm_id *> recv_rdma_cm_id;

    std::string server_ip;
    std::string client_ip;
    RDMAAdapter()
    {
        server_port = 12345;
        server_ip = "";
        client_ip = "";
    }
    // DictXiong: 为什么这里不直接传 string?
    void set_server_ip(const char *_server_ip)
    {
        server_ip = _server_ip;
        LOG(INFO) << "Server IP: " << server_ip;
    }
    void set_client_ip(const char *_client_ip)
    {
        client_ip = _client_ip;
        LOG(INFO) << "Client IP: " << client_ip;
    }
    void set_server_port(unsigned short port)
    {
        server_port = port;
        LOG(INFO) << "Server Port: " << port;
    }
    std::string get_client_ip() { return client_ip; }
    std::string get_server_ip() { return server_ip; }
};

#define TIMEOUT_IN_MS 500
#define TEST_NZ(x)                                               \
    do                                                           \
    {                                                            \
        if ((x))                                                 \
        {                                                        \
            printf("[%s:%d]: ", __FILE__, __LINE__);             \
            rc_die("error: " #x " failed (returned non-zero)."); \
        }                                                        \
    } while (0)
#define TEST_Z(x)                                                 \
    do                                                            \
    {                                                             \
        if (!(x))                                                 \
        {                                                         \
            printf("[%s:%d]: ", __FILE__, __LINE__);              \
            rc_die("error: " #x " failed (returned zero/null)."); \
        }                                                         \
    } while (0)

class RDMAConnector
{
public:
    RDMAConnector() {}
    ~RDMAConnector() {}

public:
    struct ibv_pd *get_protect_domain() { return this->pd; }
    struct ibv_cq *get_completion_queue() { return this->cq; }

private:
    struct ibv_context *ibv_ctx = 0;
    struct ibv_pd *pd = 0;
    struct ibv_cq *cq = 0;
    struct ibv_comp_channel *comp_channel = 0;

    //register buffer for remote to write
    char *buffer = 0;
    struct ibv_mr *buffer_mr = 0;
};

class RDMABase
{
public:
    RDMABase() {}
    virtual ~RDMABase() {}
    void show_performance(int time_duration = 1);
    void add_performance(size_t data_num);
    // 若添加, 则返回 true; 若覆盖, 则返回 false.
    bool bind_recv_imm(uint32_t imm, std::function<void(ibv_wc*)> func);

protected:
    virtual void on_disconnect(struct rdma_cm_id *id) = 0;
    virtual void on_connection(struct rdma_cm_id *id) = 0;
    virtual void on_pre_conn(struct rdma_cm_id *id) = 0;
    virtual void on_imm_recv(struct ibv_wc *wc);

    virtual void build_connection(struct rdma_cm_id *id);
    virtual void build_qp_attr(struct rdma_cm_id *id,
                               struct ibv_qp_init_attr *qp_attr);
    virtual void build_context(struct rdma_cm_id *id);
    virtual void build_params(struct rdma_conn_param *params);

    virtual struct RDMAContext *get_or_build_ctx(struct rdma_cm_id *id);

    virtual void rc_die(const char *reason);
    virtual void log_info(const char *format, ...);

    
public:
    virtual struct ibv_pd *rc_get_pd(struct rdma_cm_id *id);
    virtual void set_tos(uint8_t t) {tos = t;}
    

private:
    RDMAAdapter *rdma_adapter_;
    size_t recv_count_ = 0;
    // 消息绑定. 在 poll_cq 里面, 对于所有接收到的数据, 其 imm_data 对应要执行的操作. RDMABase 负责收集和存储, 并提供 wc 处理函数. C/S 应当在知道数据有 imm 时调用. 
    std::map<uint32_t, std::function<void(ibv_wc*)> > recv_imm_binding;

protected:
    // traffic class configuration
    // https://blog.csdn.net/sunshuying1010/article/details/103661289
    uint8_t tos = 0;
};

#endif