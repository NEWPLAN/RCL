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

bool parse_single_wc(struct ibv_wc *wc);
void print_usage(char *app);

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

        int c = getopt_long(argc, argv, "p:d:i:s:n:lec",
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

    struct send_lat_context prog_ctx = {
        .ib_dev_name = ib_dev_name,
        .dev_port = ib_port,
        .gid_index = DEFAULT_GID_INDEX,
        .ctx = NULL,
        .channel = NULL,
        .pd = NULL,
        .mr = NULL,
        .cq = NULL,
        .qp = NULL,
        .buf = NULL,
        .buf_size = msg_size,
        .inline_msg = inline_msg,
        .use_event = use_event
    };

    if (!init_ctx(&prog_ctx)) {
        fprintf(stderr, "Fail to initialize IB resources\n");
        return -1;
    }

    printf("Initialize IB resources\n");

    // Initialize buffer content
    if (validate_buf) {
        for (int i = 0; i < prog_ctx.buf_size; i++) {
            prog_ctx.buf[i] = i & 0xFF;
        }
    }
    
    struct send_lat_dest my_dest, rem_dest;
    
    if (!init_dest(&my_dest, &prog_ctx)) {
        goto err;
    }

    printf("local address: ");
    print_dest(&my_dest);

    // Exchange destination information
    if (!client_exch_dest(server_ip, server_port, &my_dest, &rem_dest)) {
        fprintf(stderr, "Fail to exchange destination information\n");
        goto err;
    }

    printf("remote address: ");
    print_dest(&rem_dest);

    // Connect to the server 
    if (!connect_qp(&prog_ctx, &my_dest, &rem_dest)) {
        fprintf(stderr, "Fail to connect to the server\n");
        goto err;
    }

    printf("Connect to the server\n");
    
    if (!post_send(&prog_ctx)) {
        fprintf(stderr, "Could not post send\n");
        goto err;
    }

    if (gettimeofday(&start, NULL)) {
	    fprintf(stderr, "Cannot get current time\n");
	    goto err;
    }

    //printf("Post send\n");

    // # of send requests that have completed.
    int cnt_send_compl = 0;
    struct ibv_wc wc;
    int ne;

    while (cnt_send_compl < iters) {
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
            ne = ibv_poll_cq(prog_ctx.cq, 1, &wc);
            if (ne < 0) {
                fprintf(stderr, "Fail to poll CQ (%d)\n", ne);
			    goto err;

            } else if (ne == 0) {
                break;
            }

            if (!parse_single_wc(&wc)) {
                goto err;
            }

            // Trigger the next send request
            if (++cnt_send_compl < iters && !post_send(&prog_ctx)) {
                fprintf(stderr, "Could not post send\n");
                goto err;
            }
        }
    }
    
    if (gettimeofday(&end, NULL)) {
		fprintf(stderr, "Cannot get current time\n");
		goto err;
	}

    printf("%d send requests have completed\n", iters);

    float usec = (end.tv_sec - start.tv_sec) * 1000000 +
			     (end.tv_usec - start.tv_usec);

    printf("%d iters in %.2f usec = %.2f usec/iter\n",
           iters, usec, usec / iters);


    printf("Destroy IB resources\n");
    destroy_ctx(&prog_ctx);
    return 0;

err:
    printf("Destroy IB resources\n");
    destroy_ctx(&prog_ctx);
    return -1;
}

bool parse_single_wc(struct ibv_wc *wc)
{
    if (wc->status != IBV_WC_SUCCESS) {
        fprintf(stderr, "Work request status is %s\n", ibv_wc_status_str(wc->status));
        return false;
    }

    if (wc->opcode != IBV_WC_SEND) {
        fprintf(stderr, "Work request opcode is not IBV_WC_SEND (%d)\n", wc->opcode);
        return false;
    }

    return true;
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