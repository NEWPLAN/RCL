
#include <thread>
#include <chrono>
#include <glog/logging.h>
#include "rdma_client.h"
#include "ip_qos_helper.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/ip.h>
#include "pre_connector.h"
#include "rdma_adapter.h"
#include "config.h"
#include "rdma_session.h"

void RDMAClient::config_check()
{
    std::this_thread::sleep_for(std::chrono::seconds(1));

    if (this->conf_.tcp_port == 0 || this->conf_.server_name.length() == 0)
    {
        LOG(FATAL) << "Illegal server configuration:  "
                   << this->conf_.server_name << ":"
                   << this->conf_.tcp_port;
    }
    LOG(INFO) << "Client is trying to connect to "
              << this->conf_.server_name << ":"
              << this->conf_.tcp_port;

    std::this_thread::sleep_for(std::chrono::seconds(1));
}

void RDMAClient::run()
{
    do
    {
        LOG(INFO) << "Running in client";
        std::this_thread::sleep_for(std::chrono::seconds(5));

    } while (true);
}

void RDMAClient::pre_connect()
{
    LOG(INFO) << "[On] Preparing socket...";
    if (this->root_sock == 0)
    {
        this->root_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (this->root_sock <= 0)
            LOG(FATAL) << "Error of creating socket...";
        LOG(INFO) << "Creating root socket: " << this->root_sock;
    }
    newplan::IPQoSHelper::set_and_check_ip_tos(this->root_sock, 0x10);
    newplan::IPQoSHelper::set_and_check_ip_priority(this->root_sock, 4);
    LOG(INFO) << "[Done] Preparing socket...";

    LOG(INFO) << "[On] Checking config...";
    this->config_check();
    LOG(INFO) << "[Done] Checking config...";
}
void RDMAClient::connecting()
{
    LOG(INFO) << "[On] Connecting ...";
    struct sockaddr_in c_to_server;
    c_to_server.sin_family = AF_INET;
    c_to_server.sin_port = htons(this->conf_.tcp_port);
    c_to_server.sin_addr.s_addr = inet_addr(this->conf_.server_name.c_str());
    int count_try = 10 * 300; //default 300s
    do
    {
        if (connect(this->root_sock,
                    (struct sockaddr *)&c_to_server,
                    sizeof(c_to_server)) == 0)
        {
            is_connected = true;
            break; // break when successing
        }

        LOG_EVERY_N(INFO, 10) << "[" << count_try / 10
                              << "] Failed to connect: "
                              << this->conf_.server_name
                              << ":" << this->conf_.tcp_port;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } while (count_try-- > 0);
    LOG(INFO) << "[Done] Connecting ...";
}
void RDMAClient::post_connect()
{
    if (!is_connected)
        LOG(FATAL) << "failed to connect ...";

    RDMASession *client_sess = RDMASession::new_rdma_session(this->root_sock,
                                                             this->conf_.server_name,
                                                             this->conf_.tcp_port,
                                                             conf_);
    client_sess->session_init();
    client_sess->run_tests_send_side();
    LOG(FATAL) << "Error of in the client side";
}

void RDMAClient::two_channel_test(NPRDMAdapter *adpter)
{
    LOG(INFO) << "[debug] in two channel test";

    struct resources *res = adpter->get_context();

    char temp_char;
    NPRDMAPreConnector *pre_con = adpter->get_pre_connector();
    int index = 0;
    if (pre_con->sock_sync_data(1, (char *)"O", &temp_char))
        LOG(FATAL) << "sync error before RDMA ops";
    adpter->post_ctrl_recv(index);
    if (pre_con->sock_sync_data(1, (char *)"O", &temp_char))
        LOG(FATAL) << "sync error After RDMA ops";
    LOG(INFO) << "Prepared everything for benchmark\n\n\n";
    struct ibv_wc wc;
    adpter->post_data_write_with_imm(index, 0xff00 + index);

    do
    {
        // Wait for completion events.
        // If we use busy polling, this step is skipped.
        if (conf_.use_event)
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

        if (adpter->poll_completion(wc))
        {
            LOG(WARNING) << "poll completion failed\n\n\n\n\n\n\n\n";
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
            adpter->post_ctrl_recv(index);
            adpter->post_data_write_with_imm(index, 0xff00 + index);
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
            adpter->post_ctrl_recv(index);
            adpter->post_ctrl_send(index);
            break;
        }
        default:
            LOG(WARNING) << "Unknown opcode" << wc.opcode;
        }
    } while (true);
}

void RDMAClient::client_test(NPRDMAdapter *adpter)
{

    struct resources *res = adpter->get_context();
    char temp_char;
    NPRDMAPreConnector *pre_con = adpter->get_pre_connector();

    /* in both sides we expect to get a completion */
    if (adpter->poll_completion())
        LOG(FATAL) << "poll completion failed";

    /* after polling the completion we have the message in the client buffer too */
    LOG(INFO) << "[in] client recv: " << res->buf;

    /* Sync so we are sure server side has data ready before client tries to read it */
    if (pre_con->sock_sync_data(1, (char *)"R", &temp_char))
        LOG(FATAL) << "sync error before RDMA ops";

    /* Now the client performs an RDMA read and then write on server.
Note that the server has no idea these events have occured */

    /* First we read contens of server's buffer */
    if (adpter->post_send(IBV_WR_RDMA_READ))
        LOG(FATAL) << "failed to post SR 2";
    if (adpter->poll_completion())
        LOG(FATAL) << "poll completion failed 2";
    LOG(INFO) << "[in] client read: " << res->buf;
    /* Now we replace what's in the server's buffer */
    strcpy(res->buf, RDMAMSGW);
    LOG(INFO) << "[out] client write: " << res->buf;

    if (adpter->post_send(IBV_WR_RDMA_WRITE))
        LOG(FATAL) << "failed to post SR 3";
    if (adpter->poll_completion())
        LOG(FATAL) << "poll completion failed 3";

    // /* Sync so server will know that client is done mucking with its memory */
    if (pre_con->sock_sync_data(1, (char *)"W", &temp_char))
        LOG(FATAL) << "sync error after RDMA ops";

    if (adpter->resources_destroy())
        LOG(FATAL) << "failed to destroy resources";
}