#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <malloc.h>
#include "common.h"

// In a list of IB devices (dev_list), given a IB device's name
// (ib_dev_name), the function returns its ID.
static inline int ib_dev_id_by_name(char *ib_dev_name,
                                    struct ibv_device **dev_list,
                                    int num_devices)
{
    for (int i = 0; i < num_devices; i++)
    {
        if (strcmp(ibv_get_device_name(dev_list[i]), ib_dev_name) == 0)
        {
            return i;
        }
    }

    return -1;
}

bool init_ctx(struct send_lat_context *ctx)
{
    if (!ctx)
    {
        goto err;
    }

    struct ibv_device **dev_list = NULL;
    int num_devices;

    // Get IB device list
    dev_list = ibv_get_device_list(&num_devices);
    if (!dev_list)
    {
        fprintf(stderr, "Fail to get IB device list\n");
        goto err;
    }
    else if (num_devices == 0)
    {
        fprintf(stderr, "No IB devices found\n");
        goto clean_dev_list;
    }

    int ib_dev_id = -1;
    if (ctx->ib_dev_name == 0)
    {
        ib_dev_id = 0;
        ctx->ib_dev_name = strdup(ibv_get_device_name(dev_list[0]));
    }
    else
        ib_dev_id = ib_dev_id_by_name(ctx->ib_dev_name, dev_list, num_devices);
    printf("Using device: %s\n", ctx->ib_dev_name);
    if (ib_dev_id < 0)
    {
        fprintf(stderr, "Fail to find IB device %s\n", ctx->ib_dev_name);
        goto clean_dev_list;
    }

    // Create a context for the RDMA device
    ctx->ctx = ibv_open_device(dev_list[ib_dev_id]);
    if (ctx->ctx)
    {
        printf("Open IB device %s\n", ibv_get_device_name(dev_list[ib_dev_id]));
    }
    else
    {
        fprintf(stderr, "Fail to open IB device %s\n", ibv_get_device_name(dev_list[ib_dev_id]));
        goto clean_dev_list;
    }

    // Create a completion channel
    if (ctx->use_event)
    {
        ctx->channel = ibv_create_comp_channel(ctx->ctx);
        if (!(ctx->channel))
        {
            fprintf(stderr, "Cannot create completion channel\n");
            goto clean_device;
        }
    }
    else
    {
        ctx->channel = NULL;
    }

    // Allocate protection domain
    ctx->pd = ibv_alloc_pd(ctx->ctx);
    if (!(ctx->pd))
    {
        fprintf(stderr, "Fail to allocate protection domain\n");
        goto clean_comp_channel;
    }

    // Allocate memory
    ctx->buf = (char *)memalign(sysconf(_SC_PAGESIZE), ctx->buf_size);
    if (!(ctx->buf))
    {
        fprintf(stderr, "Fail to allocate memory\n");
        goto clean_pd;
    }

    int access_flags = IBV_ACCESS_LOCAL_WRITE;
    ctx->mr = ibv_reg_mr(ctx->pd, ctx->buf, ctx->buf_size, access_flags);

    if (!(ctx->mr))
    {
        fprintf(stderr, "Fail to register memory region\n");
        goto clean_buffer;
    }

    // Query device attributes
    if (ibv_query_device(ctx->ctx, &(ctx->dev_attr)) != 0)
    {
        fprintf(stderr, "Fail to query device attributes\n");
        goto clean_mr;
    }

    // Query port attributes
    if (ibv_query_port(ctx->ctx, ctx->dev_port, &(ctx->port_attr)) != 0)
    {
        fprintf(stderr, "Fail to query port attributes\n");
        goto clean_mr;
    }

    // Create a completion queue
    ctx->cq = ibv_create_cq(ctx->ctx, 4096 * 10, NULL, ctx->channel, 0);
    if (!(ctx->cq))
    {
        fprintf(stderr, "Fail to create the completion queue\n");
        goto clean_mr;
    }

    if (ctx->use_event)
    {
        if (ibv_req_notify_cq(ctx->cq, 0))
        {
            fprintf(stderr, "Cannot request CQ notification\n");
            goto clean_cq;
        }
    }
    printf("Max sq %d, rq: %d\n", ctx->dev_attr.max_qp_wr, ctx->dev_attr.max_qp_wr);
    // Create a queue pair (QP)
    struct ibv_qp_attr attr;
    struct ibv_qp_init_attr init_attr = {
        .send_cq = ctx->cq,
        .recv_cq = ctx->cq,
        .cap = {
            .max_send_wr = 1024,
            .max_recv_wr = 1024,
            .max_send_sge = 1,
            .max_recv_sge = 1,
        },
        .qp_type = IBV_QPT_RC,
    };

    ctx->qp = ibv_create_qp(ctx->pd, &init_attr);
    if (!(ctx->qp))
    {
        fprintf(stderr, "Fail to create QP\n");
        goto clean_cq;
    }

    ctx->send_flags = IBV_SEND_SIGNALED;
    if (ctx->inline_msg)
    {
        ibv_query_qp(ctx->qp, &attr, IBV_QP_CAP, &init_attr);

        if (init_attr.cap.max_inline_data >= ctx->buf_size)
        {
            ctx->send_flags |= IBV_SEND_INLINE;
        }
        else
        {
            fprintf(stderr, "Fail to set IBV_SEND_INLINE because max inline data size is %d\n",
                    init_attr.cap.max_inline_data);
        }
    }

    attr.qp_state = IBV_QPS_INIT;
    attr.pkey_index = 0;
    attr.port_num = ctx->dev_port;
    attr.qp_access_flags = 0;

    if (ibv_modify_qp(ctx->qp, &attr,
                      IBV_QP_STATE |
                          IBV_QP_PKEY_INDEX |
                          IBV_QP_PORT |
                          IBV_QP_ACCESS_FLAGS))
    {

        fprintf(stderr, "Fail to modify QP to INIT\n");
        goto clean_qp;
    }

    ibv_free_device_list(dev_list);
    return true;

clean_qp:
    ibv_destroy_qp(ctx->qp);

clean_cq:
    ibv_destroy_cq(ctx->cq);

clean_mr:
    ibv_dereg_mr(ctx->mr);

clean_buffer:
    free(ctx->buf);

clean_pd:
    ibv_dealloc_pd(ctx->pd);

clean_comp_channel:
    if (ctx->channel)
    {
        ibv_destroy_comp_channel(ctx->channel);
    }

clean_device:
    ibv_close_device(ctx->ctx);

clean_dev_list:
    ibv_free_device_list(dev_list);

err:
    return false;
}

