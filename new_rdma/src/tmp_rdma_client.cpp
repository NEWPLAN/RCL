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

#include "tmp_rdma_adapter.h"

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
        NPRDMASession *new_session = NPRDMASession::new_rdma_session(pre_connector);
        this->store_session(new_session);

        new_session->do_connect(get_type() == ENDPOINT_SERVER);
        start_service(new_session);
    }
    void RDMAClient::after_connect()
    {
        LOG(INFO) << "after connect";
    }

    void RDMAClient::start_service(NPRDMASession *sess)
    {
        LOG(INFO) << "Start service";
        NPRDMAAdapter *ctx = sess->get_channel()->get_context();

        unsigned int iters = DEFAULT_ITERS;
        bool use_event = false;
        bool validate_buf = false;
        struct timeval start, end;
        struct ibv_wc wc;

        if (validate_buf)
            LOG(INFO) << "validate the buf";

        {
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
            int cnt_write_compl = 0;
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

}; // namespace newplan

static void print_usage(char *app);

bool client_connect_with_server_new(NPRDMAAdapter *ctx,
                                    char *server_ip,
                                    int server_port);

int client_main_v2(int argc, char *argv[])
{
    unsigned int iters = DEFAULT_ITERS;
    bool use_event = false;
    bool validate_buf = false;
    char *server_ip = argv[1];
    struct ibv_wc wc;

    LOG(INFO) << "Connect to Server: " << server_ip;

    struct timeval start, end;

    NPRDMAAdapter *rdma_ctx = new NPRDMAAdapter();

    if (!rdma_ctx->init_ctx())
    {
        fprintf(stderr, "Fail to initialize IB resources\n");
        return -1;
    }

    printf("Initialize IB resources\n");

    // Initialize data plane buffer content
    if (validate_buf)
    {
        for (int i = 0; i < rdma_ctx->ctx->data_buf_size; i++)
        {
            rdma_ctx->ctx->data_buf[i] = i & 0xFF;
        }
    }

    // Connect to the server
    if (!client_connect_with_server_new(rdma_ctx, server_ip, DEFAULT_PORT))
    {
        fprintf(stderr, "Fail to connect to the server\n");
        exit(-1);
    }
    printf("Connect to the server\n");

    // Wait for the memory information from the server
    if (!rdma_ctx->wait_for_wc(&wc) || !rdma_ctx->parse_recv_wc(&wc))
    {
        fprintf(stderr, "Fail to get the completed recv request\n");
        exit(-1);
    }

    // Get remote data plane memory information
    struct write_lat_mem *rem_mem = (struct write_lat_mem *)rdma_ctx->ctx->ctrl_buf;
    rdma_ctx->print_mem(rem_mem);

    // Generate the first write request
    if (!rdma_ctx->post_data_write(rem_mem))
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
    int cnt_write_compl = 0;
    while (cnt_write_compl < iters)
    {
        // Wait for completion events.
        // If we use busy polling, this step is skipped.
        if (use_event)
        {
            struct ibv_cq *ev_cq;
            void *ev_ctx;

            if (ibv_get_cq_event(rdma_ctx->ctx->channel, &ev_cq, &ev_ctx))
            {
                fprintf(stderr, "Fail to get cq_event\n");
                exit(-1);
            }

            if (ev_cq != rdma_ctx->ctx->cq)
            {
                fprintf(stderr, "CQ event for unknown CQ %p\n", ev_cq);
                exit(-1);
            }

            ibv_ack_cq_events(rdma_ctx->ctx->cq, 1);

            if (ibv_req_notify_cq(rdma_ctx->ctx->cq, 0))
            {
                fprintf(stderr, "Cannot request CQ notification\n");
                exit(-1);
            }
        }

        // Empty the completion queue
        while (true)
        {
            int ne = ibv_poll_cq(rdma_ctx->ctx->cq, 1, &wc);
            if (ne < 0)
            {
                fprintf(stderr, "Fail to poll CQ (%d)\n", ne);
                exit(-1);
            }
            else if (ne == 0)
            {
                break;
            }

            if (!rdma_ctx->parse_write_wc(&wc))
            {
                exit(-1);
            }

            // Trigger the next write request
            if (++cnt_write_compl < iters && !rdma_ctx->post_data_write(rem_mem))
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
    if (!rdma_ctx->post_data_write_with_imm(rem_mem, TEST_COMPLETION))
    {
        fprintf(stderr, "Could not post write with immediate\n");
        exit(-1);
    }

    // Wait for the completion of the write with immediate request
    if (!rdma_ctx->wait_for_wc(&wc) || !rdma_ctx->parse_write_wc(&wc))
    {
        fprintf(stderr, "Fail to get the completed write with immediate request\n");
        exit(-1);
    }

    printf("Destroy IB resources\n");
    rdma_ctx->destroy_ctx();
    return 0;
}

static void print_usage(char *app)
{
    if (!app)
    {
        return;
    }

    fprintf(stderr, "Usage: %s [options] host\n", app);
    fprintf(stderr, "Options:\n");

    fprintf(stderr, "  -p, --port=<port>         listen on/connect to port <port> (default %d)\n", DEFAULT_PORT);
    fprintf(stderr, "  -d, --ib-dev=<dev>        use IB device <dev>\n");
    fprintf(stderr, "  -i, --ib-port=<port>      use port <port> of IB device (default %d)\n", DEFAULT_IB_PORT);
    fprintf(stderr, "  -s, --size=<size>         size of message to exchange (default %d)\n", DEFAULT_MSG_SIZE);
    fprintf(stderr, "  -n, --iters=<iters>       number of exchanges (default %d)\n", DEFAULT_ITERS);
    fprintf(stderr, "  -l, --inline              inline message with the work request\n");
    fprintf(stderr, "  -e, --events              sleep on CQ events\n");
    fprintf(stderr, "  -c, --chk                 validate received buffer\n");
}

bool client_connect_with_server_new(NPRDMAAdapter *ctx,
                                    char *server_ip,
                                    int server_port)
{
    if (!ctx || !ctx->ctx)
    {
        return false;
    }

    // Create socket file descriptor
    int sockfd = 0;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "Socket creation error\n");
        exit(-1);
    }

    // Initialize server socket address
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0)
    {
        fprintf(stderr, "Invalid address\n");
        exit(-1);
    }

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        fprintf(stderr, "Connection error\n");
        exit(-1);
    }

    struct write_lat_dest my_dest, rem_dest;
    // Get my destination information
    if (!ctx->init_dest(&my_dest))
    {
        exit(-1);
    }

    printf("local address: ");
    ctx->print_dest(&my_dest);

    // Send my destination information to the server
    if (!ctx->write_dest_info(sockfd, &my_dest))
    {
        exit(-1);
    }
    else
    {
        printf("Send the destination information to the server\n");
    }

    // Receive the destination information from the server
    if (!ctx->read_dest_info(sockfd, &rem_dest))
    {
        exit(-1);
    }
    else
    {
        printf("Receive the destination information from the server\n");
    }

    printf("remote address: ");
    ctx->print_dest(&rem_dest);

    // Connect QP of the remote server
    if (!ctx->connect_qp(&my_dest, &rem_dest))
    {
        fprintf(stderr, "Fail to connect QP of the server\n");
        exit(-1);
    }

    // Post the receive request to receive memory information from the server
    if (!ctx->post_ctrl_recv())
    {
        fprintf(stderr, "Fail to post the receive request\n");
        exit(-1);
    }

    // Tell the server that the client is ready to receive the memory information (via RDMA send)
    char ready_msg[] = READY_MSG;
    if (ctx->write_exact(sockfd, ready_msg, sizeof(ready_msg)) != sizeof(ready_msg))
    {
        fprintf(stderr, "Fail to tell the server that the client is ready\n");
        exit(-1);
    }

    close(sockfd);
    return true;

err:
    close(sockfd);
    return false;
}