#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>
#include <string.h>

#include "rdma_adapter.h"

#include "rdma_endpoint.h"
namespace newplan
{
    void RDMAServer::run_async()
    {
        LOG(INFO) << "Server runing async";
        before_connect();
        do_connect();
        after_connect();
    }

    void RDMAServer::run()
    {
        LOG(INFO) << "Server runing ";
    }

    void RDMAServer::before_connect()
    {
        LOG(INFO) << "Before connect";

        short listen_sock = get_socket();
        if (listen_sock != 0)
        { // bind to socket and accept new connection requests;
            struct sockaddr_in sin;
            memset(&sin, 0, sizeof(sin));
            sin.sin_family = AF_INET;
            sin.sin_addr.s_addr = inet_addr(server_ip_.c_str());
            sin.sin_port = htons(port_);

            if (bind(listen_sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
            {
                LOG(FATAL) << "Error when binding to socket";
            }
            if (listen(listen_sock, 1024) < 0)
            {
                LOG(FATAL) << "Error when listen on socket";
            }
            LOG(INFO) << "The server is waiting for connections";
        }
        else
        {
            LOG(FATAL) << "Error of initing the listen socket";
        }
    }
    void RDMAServer::do_connect()
    {
        LOG(INFO) << "do connect";
        short listen_sock = get_socket();

        do //loop wait and build new rdma connections
        {
            struct sockaddr_in cin;
            socklen_t len = sizeof(cin);
            int client_fd;

            if ((client_fd = accept(listen_sock, (struct sockaddr *)&cin, &len)) == -1)
            {
                LOG(FATAL) << "Error of accepting new connection";
            }

            std::string client_ip = std::string(inet_ntoa(cin.sin_addr));
            int client_port = htons(cin.sin_port);
            LOG(INFO) << "receive a connecting request from " << client_ip << ":" << client_port;
            PreConnector *pre_connector = new PreConnector(client_fd, client_ip, client_port); //building new pre_connector here;
            RDMASession *new_session = RDMASession::new_rdma_session(pre_connector);
            this->store_session(new_session);

            { // move to the tread queue;
                auto connection_thread = new std::thread([=]() {
                    new_session->do_connect(get_type() == ENDPOINT_SERVER);
                    start_service(new_session);
                });
                connection_threads.push_back(connection_thread);
            }
        } while (true);
        LOG(INFO) << "exit of accepting new connection";
    }
    void RDMAServer::after_connect()
    {
        LOG(INFO) << "after connect";
    }

    void RDMAServer::start_service(RDMASession *sess)
    {
        LOG(INFO) << "Server is starting the rdma service";
        bool validate_buf = false;

        struct ibv_wc wc;
        RDMAAdapter *ctx = sess->get_channel()->get_context();

        {
            validate_buf = true;
        }

        if (validate_buf)
            LOG(INFO) << "validate the buf";

        {
            // Post a receive request to receive the completion notification
            if (!ctx->post_ctrl_recv())
            {
                exit(-1);
            }

            // Get my data plane memory information
            struct write_lat_mem *my_mem = (struct write_lat_mem *)ctx->ctx->ctrl_buf;
            if (!ctx->init_data_mem(my_mem))
            {
                exit(-1);
            }
            ctx->print_mem(my_mem);

            // Post a send request to send the memory information
            if (!ctx->post_ctrl_send())
            {
                exit(-1);
            }

            // Wait for the completion of the send request
            if (!ctx->wait_for_wc(&wc) || !ctx->parse_send_wc(&wc))
            {
                fprintf(stderr, "Fail to get the completed send request\n");
                exit(-1);
            }

            // Wait for the completion of the recerive request
            if (!ctx->wait_for_wc(&wc) || !ctx->parse_recv_with_imm_wc(&wc))
            {
                fprintf(stderr, "Fail to get the completed recv request\n");
                exit(-1);
            }

            // Parse immediate data
            if (ntohl(wc.imm_data) != TEST_COMPLETION)
            {
                fprintf(stderr, "Failt to get the test completion message\n");
                exit(-1);
            }

            // Validate received buffer content
            if (validate_buf)
            {
                for (uint32_t i = 0; i < ctx->ctx->data_buf_size; i++)
                {
                    if (ctx->ctx->data_buf[i] != (char)(i & 0xFF))
                    {
                        fprintf(stderr, "Invalid data in byte %d\n", i);
                    }
                }
            }

            printf("Destroy IB resources\n");
            ctx->destroy_ctx();
        }
    }

}; // namespace newplan