void destroy_ctx(struct send_lat_context *ctx)
{
    if (!ctx)
    {
        return;
    }

    // Destroy queue pair
    if (ctx->qp)
    {
        ibv_destroy_qp(ctx->qp);
    }

    // Destroy completion queue
    if (ctx->cq)
    {
        ibv_destroy_cq(ctx->cq);
    }

    // Un-register memory region
    if (ctx->mr)
    {
        ibv_dereg_mr(ctx->mr);
    }

    // Free memory
    if (ctx->buf)
    {
        free(ctx->buf);
    }

    // Destroy protection domain
    if (ctx->pd)
    {
        ibv_dealloc_pd(ctx->pd);
    }

    // Desotry completion channel
    if (ctx->channel)
    {
        ibv_destroy_comp_channel(ctx->channel);
    }

    // Close RDMA device context
    if (ctx->ctx)
    {
        ibv_close_device(ctx->ctx);
    }
}

// Get the index of GID whose type is RoCE V2
// Refer to https://docs.mellanox.com/pages/viewpage.action?pageId=12013422#RDMAoverConvergedEthernet(RoCE)-RoCEv2 for more details
int get_rocev2_gid_index(struct send_lat_context *ctx)
{
    int gid_index = 2;

    while (true)
    {
        FILE *fp;
        char *line = NULL;
        size_t len = 0;
        ssize_t read;
        char file_name[128];

        snprintf(file_name, sizeof(file_name),
                 "/sys/class/infiniband/%s/ports/%d/gid_attrs/types/%d",
                 ctx->ib_dev_name, ctx->dev_port, gid_index);

        fp = fopen(file_name, "r");
        if (!fp)
        {
            break;
        }

        read = getline(&line, &len, fp);
        if (read <= 0)
        {
            fclose(fp);
            break;
        }

        if (strncmp(line, "RoCE v2", strlen("RoCE v2")) == 0)
        {
            fclose(fp);
            free(line);
            return gid_index;
        }

        fclose(fp);
        free(line);
        gid_index++;
    }

    return DEFAULT_GID_INDEX;
}

