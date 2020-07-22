#define _GNU_SOURCE
#include <stdio.h>
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/time.h>
#include <getopt.h>
#include <string.h>
#include "common.h"

void print_usage(char *app);
bool client_connect_with_server(struct write_lat_context *ctx,
                                char *server_ip,
                                int server_port);

int main(int argc, char *argv[])
{   
    unsigned int server_port    = DEFAULT_PORT;
    char *ib_dev_name           = NULL;
    int ib_port                 = DEFAULT_IB_PORT;
    unsigned int msg_size       = DEFAULT_MSG_SIZE;
    unsigned int iters          = DEFAULT_ITERS;
    bool inline_msg             = false;
    bool use_event              = false;
    bool validate_buf           = false;
    char *server_ip             = NULL;
    struct ibv_wc wc;

    while (1) {
        static struct option long_options[] = {
            { .name = "port",     .has_arg = 1, .val = 'p' },
		    { .name = "ib-dev",   .has_arg = 1, .val = 'd' },
		    { .name = "ib-port",  .has_arg = 1, .val = 'i' },
            { .name = "size",     .has_arg = 1, .val = 's' },
            { .name = "iters",    .has_arg = 1, .val = 'n' },
            { .name = "inline",   .has_arg = 0, .val = 'l' },
            { .name = "events",   .has_arg = 0, .val = 'e' },
            { .name = "chk",      .has_arg = 0, .val = 'c' },
            {}
        };

        int c = getopt_long(argc, argv, "p:d:i:s:n:lejc",
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
            
            case 'n':
			    iters = strtoul(optarg, NULL, 0);
			    break;
            
            case 'l':
                inline_msg = true;
                break;

            case 'e':
                use_event = true;
                break;

            case 'c':
                validate_buf = true;
                break;

            default:
                print_usage(argv[0]);
                return -1;
        }
    }

    //printf("optind %d argc %d\n", optind, argc);

    if (optind == argc - 1) {
        server_ip = strdupa(argv[optind]);

    } else {
        print_usage(argv[0]);
        return -1;
    }

    struct timeval start, end;

    struct write_lat_context prog_ctx = {
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
        .inline_msg = inline_msg,
        .use_event = use_event
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
    
    // Connect to the server 
    if (!client_connect_with_server(&prog_ctx, server_ip, server_port)) {
        fprintf(stderr, "Fail to connect to the server\n");
        goto err;
    }
    printf("Connect to the server\n");

    // Wait for the memory information from the server
    if (!wait_for_wc(&prog_ctx, &wc) || !parse_recv_wc(&wc)) {
        fprintf(stderr, "Fail to get the completed recv request\n");
        goto err;
    }

    // Get remote data plane memory information
    struct write_lat_mem *rem_mem = (struct write_lat_mem*)prog_ctx.ctrl_buf;
    print_mem(rem_mem);

    // Generate the first write request
    if (!post_data_write(&prog_ctx, rem_mem)) {
        fprintf(stderr, "Could not post write\n");
        goto err;
    }

    if (gettimeofday(&start, NULL)) {
		fprintf(stderr, "Cannot get current time\n");
		goto err;
	}

    // Wait for the completions of all the write requests
    int cnt_write_compl = 0;
    while (cnt_write_compl < iters) {
        // Wait for completion events.
        // If we use busy polling, this step is skipped.
        if (use_event) {
            struct ibv_cq *ev_cq;
			void          *ev_ctx;

            if (ibv_get_cq_event(prog_ctx.channel, &ev_cq, &ev_ctx)) {
				fprintf(stderr, "Fail to get cq_event\n");
				goto err;
			}

            if (ev_cq != prog_ctx.cq) {
                fprintf(stderr, "CQ event for unknown CQ %p\n", ev_cq);
				goto err;
            }

            ibv_ack_cq_events(prog_ctx.cq, 1);

            if (ibv_req_notify_cq(prog_ctx.cq, 0)) {
                fprintf(stderr, "Cannot request CQ notification\n");
                goto err;
            }
        }
        
        // Empty the completion queue
        while (true) {
            int ne = ibv_poll_cq(prog_ctx.cq, 1, &wc);
            if (ne < 0) {
                fprintf(stderr, "Fail to poll CQ (%d)\n", ne);
			    goto err;

            } else if (ne == 0) {
                break;
            }

            if (!parse_write_wc(&wc)) {
                goto err;
            }

            // Trigger the next write request
            if (++cnt_write_compl < iters && !post_data_write(&prog_ctx, rem_mem)) {
                fprintf(stderr, "Could not post write\n");
                goto err;
            }
        }
    }

    if (gettimeofday(&end, NULL)) {
		fprintf(stderr, "Cannot get current time\n");
		goto err;
	}

    printf("%d write requests have completed\n", iters);

    float usec = (end.tv_sec - start.tv_sec) * 1000000 +
			     (end.tv_usec - start.tv_usec);

    printf("%d iters in %.2f usec = %.2f usec/iter\n",
           iters, usec, usec / iters);

    // Generate a RDMA write with immediate request to notify the server of completion of writes
    if (!post_data_write_with_imm(&prog_ctx, rem_mem, TEST_COMPLETION)) {
        fprintf(stderr, "Could not post write with immediate\n");
        goto err;
    }

    // Wait for the completion of the write with immediate request
    if (!wait_for_wc(&prog_ctx, &wc) || !parse_write_wc(&wc)) {
        fprintf(stderr, "Fail to get the completed write with immediate request\n");
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

// The client connects with the server
bool client_connect_with_server(struct write_lat_context *ctx,
                                char *server_ip,
                                int server_port)
{
    if (!ctx) {
        return false;
    }

    // Create socket file descriptor
    int sockfd = 0;    
    if ((sockfd =  socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Socket creation error\n");
        goto err;
    }

    // Initialize server socket address
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(server_port); 
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address\n");
        goto err;
    }

    // Connect to the server
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "Connection error\n");
        goto err;
    }

    struct write_lat_dest my_dest, rem_dest;
    // Get my destination information
    if (!init_dest(&my_dest, ctx)) {
        goto err;
    }

    printf("local address: ");
    print_dest(&my_dest);

    // Send my destination information to the server
    if (!write_dest_info(sockfd, &my_dest)) {
        goto err;
    
    } else {
        printf("Send the destination information to the server\n");
    }

    // Receive the destination information from the server
    if (!read_dest_info(sockfd, &rem_dest)) {
        goto err;

    } else {
        printf("Receive the destination information from the server\n");
    }

    printf("remote address: ");
    print_dest(&rem_dest);

    // Connect QP of the remote server
    if (!connect_qp(ctx, &my_dest, &rem_dest)) {
        fprintf(stderr, "Fail to connect QP of the server\n");
        goto err;
    }

    // Post the receive request to receive memory information from the server
    if (!post_ctrl_recv(ctx)) {
        fprintf(stderr, "Fail to post the receive request\n");
        goto err;
    }

    // Tell the server that the client is ready to receive the memory information (via RDMA send)
    char ready_msg[] = READY_MSG;
    if (write_exact(sockfd, ready_msg, sizeof(ready_msg)) != sizeof(ready_msg)) {
        fprintf(stderr, "Fail to tell the server that the client is ready\n");
        goto err;
    }

    close(sockfd);
    return true;

err:
    close(sockfd);
    return false;  
}