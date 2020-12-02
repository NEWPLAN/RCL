#ifndef __NEWPLAN_RDMA_SESSION_H__
#define __NEWPLAN_RDMA_SESSION_H__
#include <iostream>
#include "pre_connector.h"
#include "rdma_adapter.h"
#include <string>
#include "config.h"
#include <glog/logging.h>
#include <thread>
#include <chrono>

class RDMASession
{
public:
    RDMASession(int, std::string, int, Config);
    virtual ~RDMASession();

public:
    void session_init();
    inline int pull_ctrl(int index)
    {
#if defined DEBUG_SINGLE_THREAD
        LOG(INFO) << "post_ctrl_recv: " << pre_con->get_peer_ip()
                  << ": " << ++in_counter;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif
        return adpter->post_ctrl_recv(index);
    }
    int push_ctrl(int index)
    {
#if defined DEBUG_SINGLE_THREAD
        LOG(INFO) << "post_ctrl_send: " << pre_con->get_peer_ip()
                  << ": " << ++out_counter;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif
        return adpter->post_ctrl_send(index);
    }
    int pull_data(int index)
    { //in the receiver side, post a ctrl flags to recv data

        return adpter->post_ctrl_send(index);
    }
    int push_data(int index)
    { // in the sender side, write data remote
#if defined DEBUG_SINGLE_THREAD
        LOG(INFO) << "post_data_write: " << pre_con->get_peer_ip()
                  << ": " << ++out_counter;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif
        return adpter->post_data_write_with_imm(index, 0xff00 + index);
    }

    int push_data(int index, int sliced_msg_size)
    { // in the sender side, write data remote
#if defined DEBUG_SINGLE_THREAD
        LOG(INFO) << "post_data_write: " << pre_con->get_peer_ip()
                  << ": " << ++out_counter;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif
        LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[out] Using sliced msg:" << sliced_msg_size;
        if (conf.DATA_MSG_SIZE % sliced_msg_size != 0)
            LOG(FATAL) << "[out] cannot slice message into block with size=" << sliced_msg_size;

        for (int msg_index = 0; msg_index < conf.DATA_MSG_SIZE / sliced_msg_size - 1; ++msg_index)
            adpter->post_data_write(index, sliced_msg_size);
        return adpter->post_data_write_with_imm(index, 0x7700 + index, sliced_msg_size);
    }

    int query_status(struct ibv_wc *, int num = 1);
    inline int peek_status(struct ibv_wc *wc, int num = 1)
    {
        num++;
        struct resources *res = adpter->get_context();
        // Wait for completion events.
        // If we use busy polling, this step is skipped.
        if (conf.use_event)
        {
            struct ibv_cq *ev_cq;
            void *ev_ctx;
            LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[pre] get CQ event";

            if (ibv_get_cq_event(res->channel, &ev_cq, &ev_ctx))
                LOG(FATAL) << "Fail to get cq_event";
            if (ev_cq != res->cq)
                LOG(FATAL) << "CQ event for unknown CQ " << ev_cq;
            ibv_ack_cq_events(res->cq, 1);
            if (ibv_req_notify_cq(res->cq, 0))
                LOG(FATAL) << "Cannot request CQ notification";
            LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[post] get CQ event";
        }
        return adpter->peek_status(wc);
    }

    void sync_data(char *, std::string);

    std::string &get_peer_ip() { return pre_con->get_peer_ip(); }

public:
    static RDMASession *new_rdma_session(int sock, std::string peer_ip, int peer_port, Config conf_)
    {
        return new RDMASession(sock, peer_ip, peer_port, conf_);
    }

    void run_tests_send_side();
    void run_tests_recv_side();

private:
    NPRDMAdapter *adpter = 0;
    NPRDMAPreConnector *pre_con = 0;
    Config conf;

public:
    uint64_t in_counter = 0;
    uint64_t out_counter = 0;
};
#endif