// Read exactly 'count' bytes from the file descriptor 'fd'
// and store the bytes into buffer 'buf'.
// Return the number of bytes successfully read.
static size_t write_exact(int fd, char *buf, size_t count)
{
    // current buffer loccation
    char *cur_buf = NULL;
    // # of bytes that have been written
    size_t bytes_wrt = 0;
    int n;

    if (!buf)
    {
        return 0;
    }

    cur_buf = buf;

    while (count > 0)
    {
        n = write(fd, cur_buf, count);

        if (n <= 0)
        {
            fprintf(stderr, "write error\n");
            break;
        }
        else
        {
            bytes_wrt += n;
            count -= n;
            cur_buf += n;
        }
    }

    return bytes_wrt;
}

// Write exactly 'count' bytes storing in buffer 'buf' into
// the file descriptor 'fd'.
// Return the number of bytes sucsessfully written.
static size_t read_exact(int fd, char *buf, size_t count)
{
    // current buffer loccation
    char *cur_buf = NULL;
    // # of bytes that have been read
    size_t bytes_read = 0;
    int n;

    if (!buf)
    {
        return 0;
    }

    cur_buf = buf;

    while (count > 0)
    {
        n = read(fd, cur_buf, count);

        if (n <= 0)
        {
            fprintf(stderr, "read error\n");
            break;
        }
        else
        {
            bytes_read += n;
            count -= n;
            cur_buf += n;
        }
    }

    return bytes_read;
}

static void wire_gid_to_gid(const char *wgid, union ibv_gid *gid)
{
    char tmp[9];
    uint32_t v32;
    int i;

    for (tmp[8] = 0, i = 0; i < 4; ++i)
    {
        memcpy(tmp, wgid + i * 8, 8);
        sscanf(tmp, "%x", &v32);
        *(uint32_t *)(&gid->raw[i * 4]) = ntohl(v32);
    }
}

static void gid_to_wire_gid(const union ibv_gid *gid, char wgid[])
{
    int i;

    for (i = 0; i < 4; ++i)
        sprintf(&wgid[i * 8], "%08x", htonl(*(uint32_t *)(gid->raw + i * 4)));
}

// Write my destination information to a remote server
bool write_dest_info(int fd, struct send_lat_dest *my_dest)
{
    if (!fd)
    {
        return false;
    }

    // Message format: "LID : QPN : PSN : GID"
    char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
    char gid[33];

    gid_to_wire_gid(&my_dest->gid, gid);
    sprintf(msg, "%04x:%06x:%06x:%s", my_dest->lid, my_dest->qpn, my_dest->psn, gid);

    if (write_exact(fd, msg, sizeof(msg)) != sizeof(msg))
    {
        fprintf(stderr, "Could not send the local address\n");
        return false;
    }

    return true;
}

// Read destination informaiton
bool read_dest_info(int fd, struct send_lat_dest *rem_dest)
{
    if (!fd)
    {
        return false;
    }

    // Message format: "LID : QPN : PSN : GID"
    char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
    char gid[33];

    if (read_exact(fd, msg, sizeof(msg)) != sizeof(msg))
    {
        fprintf(stderr, "Could not receive the remote address\n");
        return false;
    }

    sscanf(msg, "%x:%x:%x:%s", &rem_dest->lid, &rem_dest->qpn, &rem_dest->psn, gid);
    wire_gid_to_gid(gid, &rem_dest->gid);

    return true;
}

