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

#include "tmp_rdma_adapter.h"

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
            NPRDMASession *new_session = NPRDMASession::new_rdma_session(pre_connector);
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

    void RDMAServer::start_service(NPRDMASession *sess)
    {
        LOG(INFO) << "Server is starting the rdma service";
        bool validate_buf = false;
        struct ibv_wc wc;
        NPRDMAAdapter *ctx = sess->get_channel()->get_context();

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
                for (int i = 0; i < ctx->ctx->data_buf_size; i++)
                {
                    if (ctx->ctx->data_buf[i] != (i & 0xFF))
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

static void print_usage(char *app);

bool server_connect_with_client_new(NPRDMAAdapter *ctx,
                                    int server_port);

int server_main_v2(int argc, char *argv[])
{

    bool validate_buf = false;
    struct ibv_wc wc;
    NPRDMAAdapter *rdma_ctx = new NPRDMAAdapter();

    if (!rdma_ctx->init_ctx())
    {
        fprintf(stderr, "Fail to initialize IB resources\n");
        return -1;
    }

    printf("Initialize IB resources\n");

    if (!server_connect_with_client_new(rdma_ctx, DEFAULT_PORT))
    {
        fprintf(stderr, "Fail to connect to the client\n");
        exit(-1);
    }
    printf("Connect to the client\n");

    // Post a receive request to receive the completion notification
    if (!rdma_ctx->post_ctrl_recv())
    {
        exit(-1);
    }

    // Get my data plane memory information
    struct write_lat_mem *my_mem = (struct write_lat_mem *)rdma_ctx->ctx->ctrl_buf;
    if (!rdma_ctx->init_data_mem(my_mem))
    {
        exit(-1);
    }
    rdma_ctx->print_mem(my_mem);

    // Post a send request to send the memory information
    if (!rdma_ctx->post_ctrl_send())
    {
        exit(-1);
    }

    // Wait for the completion of the send request
    if (!rdma_ctx->wait_for_wc(&wc) || !rdma_ctx->parse_send_wc(&wc))
    {
        fprintf(stderr, "Fail to get the completed send request\n");
        exit(-1);
    }

    // Wait for the completion of the recerive request
    if (!rdma_ctx->wait_for_wc(&wc) || !rdma_ctx->parse_recv_with_imm_wc(&wc))
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
        for (int i = 0; i < rdma_ctx->ctx->data_buf_size; i++)
        {
            if (rdma_ctx->ctx->data_buf[i] != (i & 0xFF))
            {
                fprintf(stderr, "Invalid data in byte %d\n", i);
            }
        }
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

    fprintf(stderr, "Usage: %s [options]\n", app);
    fprintf(stderr, "Options:\n");

    fprintf(stderr, "  -p, --port=<port>         listen on/connect to port <port> (default %d)\n", DEFAULT_PORT);
    fprintf(stderr, "  -d, --ib-dev=<dev>        use IB device <dev>\n");
    fprintf(stderr, "  -i, --ib-port=<port>      use port <port> of IB device (default %d)\n", DEFAULT_IB_PORT);
    fprintf(stderr, "  -s, --size=<size>         size of message to exchange (default %d)\n", DEFAULT_MSG_SIZE);
    fprintf(stderr, "  -c, --chk                 validate received buffer\n");
}

// The server connects with the client
bool server_connect_with_client_new(NPRDMAAdapter *ctx,
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

    // To allow reuse of local addresses
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        fprintf(stderr, "Set socket option error\n");
        exit(-1);
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(server_port);
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        fprintf(stderr, "Bind error\n");
        exit(-1);
    }

    if (listen(sockfd, 5) < 0)
    {
        fprintf(stderr, "Listen error\n");
        exit(-1);
    }

    // Accept an incoming connection
    int addrlen = sizeof(serv_addr);
    int client_sockfd = accept(sockfd, (struct sockaddr *)&serv_addr, (socklen_t *)&addrlen);
    if (client_sockfd < 0)
    {
        fprintf(stderr, "Accept error\n");
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

    // Read the client's destination information
    if (!ctx->read_dest_info(client_sockfd, &rem_dest))
    {
        fprintf(stderr, "Fail to receive the destination information from the client\n");
        close(client_sockfd);
        exit(-1);
    }
    else
    {
        printf("Receive the destination information from the client\n");
    }

    printf("remote address: ");
    ctx->print_dest(&rem_dest);

    // Before the server sends back its destination information,
    // it should connect to the remote QP first.
    if (!ctx->connect_qp(&my_dest, &rem_dest))
    {
        fprintf(stderr, "Fail to connect to the client\n");
        close(client_sockfd);
        exit(-1);
    }
    else
    {
        printf("Connect to the client\n");
    }

    // Send my destination information
    if (!ctx->write_dest_info(client_sockfd, &my_dest))
    {
        fprintf(stderr, "Fail to send my destination information to the client\n");
        close(client_sockfd);
        exit(-1);
    }
    else
    {
        printf("Send the destination information to the client\n");
    }

    // Wait for the ready information from the client
    char buf[sizeof(READY_MSG)] = {0};
    if (ctx->read_exact(client_sockfd, buf, sizeof(buf)) != sizeof(buf) ||
        strcmp(buf, READY_MSG) != 0)
    {
        fprintf(stderr, "Fail to receive the ready message from the client\n");
        close(client_sockfd);
        exit(-1);
    }
    else
    {
        printf("Receive the ready message from the client\n");
    }

    close(client_sockfd);
    close(sockfd);
    return true;

err:
    close(sockfd);
    return false;
}