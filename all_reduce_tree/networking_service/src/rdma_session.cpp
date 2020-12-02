#include "rdma_session.h"
#include <thread>
#include <chrono>
#include <glog/logging.h>

RDMASession::RDMASession(int sock, std::string peer_ip, int peer_port, Config conf_)
{
    CHECK(pre_con == 0 && adpter == 0) << "all resources are new";
    pre_con = new NPRDMAPreConnector(sock, peer_ip, peer_port);
    this->conf = conf_;
    adpter = new NPRDMAdapter(this->conf);
}
RDMASession::~RDMASession()
{
}

void RDMASession::session_init()
{
    CHECK(pre_con != 0 && adpter != 0) << "all resources are created";

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    this->adpter->resources_init(pre_con);
    adpter->resources_create();
    adpter->connect_qp();
    LOG(INFO) << "Has connected with : " << pre_con->get_peer_ip();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

int RDMASession::query_status(struct ibv_wc *wc, int num)
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

    return adpter->poll_completion(wc);
}

void RDMASession::run_tests_recv_side()
{
    LOG(INFO) << " [debug] two channel ";
    //struct resources *res = adpter->get_context();

    //char temp_char;

    int index = 0;
    sync_data((char *)"0", "sync error before RDMA ops");
    this->pull_ctrl(index);
    sync_data((char *)"0", "sync error After RDMA ops");
    LOG(INFO) << "Prepared everything for benchmark\n\n\n";
    struct ibv_wc wc;
    do
    {
        if (query_status(&wc))
        {
            LOG(WARNING) << "poll completion failed";
            continue;
        }

        switch (wc.opcode)
        {
        case IBV_WC_SEND:
        {
            LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[out] request: send";
            break;
        }
        case IBV_WC_RECV:
        {
            LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[in ] request: Recv";
            break;
        }
        case IBV_WC_RDMA_WRITE:
        {
            LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[out] request: write";
            break;
        }
        case IBV_WC_RECV_RDMA_WITH_IMM:
        {
            LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[in ] request: write_with_IMM, "
                                                 << wc.imm_data;
            pull_ctrl(1);
            push_ctrl(index);
            break;
        }
        default:
            LOG_EVERY_N(INFO, 1) << "Unknown opcode" << wc.opcode;
        }

    } while (true);
}

void RDMASession::run_tests_send_side()
{
    LOG(INFO) << "[debug] in two channel test";

    int index = 0;
    sync_data((char *)"0", "sync error before RDMA ops");
    this->pull_ctrl(index);
    sync_data((char *)"0", "sync error after RDMA ops");
    LOG(INFO) << "Prepared everything for benchmark\n\n\n";
    struct ibv_wc wc;

    push_data(index);

    do
    {
        if (query_status(&wc))
        {
            LOG(WARNING) << "poll completion failed";
            continue;
        }

        switch (wc.opcode)
        {
        case IBV_WC_SEND:
        {
            LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[out][client] request: send";
            break;
        }
        case IBV_WC_RECV:
        {
            LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[in ][client] request: Recv";
            //adpter->post_ctrl_recv(index);
            //adpter->post_data_write_with_imm(index, 0xff00 + index);
            pull_ctrl(index);
            push_data(index);
            break;
        }
        case IBV_WC_RDMA_WRITE:
        {
            LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[out][client] request: write";
            break;
        }
        case IBV_WC_RECV_RDMA_WITH_IMM:
        {
            LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[in ][client] request: write_with_IMM, "
                                                 << wc.imm_data;
            // adpter->post_ctrl_recv(index);
            // adpter->post_ctrl_send(index);
            pull_ctrl(index);
            //push_ctrl(index);
            push_data(index);
            break;
        }
        default:
            LOG(WARNING) << "Unknown opcode" << wc.opcode;
        }
    } while (true);
}

void RDMASession::sync_data(char *data, std::string info)
{
    char temp_char;
    if (pre_con->sock_sync_data(1, data, &temp_char))
        LOG(FATAL) << info;
}