// The client exchanges destination information with a server
bool client_exch_dest(char *server_ip,
                      int server_port,
                      struct send_lat_dest *my_dest,
                      struct send_lat_dest *rem_dest)
{
    if (!my_dest || !rem_dest)
    {
        return false;
    }

    // Create socket file descriptor
    int sockfd = 0;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "Socket creation error\n");
        goto err;
    }

    // Initialize server socket address
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0)
    {
        fprintf(stderr, "Invalid address\n");
        goto err;
    }

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        fprintf(stderr, "Connection error\n");
        goto err;
    }

    // Send my destination information to the remote server
    if (!write_dest_info(sockfd, my_dest))
    {
        goto err;
    }
    else
    {
        printf("Send the local address information to the remote server\n");
    }

    // Receive the remote destination information from the remote server
    if (!read_dest_info(sockfd, rem_dest))
    {
        goto err;
    }
    else
    {
        printf("Receive the remote address information from the remote server\n");
    }

    close(sockfd);
    return true;

err:
    close(sockfd);
    return false;
}

// The server exchanges destination information with a client
bool server_exch_dest(int server_port,
                      struct send_lat_context *ctx,
                      struct send_lat_dest *my_dest,
                      struct send_lat_dest *rem_dest)
{
    if (!ctx || !my_dest || !rem_dest)
    {
        return false;
    }

    // Create socket file descriptor
    int sockfd = 0;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "Socket creation error\n");
        goto err;
    }

    // To allow reuse of local addresses
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        fprintf(stderr, "Set socket option error\n");
        goto err;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(server_port);
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        fprintf(stderr, "Bind error\n");
        goto err;
    }

    if (listen(sockfd, 5) < 0)
    {
        fprintf(stderr, "Listen error\n");
        goto err;
    }

    // Accept an incoming connection
    int addrlen = sizeof(serv_addr);
    int client_sockfd = accept(sockfd, (struct sockaddr *)&serv_addr, (socklen_t *)&addrlen);
    if (client_sockfd < 0)
    {
        fprintf(stderr, "Accept error\n");
        goto err;
    }

    if (!read_dest_info(client_sockfd, rem_dest))
    {
        fprintf(stderr, "Fail to receive the destination information from the client\n");
        close(client_sockfd);
        goto err;
    }
    else
    {
        printf("Receive the destination information from the client\n");
    }

    // Before the server sends back its destination information,
    // it should connect to the remote QP first.
    if (!connect_qp(ctx, my_dest, rem_dest))
    {
        fprintf(stderr, "Fail to connect to the client\n");
        close(client_sockfd);
        goto err;
    }
    else
    {
        printf("Connect to the client\n");
    }

    if (!write_dest_info(client_sockfd, my_dest))
    {
        fprintf(stderr, "Fail to send my destination information to the client\n");
        close(client_sockfd);
        goto err;
    }
    else
    {
        printf("Send my destination information to the client\n");
    }

    close(client_sockfd);
    close(sockfd);
    return true;

err:
    close(sockfd);
    return false;
}

// Initialize destination information
bool init_dest(struct send_lat_dest *dest,
               struct send_lat_context *ctx)
{

    if (!dest || !ctx)
    {
        return false;
    }

    srand48(getpid() * time(NULL));
    // local identifier
    dest->lid = ctx->port_attr.lid;
    // QP number
    dest->qpn = ctx->qp->qp_num;
    // packet sequence number
    dest->psn = lrand48() & 0xffffff;

    // Get the index of GID whose type is RoCE v2
    ctx->gid_index = get_rocev2_gid_index(ctx);
    printf("GID index = %d\n", ctx->gid_index);

    if (ibv_query_gid(ctx->ctx, ctx->dev_port, ctx->gid_index, &(dest->gid)) != 0)
    {
        fprintf(stderr, "Cannot read my device's GID (GID index = %d)\n", ctx->gid_index);
        return false;
    }

    return true;
}

