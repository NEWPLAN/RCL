#include "rdma_server.h"
#include "rdma_client.h"
#include "pre_connector.h"
#include <thread>
#include <iostream>
#include <chrono>
#include <future>
#include "tmp_rdma_adapter.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>

void callback_server(void *args)
{
    NPRDMAAdapter *rdma_adapter = (NPRDMAAdapter *)args;

    //std::thread *loop_thread = new std::thread([=]() {
    struct ibv_wc wc;
    if (!rdma_adapter->post_ctrl_send())
    {
        LOG(FATAL) << "error of post send";
    }
    if (!rdma_adapter->wait_for_wc(&wc) ||
        !rdma_adapter->parse_send_wc(&wc))
    {
        LOG(FATAL) << "error of wait for wc";
    }
    if (ntohl(wc.imm_data) != TEST_COMPLETION)
    {
        LOG(FATAL) << "error of test connection";
    }

    printf("Destroy IB resources\n");
    rdma_adapter->destroy_ctx();

    do
    {

        LOG(INFO) << "sleep server";
        std::this_thread::sleep_for(std::chrono::seconds(1));

    } while (true);
    //LOG(INFO) << "cannot be here";
    //});
    //loop_thread->detach();
    //std::async([=]() { loop_thread->detach(); });
}

void callback_client(void *args)
{
    NPRDMAAdapter *rdma_adapter = (NPRDMAAdapter *)args;

    // std::thread *loop_thread = new std::thread([=]() {
    struct ibv_wc wc;
    unsigned int iters = DEFAULT_ITERS;
    bool use_event = false;
    bool validate_buf = false;
    struct timeval start, end;

    if (!rdma_adapter->wait_for_wc(&wc) ||
        !rdma_adapter->parse_recv_wc(&wc))
    {
        LOG(FATAL) << "error of wait for wc";
    }

    // Get remote data plane memory information
    struct write_lat_mem *rem_mem = (struct write_lat_mem *)rdma_adapter->ctx->ctrl_buf;
    rdma_adapter->print_mem(rem_mem);

    // Generate the first write request
    if (!rdma_adapter->post_data_write(rem_mem))
    {
        LOG(FATAL) << "Could not post write";
    }
    if (gettimeofday(&start, NULL))
    {
        LOG(FATAL) << "Cannot get current time";
    }

    // Wait for the completions of all the write requests
    int cnt_write_compl = 0;
    while (cnt_write_compl < iters)
    {
        // Wait for completion events.
        // If we use busy polling, this step is skipped.
        if (use_event)
        {
            struct ibv_cq *ev_cq;
            void *ev_ctx;

            if (ibv_get_cq_event(rdma_adapter->ctx->channel, &ev_cq, &ev_ctx))
            {
                fprintf(stderr, "Fail to get cq_event\n");
                exit(-1);
            }

            if (ev_cq != rdma_adapter->ctx->cq)
            {
                fprintf(stderr, "CQ event for unknown CQ %p\n", ev_cq);
                exit(-1);
            }

            ibv_ack_cq_events(rdma_adapter->ctx->cq, 1);

            if (ibv_req_notify_cq(rdma_adapter->ctx->cq, 0))
            {
                fprintf(stderr, "Cannot request CQ notification\n");
                exit(-1);
            }
        }

        // Empty the completion queue
        while (true)
        {
            int ne = ibv_poll_cq(rdma_adapter->ctx->cq, 1, &wc);
            if (ne < 0)
            {
                fprintf(stderr, "Fail to poll CQ (%d)\n", ne);
                exit(-1);
            }
            else if (ne == 0)
            {
                break;
            }

            if (!rdma_adapter->parse_write_wc(&wc))
            {
                exit(-1);
            }

            // Trigger the next write request
            if (++cnt_write_compl < iters && !rdma_adapter->post_data_write(rem_mem))
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
    if (!rdma_adapter->post_data_write_with_imm(rem_mem, TEST_COMPLETION))
    {
        LOG(FATAL) << "Could not post write with immediate";
    }

    // Wait for the completion of the write with immediate request
    if (!rdma_adapter->wait_for_wc(&wc) || !rdma_adapter->parse_write_wc(&wc))
    {
        LOG(FATAL) << "Fail to get the completed write with immediate request";
    }

    printf("Destroy IB resources\n");
    rdma_adapter->destroy_ctx();

    do
    {
        LOG(INFO) << "sleep client";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    } while (true);
    //});
}

extern int server_main_v2(int argc, char *argv[]);
extern int client_main_v2(int argc, char *argv[]);

#include "rdma_endpoint.h"
int main(int argc, char *argv[])
{
    // if (argc < 2)
    // {

    //     RDMAServer *r_server = new RDMAServer();
    //     r_server->register_cb_after_pre_connect(&callback_server);
    //     r_server->start_service_async();
    //     r_server->exit_on_error();
    //     LOG(INFO) << "Cannot be here";
    // }

    // else
    // {
    //     std::string server_ip = std::string(argv[1]);
    //     int server_port = atoi(argv[2]);
    //     RDMAClient *r_client = new RDMAClient();
    //     r_client->register_cb_after_pre_connect(&callback_client);
    //     r_client->connect(server_ip, server_port);
    //     r_client->exit_on_error();
    // }
    // return 0;

    if (argc < 2)
    {
        newplan::RDMAEndpoint *r_server = new newplan::RDMAServer(1251);
        LOG(INFO) << "Server: " << newplan::RDMA_TYPE_STRING[r_server->get_type()];
        {
            // server_main_v2(argc, argv);
        }
        r_server->run_async();
    }
    else
    {
        newplan::RDMAEndpoint *r_client = new newplan::RDMAClient(1251, argv[1]);
        LOG(INFO) << "Client: " << newplan::RDMA_TYPE_STRING[r_client->get_type()];
        {
            //client_main_v2(argc, argv);
        }

        r_client->run();
    }

    // PreConnector *pre_connector = new PreConnector(121, "0.0.0.0", 2345);
}