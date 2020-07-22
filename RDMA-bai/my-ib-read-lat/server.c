#define _GNU_SOURCE
#include <stdio.h>
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>
#include <string.h>
#include "common.h"

void print_usage(char *app);
bool server_connect_with_client(struct read_lat_context *ctx,
                                int server_port);

int main(int argc, char *argv[])
{   
    unsigned int server_port    = DEFAULT_PORT;
    char *ib_dev_name           = NULL;
    int ib_port                 = DEFAULT_IB_PORT;
    unsigned int msg_size       = DEFAULT_MSG_SIZE;
    bool validate_buf           = false;
    struct ibv_wc wc;
    
    while (1) {
		static struct option long_options[] = {
            { .name = "port",     .has_arg = 1, .val = 'p' },
			{ .name = "ib-dev",   .has_arg = 1, .val = 'd' },
			{ .name = "ib-port",  .has_arg = 1, .val = 'i' },
            { .name = "size",     .has_arg = 1, .val = 's' },
            { .name = "chk",      .has_arg = 0, .val = 'c' },
            {}
        };

        int c = getopt_long(argc, argv, "p:d:i:s:c",
				            long_options, NULL);
        
        //printf("%d\n", c);
        if (c == -1) {
            break;
        }

        switch (c) {
            case 'p':
                server_port = strtoul(optarg, NULL, 0);
                if (server_port > 65535) {
                    print_usage(argv[0]);
                    return -1;
                }
                break;
            
            case 'd':
                ib_dev_name = strdupa(optarg);
                break;
            
            case 'i':
                ib_port = strtol(optarg, NULL, 0);
                if (ib_port < 1) {
                    print_usage(argv[0]);
                    return -1;                    
                }
            
            case 's':
                msg_size = strtoul(optarg, NULL, 0);
                break;
                                                
            case 'c':
                validate_buf = true;
                break;

            default:
			    print_usage(argv[0]);
			    return -1;
        }
    }

    if (!ib_dev_name) {
        print_usage(argv[0]);
		return -1;
    }

    struct read_lat_context prog_ctx = {
        .ib_dev_name = ib_dev_name,
        .dev_port = ib_port,
        .gid_index = DEFAULT_GID_INDEX,
        .ctx = NULL,
        .channel = NULL,
        .pd = NULL,
        .data_mr = NULL,
        .ctrl_mr = NULL,
        .cq = NULL,
        .qp = NULL,
        .data_buf = NULL,
        .data_buf_size = msg_size,
        .ctrl_buf = NULL,
        .ctrl_buf_size = DEFAULT_CTRL_BUF_SIZE,
        .use_event = false
    };

    if (!init_ctx(&prog_ctx)) {
        fprintf(stderr, "Fail to initialize IB resources\n");
        return -1;
    }

    printf("Initialize IB resources\n");

    // Initialize data plane buffer content
    if (validate_buf) {
        for (int i = 0; i < prog_ctx.data_buf_size; i++) {
            prog_ctx.data_buf[i] = i & 0xFF;
        }
    }
    
    if (!server_connect_with_client(&prog_ctx, server_port)) {
        fprintf(stderr, "Fail to connect to the client\n");
        goto err;
    }
    printf("Connect to the client\n");

    // Post a receive request to receive the completion notification
    if (!post_ctrl_recv(&prog_ctx)) {
        goto err;
    }

    // Get my data plane memory information
    struct read_lat_mem *my_mem = (struct read_lat_mem*)prog_ctx.ctrl_buf;
    if (!init_data_mem(my_mem, &prog_ctx)) {
        goto err;
    }
    print_mem(my_mem);

    // Post a send request to send the memory information
    if (!post_ctrl_send(&prog_ctx)) {
        goto err;
    }

    // Wait for the completion of the send request
    if (!wait_for_wc(&prog_ctx, &wc) || !parse_send_wc(&wc)) {
        fprintf(stderr, "Fail to get the completed send request\n");
        goto err;
    }

    // Wait for the completion of the recerive request
    if (!wait_for_wc(&prog_ctx, &wc) || !parse_recv_wc(&wc)) {
        fprintf(stderr, "Fail to get the completed recv request\n");
        goto err;
    }

    // Parse immediate data 
    if (ntohl(wc.imm_data) != TEST_COMPLETION) {
        fprintf(stderr, "Failt to get the test completion message\n");
        goto err;
    }

    printf("Destroy IB resources\n");
    destroy_ctx(&prog_ctx);
    return 0;

err:
    printf("Destroy IB resources\n");
    destroy_ctx(&prog_ctx);
    return -1;
}

void print_usage(char *app)
{
    if (!app) {
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
bool server_connect_with_client(struct read_lat_context *ctx,
                                int server_port)
{
    if (!ctx) {
        return false;
    }

    // Create socket file descriptor
    int sockfd = 0;    
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Socket creation error\n");
        goto err;
    }

    // To allow reuse of local addresses
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR|SO_REUSEPORT, &opt, sizeof(opt))) { 
        fprintf(stderr, "Set socket option error\n");
        goto err;
    } 

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_addr.s_addr = INADDR_ANY; 
    serv_addr.sin_port = htons(server_port); 
    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) { 
        fprintf(stderr, "Bind error\n");
        goto err; 
    } 

    if (listen(sockfd, 5) < 0) { 
        fprintf(stderr, "Listen error\n");
        goto err; 
    } 

    // Accept an incoming connection
    int addrlen = sizeof(serv_addr); 
    int client_sockfd = accept(sockfd, (struct sockaddr*)&serv_addr, (socklen_t*)&addrlen);
    if (client_sockfd < 0) { 
        fprintf(stderr, "Accept error\n");
        goto err;
    }                  

    struct read_lat_dest my_dest, rem_dest;
    // Get my destination information
    if (!init_dest(&my_dest, ctx)) {
        goto err;
    }

    printf("local address: ");
    print_dest(&my_dest);

    // Read the client's destination information
    if (!read_dest_info(client_sockfd, &rem_dest)) {
        fprintf(stderr, "Fail to receive the destination information from the client\n");
        close(client_sockfd);
        goto err;

    } else {
        printf("Receive the destination information from the client\n");
    }

    printf("remote address: ");
    print_dest(&rem_dest);
    
    // Before the server sends back its destination information,
    // it should connect to the remote QP first.
    if (!connect_qp(ctx, &my_dest, &rem_dest)) {
        fprintf(stderr, "Fail to connect to the client\n");
        close(client_sockfd);
        goto err;

    } else {
        printf("Connect to the client\n");
    }

    // Send my destination information
    if (!write_dest_info(client_sockfd, &my_dest)) {
        fprintf(stderr, "Fail to send my destination information to the client\n");
        close(client_sockfd);
        goto err;
    
    } else {
        printf("Send the destination information to the client\n");
    }

    // Wait for the ready information from the client
    char buf[sizeof(READY_MSG)] = {0};
    if (read_exact(client_sockfd, buf, sizeof(buf)) != sizeof(buf) ||
        strcmp(buf, READY_MSG) != 0) {
        fprintf(stderr, "Fail to receive the ready message from the client\n");
        close(client_sockfd);
        goto err;

    } else {
        printf("Receive the ready message from the client\n");
    }

    close(client_sockfd);
    close(sockfd);
    return true;

err:
    close(sockfd);
    return false; 
}