void print_dest(struct send_lat_dest *dest)
{
    if (!dest)
    {
        return;
    }

    char gid[33] = {0};
    inet_ntop(AF_INET6, &(dest->gid), gid, sizeof(gid));
    printf("LID 0x%04x, QPN 0x%06x, PSN 0x%06x, GID %s\n",
           dest->lid, dest->qpn, dest->psn, gid);
}

// Post 'n' receive requests.
// Return # of receive requests that have been posted.
int post_recv(struct send_lat_context *ctx, int n)
{
    struct ibv_sge list = {
        .addr = (uintptr_t)(ctx->buf),
        .length = ctx->buf_size,
        .lkey = ctx->mr->lkey};

    struct ibv_recv_wr wr = {
        .wr_id = RECV_WRID,
        .sg_list = &list,
        .num_sge = 1,
    };

    struct ibv_recv_wr *bad_wr;
    int i;

    for (i = 0; i < n; ++i)
    {
        if (ibv_post_recv(ctx->qp, &wr, &bad_wr))
        {
            break;
        }
    }

    return i;
}

// Post a send request.
bool post_send(struct send_lat_context *ctx)
{
    struct ibv_sge list = {
        .addr = (uintptr_t)(ctx->buf),
        .length = ctx->buf_size,
        .lkey = ctx->mr->lkey};

    struct ibv_send_wr wr = {
        .wr_id = SEND_WRID,
        .sg_list = &list,
        .num_sge = 1,
        .opcode = IBV_WR_SEND,
        .send_flags = ctx->send_flags};

    struct ibv_send_wr *bad_wr;
    return (ibv_post_send(ctx->qp, &wr, &bad_wr) == 0);
}

bool connect_qp(struct send_lat_context *ctx,
                struct send_lat_dest *my_dest,
                struct send_lat_dest *rem_dest)
{
    struct ibv_qp_attr attr = {
        .qp_state = IBV_QPS_RTR,
        .path_mtu = IBV_MTU_1024,
        // Remote QP number
        .dest_qp_num = rem_dest->qpn,
        // Packet Sequence Number of the received packets
        .rq_psn = rem_dest->psn,
        .max_dest_rd_atomic = 1,
        .min_rnr_timer = 12,
        // Address vector
        .ah_attr = {
            .is_global = 0,
            .dlid = rem_dest->lid,
            .sl = 0,
            .src_path_bits = 0,
            .port_num = ctx->dev_port}};

    if (rem_dest->gid.global.interface_id)
    {
        attr.ah_attr.is_global = 1;
        // Set attributes of the Global Routing Headers (GRH)
        // When using RoCE, GRH must be configured!
        attr.ah_attr.grh.hop_limit = 1;
        attr.ah_attr.grh.dgid = rem_dest->gid;
        attr.ah_attr.grh.sgid_index = ctx->gid_index;
    }

    if (ibv_modify_qp(ctx->qp, &attr,
                      IBV_QP_STATE |
                          IBV_QP_AV |
                          IBV_QP_PATH_MTU |
                          IBV_QP_DEST_QPN |
                          IBV_QP_RQ_PSN |
                          IBV_QP_MAX_DEST_RD_ATOMIC |
                          IBV_QP_MIN_RNR_TIMER))
    {
        fprintf(stderr, "Fail to modify QP to RTR\n");
        return false;
    }

    attr.qp_state = IBV_QPS_RTS;
    // The minimum time that a QP waits for ACK/NACK from remote QP
    attr.timeout = 14;
    attr.retry_cnt = 7;
    attr.rnr_retry = 7;
    attr.sq_psn = my_dest->psn;
    attr.max_rd_atomic = 1;

    if (ibv_modify_qp(ctx->qp, &attr,
                      IBV_QP_STATE |
                          IBV_QP_TIMEOUT |
                          IBV_QP_RETRY_CNT |
                          IBV_QP_RNR_RETRY |
                          IBV_QP_SQ_PSN |
                          IBV_QP_MAX_QP_RD_ATOMIC))
    {
        fprintf(stderr, "Failed to modify QP to RTS\n");
        return false;
    }

    return true;
}