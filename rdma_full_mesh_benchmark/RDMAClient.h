#ifndef __RDMA_CLIENT_H__
#define __RDMA_CLIENT_H__
#include <string>
#include "RDMABase.h"
#include <thread>

class RDMAClient : public RDMABase
{
public:
    RDMAClient(RDMAAdapter &rdma_adapter);
    RDMAClient(const std::string &server_ip, const std::string &client_ip, BlockingQueue<comm_job> *q);
    ~RDMAClient();

    void setup();
    void start_service();
    void set_when_write_finished(std::function<void()> f) {todo_when_write_finished = f;}

protected:
    virtual void *poll_cq(void *_id);
    virtual void on_connection(struct rdma_cm_id *id) {}
    virtual void on_pre_conn(struct rdma_cm_id *id);
    void on_completion(struct ibv_wc *wc);
    void on_disconnect(struct rdma_cm_id *id);
    void event_loop(struct rdma_event_channel *ec);

private:
    void post_receive(uint32_t msg_id);
    void write_remote(uint32_t buffer_id,
                      uint32_t window_id,
                      uint32_t len);
    void send_next_chunk(uint32_t buffer_id, uint32_t window_id);
    void send_file_name(struct rdma_cm_id *id);
    void write_large_block(uint32_t len);
    void send_imm(uint32_t imm_data = IMM_TEST);
    void poll_job_queue();

    
private:
    void _send_loops();
    void _init();

private:
    std::string ip_addr_;

    std::thread *send_thread = nullptr;
    RDMAAdapter rdma_adapter_;
    struct sockaddr_in ser_in, local_in; /*server ip and local ip*/

    struct RDMAContext *ctx = 0;
    // 用于 write, 以及 send imm
    BlockingQueue<comm_job> *job_queue;
    std::function<void()> todo_when_write_finished = nullptr;
};

#endif