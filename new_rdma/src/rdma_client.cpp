#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/time.h>
#include <getopt.h>
#include <string.h>

#include "rdma_adapter.h"

#include <glog/logging.h>

#include "rdma_endpoint.h"
namespace newplan
{

    void RDMAClient::run_async()
    {
        LOG(FATAL) << "Client will never run async";
    }
    void RDMAClient::run()
    {
        LOG(INFO) << "Client runing";
        before_connect();
        do_connect();
        after_connect();
    }

    void RDMAClient::before_connect()
    {
        LOG(INFO) << "Before connect";
    }

    PreConnector *RDMAClient::pre_connecting()
    {
        struct sockaddr_in c_to_server;
        c_to_server.sin_family = AF_INET;
        c_to_server.sin_port = htons(this->port_);
        c_to_server.sin_addr.s_addr = inet_addr(this->server_ip_.c_str());

        if (is_connected_)
        {
            LOG(WARNING) << "Already connected to "
                         << server_ip_ << ":" << port_;
            return nullptr;
        }
        int count_try = 10 * 300; //default 300s

        int sock_fd = get_socket();

        if (sock_fd == 0)
        {
            LOG(FATAL) << "Error: socket can not be empty";
        }

        do
        {
            if (::connect(sock_fd, (struct sockaddr *)&c_to_server,
                          sizeof(c_to_server)) == 0)
                break; // break when successing

            LOG_EVERY_N(INFO, 10) << "[" << count_try / 10
                                  << "] Failed to connect: "
                                  << this->server_ip_ << ":" << this->port_;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } while (count_try-- > 0);

        is_connected_ = true;

        PreConnector *pre_connector = new PreConnector(sock_fd,
                                                       this->server_ip_,
                                                       this->port_);
        return pre_connector;
    }
    void RDMAClient::do_connect()
    {
        LOG(INFO) << "do connect";
        PreConnector *pre_connector = pre_connecting();
        RDMASession *new_session = RDMASession::new_rdma_session(pre_connector);
        this->store_session(new_session);

        new_session->do_connect(get_type() == ENDPOINT_SERVER);
        start_service(new_session);
    }
    void RDMAClient::after_connect()
    {
        LOG(INFO) << "after connect";
    }

    void RDMAClient::start_service_default(RDMASession *sess)
    {
        LOG(INFO) << "Starting service";
        RDMAAdapter *ctx = sess->get_channel(DATA_CHANNEL)->get_context();

        unsigned int iters = DEFAULT_ITERS;
        bool use_event = false;
        bool validate_buf = true;
        struct timeval start, end;
        struct ibv_wc wc;

        if (validate_buf)
        {
            LOG(INFO) << "validate the buf";
            for (uint32_t i = 0; i < ctx->ctx->data_buf_size; i++)
            {
                ctx->ctx->data_buf[i] = (uint8_t)(i & 0xFF);
            }
        }

        { // server send a message to client to init a flow
            if (!ctx->wait_for_wc(&wc) ||
                !ctx->parse_recv_wc(&wc))
            {
                LOG(FATAL) << "Fail to get the completed recv request";
            }

            // Get remote data plane memory information
            struct write_lat_mem *rem_mem = (struct write_lat_mem *)ctx->ctx->ctrl_buf;
            ctx->print_mem(rem_mem);

            // Generate the first write request
            if (!ctx->post_data_write(rem_mem))
            {
                fprintf(stderr, "Could not post write\n");
                exit(-1);
            }

            if (gettimeofday(&start, NULL))
            {
                fprintf(stderr, "Cannot get current time\n");
                exit(-1);
            }

            // Wait for the completions of all the write requests
            uint32_t cnt_write_compl = 0;
            while (cnt_write_compl < iters)
            {
                // Wait for completion events.
                // If we use busy polling, this step is skipped.
                if (use_event)
                {
                    struct ibv_cq *ev_cq;
                    void *ev_ctx;

                    if (ibv_get_cq_event(ctx->ctx->channel, &ev_cq, &ev_ctx))
                    {
                        fprintf(stderr, "Fail to get cq_event\n");
                        exit(-1);
                    }

                    if (ev_cq != ctx->ctx->cq)
                    {
                        fprintf(stderr, "CQ event for unknown CQ %p\n", ev_cq);
                        exit(-1);
                    }

                    ibv_ack_cq_events(ctx->ctx->cq, 1);

                    if (ibv_req_notify_cq(ctx->ctx->cq, 0))
                    {
                        fprintf(stderr, "Cannot request CQ notification\n");
                        exit(-1);
                    }
                }

                // Empty the completion queue
                while (true)
                {
                    int ne = ibv_poll_cq(ctx->ctx->cq, 1, &wc);
                    if (ne < 0)
                    {
                        fprintf(stderr, "Fail to poll CQ (%d)\n", ne);
                        exit(-1);
                    }
                    else if (ne == 0)
                    {
                        break;
                    }

                    if (!ctx->parse_write_wc(&wc))
                    {
                        exit(-1);
                    }

                    // Trigger the next write request
                    if (++cnt_write_compl < iters && !ctx->post_data_write(rem_mem))
                    {
                        fprintf(stderr, "Could not post write\n");
                        exit(-1);
                    }
                }
            }

            if (gettimeofday(&end, NULL))
            {
                fprintf(stderr, "Cannot get current time\n");
                exit(-1);
            }

            printf("%d write requests have completed\n", iters);

            float usec = (end.tv_sec - start.tv_sec) * 1000000 +
                         (end.tv_usec - start.tv_usec);

            printf("%d iters in %.2f usec = %.2f usec/iter\n",
                   iters, usec, usec / iters);

            // Generate a RDMA write with immediate request to notify the server of completion of writes
            if (!ctx->post_data_write_with_imm(rem_mem, TEST_COMPLETION))
            {
                fprintf(stderr, "Could not post write with immediate\n");
                exit(-1);
            }

            // Wait for the completion of the write with immediate request
            if (!ctx->wait_for_wc(&wc) ||
                !ctx->parse_write_wc(&wc))
            {
                fprintf(stderr, "Fail to get the completed write with immediate request\n");
                exit(-1);
            }

            printf("Destroy IB resources\n");
            ctx->destroy_ctx();
        }
    }

    void RDMAClient::start_service(RDMASession *sess)
    {
        LOG(INFO) << "Starting service";
        RDMAAdapter *data_ctx = sess->get_channel(DATA_CHANNEL)->get_context();

        bool use_event = false;
        struct ibv_wc wc;

        int first_epoch = true;
        struct write_lat_mem *rem_mem = nullptr;

        while (true)
        {
            // Wait for completion events.
            // If we use busy polling, this step is skipped.
            if (use_event)
            {
                struct ibv_cq *ev_cq;
                void *ev_ctx;

                if (ibv_get_cq_event(data_ctx->ctx->channel, &ev_cq, &ev_ctx))
                {
                    fprintf(stderr, "Fail to get cq_event\n");
                    exit(-1);
                }

                if (ev_cq != data_ctx->ctx->cq)
                {
                    fprintf(stderr, "CQ event for unknown CQ %p\n", ev_cq);
                    exit(-1);
                }

                ibv_ack_cq_events(data_ctx->ctx->cq, 1);

                if (ibv_req_notify_cq(data_ctx->ctx->cq, 0))
                {
                    fprintf(stderr, "Cannot request CQ notification\n");
                    exit(-1);
                }
            }

            // Empty the completion queue
            while (true)
            {
                if (!data_ctx->wait_for_wc(&wc))
                { //poll completion queue in a blocking approach
                    fprintf(stderr, "Fail to get the completed send request\n");
                    exit(-1);
                }

                if (wc.status != IBV_WC_SUCCESS)
                {
                    LOG(FATAL) << "Error when polling work completion: " << ibv_wc_status_str(wc.status);
                }

                switch (wc.opcode)
                {
                case IBV_WC_SEND:
                {
                    LOG_EVERY_N(INFO, 100) << "[out] request: send ";
                    break;
                }
                case IBV_WC_RECV:
                {
                    LOG_EVERY_N(INFO, 100) << "[in ] request: Recv";

                    if (!data_ctx->post_ctrl_recv())
                    { // Post the receive request to receive memory information from the server
                        LOG(FATAL) << "Fail to post the receive request";
                    }
                    if (first_epoch)
                    { // server send a message to client to init a flow
                        first_epoch = false;
                        // Get remote data plane memory information
                        rem_mem = (struct write_lat_mem *)data_ctx->ctx->ctrl_buf;
                        data_ctx->print_mem(rem_mem);
                    }
                    // Trigger the next write request
                    if (!data_ctx->post_data_write_with_imm(rem_mem, data_ctx->ctx->data_buf_size))
                    {
                        fprintf(stderr, "Could not post write\n");
                        exit(-1);
                    }
                    //LOG(INFO) << "Having sent data to server";
                    break;
                }
                case IBV_WC_RDMA_WRITE:
                {
                    LOG_EVERY_N(INFO, 100) << "[out] request: write";
                    break;
                }
                case IBV_WC_RECV_RDMA_WITH_IMM:
                {
                    LOG_EVERY_N(INFO, 100) << "[in ] request: write_with_IMM, "
                                           << ntohl(wc.imm_data);
                    break;
                }
                default:
                    LOG(INFO) << "Unknown opcode" << wc.opcode;
                }
            }
        }

        printf("Destroy IB resources\n");
        data_ctx->destroy_ctx();
    }
}; // namespace newplan
