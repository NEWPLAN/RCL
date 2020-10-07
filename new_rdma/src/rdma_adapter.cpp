
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <glog/logging.h>
#include "rdma_adapter.h"

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

RDMAAdapter::RDMAAdapter()
{
    if (ctx == nullptr)
    {
        ctx = new write_lat_context();
        memset(ctx, 0, sizeof(write_lat_context));
        {
            ctx->ib_dev_name = default_device;
            ctx->dev_port = DEFAULT_IB_PORT;
            ctx->gid_index = DEFAULT_GID_INDEX;
            ctx->ctx = NULL;
            ctx->channel = NULL;
            ctx->pd = NULL;
            ctx->data_mr = NULL;
            ctx->ctrl_mr = NULL;
            ctx->cq = NULL;
            ctx->qp = NULL;
            ctx->data_buf = NULL;
            ctx->data_buf_size = DEFAULT_MSG_SIZE;
            ctx->ctrl_buf = NULL;
            ctx->ctrl_buf_size = DEFAULT_CTRL_BUF_SIZE;
            ctx->inline_msg = false;
            ctx->use_event = false;
        }
    }
    LOG(INFO) << "building RDMAAdapter ...";
}

RDMAAdapter::~RDMAAdapter()
{
    delete ctx;
    LOG(INFO) << "Destroy RDMAAdapter...";
}

bool RDMAAdapter::init_ctx()
{
    if (!ctx)
    {
        LOG(INFO) << "Error of init context";
        exit(-1);
    }

    struct ibv_device **dev_list = NULL;
    int num_devices;

    // Get IB device list
    dev_list = ibv_get_device_list(&num_devices);
    if (!dev_list)
    {
        fprintf(stderr, "Fail to get IB device list\n");
        exit(-1);
    }
    else if (num_devices == 0)
    {
        fprintf(stderr, "No IB devices found\n");
        exit(-1);
    }

    int ib_dev_id = -1;
    ib_dev_id = ib_dev_id_by_name(ctx->ib_dev_name, dev_list, num_devices);
    if (ib_dev_id < 0)
    {
        fprintf(stderr, "Fail to find IB device %s\n", ctx->ib_dev_name);
        exit(-1);
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
        exit(-1);
    }

    LOG(INFO) << "Open IB device at: " << ibv_get_device_name(dev_list[ib_dev_id]);

    // Create a completion channel
    if (ctx->use_event)
    {
        ctx->channel = ibv_create_comp_channel(ctx->ctx);
        if (!(ctx->channel))
        {
            fprintf(stderr, "Cannot create completion channel\n");
            exit(-1);
        }
    }
    else
    {
        LOG(INFO) << "Do not use the event channel";
        ctx->channel = NULL;
    }

    // Allocate protection domain
    ctx->pd = ibv_alloc_pd(ctx->ctx);
    if (!(ctx->pd))
    {
        fprintf(stderr, "Fail to allocate protection domain\n");
        exit(-1);
    }

    LOG(INFO) << "Allocating protection domain";

    // Allocate memory for control plane messages
    ctx->ctrl_buf = (char *)memalign(sysconf(_SC_PAGESIZE), ctx->ctrl_buf_size);
    if (!(ctx->ctrl_buf))
    {
        fprintf(stderr, "Fail to allocate memory for control plane messagees\n");
        exit(-1);
    }

    // Allocate memory for data plane messages
    ctx->data_buf = (char *)memalign(sysconf(_SC_PAGESIZE), ctx->data_buf_size);
    if (!(ctx->data_buf))
    {
        fprintf(stderr, "Fail to allocate memory for data plane messagees\n");
        exit(-1);
    }

    LOG(INFO) << "Registering control plane message";

    // Register memory region for control plane messages
    int access_flags = IBV_ACCESS_LOCAL_WRITE;
    ctx->ctrl_mr = ibv_reg_mr(ctx->pd, ctx->ctrl_buf, ctx->ctrl_buf_size, access_flags);
    if (!(ctx->ctrl_mr))
    {
        fprintf(stderr, "Fail to register memory region for control plane messages\n");
        exit(-1);
    }

    // Register memory region for data plane messages
    access_flags = IBV_ACCESS_REMOTE_WRITE |
                   IBV_ACCESS_LOCAL_WRITE;
    ctx->data_mr = ibv_reg_mr(ctx->pd, ctx->data_buf,
                              ctx->data_buf_size, access_flags);
    if (!(ctx->data_mr))
    {
        fprintf(stderr, "Fail to register memory region for data plane messages\n");
        exit(-1);
    }

    LOG(INFO) << "Register data plane message";

    // Query device attributes
    if (ibv_query_device(ctx->ctx, &(ctx->dev_attr)) != 0)
    {
        fprintf(stderr, "Fail to query device attributes\n");
        exit(-1);
    }

    // Query port attributes
    if (ibv_query_port(ctx->ctx, ctx->dev_port, &(ctx->port_attr)) != 0)
    {
        fprintf(stderr, "Fail to query port attributes\n");
        exit(-1);
    }

    LOG(INFO) << "Querying IBV ports";

    // Create a completion queue
    ctx->cq = ibv_create_cq(ctx->ctx,
                            4096, //ctx->dev_attr.max_cqe,
                            NULL, ctx->channel, 0);
    if (!(ctx->cq))
    {
        fprintf(stderr, "Fail to create the completion queue\n");
        exit(-1);
    }

    LOG(INFO) << "Creating completion queue";

    if (ctx->use_event)
    {
        if (ibv_req_notify_cq(ctx->cq, 0))
        {
            fprintf(stderr, "Cannot request CQ notification\n");
            exit(-1);
        }
    }
    // Create a queue pair (QP)
    struct ibv_qp_attr attr;
    struct ibv_qp_init_attr init_attr = {
        .send_cq = ctx->cq,
        .recv_cq = ctx->cq,
        .cap = {
            .max_send_wr = 1024, //ctx->dev_attr.max_qp_wr,
            .max_recv_wr = 1024, //ctx->dev_attr.max_qp_wr,
            .max_send_sge = 1,
            .max_recv_sge = 1,
        },
        .qp_type = IBV_QPT_RC,
    };

    ctx->qp = ibv_create_qp(ctx->pd, &init_attr);
    if (!(ctx->qp))
    {
        fprintf(stderr, "Fail to create QP\n");
        exit(-1);
    }

    ctx->send_flags = IBV_SEND_SIGNALED;
    if (ctx->inline_msg)
    {
        ibv_query_qp(ctx->qp, &attr, IBV_QP_CAP, &init_attr);

        if (init_attr.cap.max_inline_data >= ctx->ctrl_buf_size &&
            init_attr.cap.max_inline_data >= ctx->data_buf_size)
        {
            ctx->send_flags |= IBV_SEND_INLINE;
        }
        else
        {
            fprintf(stderr, "Fail to set IBV_SEND_INLINE because max inline data size is %d\n",
                    init_attr.cap.max_inline_data);
        }
    }

    modify_qp_to_init(attr);

    ibv_free_device_list(dev_list);
    return true;
}

void RDMAAdapter::destroy_ctx()
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
    if (ctx->data_mr)
    {
        ibv_dereg_mr(ctx->data_mr);
    }

    if (ctx->ctrl_mr)
    {
        ibv_dereg_mr(ctx->ctrl_mr);
    }

    // Free memory
    if (ctx->data_buf)
    {
        free(ctx->data_buf);
    }

    if (ctx->ctrl_buf)
    {
        free(ctx->ctrl_buf);
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
int RDMAAdapter::get_rocev2_gid_index()
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

// Initialize destination information
bool RDMAAdapter::init_dest(struct write_lat_dest *dest)
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
    ctx->gid_index = get_rocev2_gid_index();
    printf("GID index = %d\n", ctx->gid_index);

    if (ibv_query_gid(ctx->ctx, ctx->dev_port, ctx->gid_index, &(dest->gid)) != 0)
    {
        fprintf(stderr, "Cannot read my device's GID (GID index = %d)\n", ctx->gid_index);
        return false;
    }

    return true;
}

void RDMAAdapter::print_dest(struct write_lat_dest *dest)
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

// Initialize data plane memory information
bool RDMAAdapter::init_data_mem(struct write_lat_mem *mem)
{
    if (!mem || !ctx)
    {
        return false;
    }

    mem->addr = (uint64_t)(ctx->data_mr->addr);
    mem->key = ctx->data_mr->rkey;
    return true;
}

void RDMAAdapter::print_mem(struct write_lat_mem *mem)
{
    if (!mem)
    {
        return;
    }

    printf("Addr %" PRIu64 ", Key %" PRIu32 "\n", mem->addr, mem->key);
}

// Post a receive request to receive a control plane message.
// Return true on success.
bool RDMAAdapter::post_ctrl_recv()
{
    struct ibv_sge list = {
        .addr = (uintptr_t)(ctx->ctrl_buf),
        .length = ctx->ctrl_buf_size,
        .lkey = ctx->ctrl_mr->lkey};

    struct ibv_recv_wr wr = {
        .wr_id = RECV_WRID,
        .sg_list = &list,
        .num_sge = 1,
    };

    struct ibv_recv_wr *bad_wr;
    return ibv_post_recv(ctx->qp, &wr, &bad_wr) == 0;
}

// Post a send request to send a control plane message.
// Return true on success.
bool RDMAAdapter::post_ctrl_send()
{
    struct ibv_sge list = {
        .addr = (uintptr_t)(ctx->ctrl_buf),
        .length = ctx->ctrl_buf_size,
        .lkey = ctx->ctrl_mr->lkey};

    struct ibv_send_wr wr = {
        .wr_id = SEND_WRID,
        .sg_list = &list,
        .num_sge = 1,
        .opcode = IBV_WR_SEND,
        .send_flags = ctx->send_flags};

    struct ibv_send_wr *bad_wr;
    return ibv_post_send(ctx->qp, &wr, &bad_wr) == 0;
}

// Post a write request to send a data plane messages
// Return true on success.
bool RDMAAdapter::post_data_write(struct write_lat_mem *rem_mem)
{
    struct ibv_sge list;
    memset(&list, 0, sizeof(list));

    list.addr = (uintptr_t)(ctx->data_buf);
    list.length = ctx->data_buf_size;
    list.lkey = ctx->data_mr->lkey;

    struct ibv_send_wr wr;
    memset(&wr, 0, sizeof(wr));

    wr.wr_id = WRITE_WRID;
    wr.sg_list = &list;
    wr.num_sge = 1;
    wr.opcode = IBV_WR_RDMA_WRITE;
    wr.send_flags = ctx->send_flags;
    wr.wr.rdma.remote_addr = rem_mem->addr;
    wr.wr.rdma.rkey = rem_mem->key;

    struct ibv_send_wr *bad_wr;
    return ibv_post_send(ctx->qp, &wr, &bad_wr) == 0;
}

bool RDMAAdapter::post_data_write_with_imm(struct write_lat_mem *rem_mem,
                                           unsigned int imm_data)
{
    struct ibv_sge list;
    memset(&list, 0, sizeof(list));

    list.addr = (uintptr_t)(ctx->data_buf);
    list.length = ctx->data_buf_size;
    list.lkey = ctx->data_mr->lkey;

    struct ibv_send_wr wr;
    memset(&wr, 0, sizeof(wr));

    wr.wr_id = WRITE_WITH_IMM_WRID;
    wr.sg_list = &list;
    wr.num_sge = 1;
    wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
    wr.send_flags = ctx->send_flags;
    wr.imm_data = htonl(imm_data);
    wr.wr.rdma.remote_addr = rem_mem->addr;
    wr.wr.rdma.rkey = rem_mem->key;

    struct ibv_send_wr *bad_wr;
    return ibv_post_send(ctx->qp, &wr, &bad_wr) == 0;
}

bool RDMAAdapter::connect_qp(struct write_lat_dest *my_dest,
                             struct write_lat_dest *rem_dest)
{
    if (!modify_qp_to_rtr(rem_dest))
    {
        LOG(FATAL) << "Error of modify_qp_to_rtr";
        return false;
    }
    if (!modify_qp_to_rts(my_dest))
    {
        LOG(FATAL) << "Error of modify_qp_to_rts";
        return false;
    }

    return true;
}

bool RDMAAdapter::parse_recv_wc(struct ibv_wc *wc)
{
    if (wc->status != IBV_WC_SUCCESS)
    {
        fprintf(stderr, "Work request status is %s\n", ibv_wc_status_str(wc->status));
        return false;
    }

    if (wc->opcode != IBV_WC_RECV)
    {
        fprintf(stderr, "Work request opcode is not IBV_WC_RECV (%d)\n", wc->opcode);
        return false;
    }

    return true;
}

bool RDMAAdapter::parse_recv_with_imm_wc(struct ibv_wc *wc)
{
    if (wc->status != IBV_WC_SUCCESS)
    {
        fprintf(stderr, "Work request status is %s\n", ibv_wc_status_str(wc->status));
        return false;
    }

    if (wc->opcode != IBV_WC_RECV_RDMA_WITH_IMM)
    {
        fprintf(stderr, "Work request opcode is not IBV_WC_RECV_RDMA_WITH_IMM (%d)\n", wc->opcode);
        return false;
    }

    return true;
}

bool RDMAAdapter::parse_send_wc(struct ibv_wc *wc)
{
    if (wc->status != IBV_WC_SUCCESS)
    {
        fprintf(stderr, "Work request status is %s\n", ibv_wc_status_str(wc->status));
        return false;
    }

    if (wc->opcode != IBV_WC_SEND)
    {
        fprintf(stderr, "Work request opcode is not IBV_WC_SEND (%d)\n", wc->opcode);
        return false;
    }

    return true;
}

bool RDMAAdapter::parse_write_wc(struct ibv_wc *wc)
{
    if (wc->status != IBV_WC_SUCCESS)
    {
        fprintf(stderr, "Work request status is %s\n", ibv_wc_status_str(wc->status));
        return false;
    }

    if (wc->opcode != IBV_WC_RDMA_WRITE)
    {
        fprintf(stderr, "Work request opcode is not IBV_WC_RDMA_WRITE (%d)\n", wc->opcode);
        return false;
    }

    return true;
}

// Wait for a completed work request.
// Return the completed work request in @wc.
// Return true on success.
bool RDMAAdapter::wait_for_wc(struct ibv_wc *wc)
{
    while (true)
    {
        int ne = ibv_poll_cq(ctx->cq, 1, wc);
        if (ne < 0)
        {
            fprintf(stderr, "Fail to poll CQ (%d)\n", ne);
            return false;
        }
        else if (ne > 0)
        {
            return true;
        }
        else
        {
            continue;
        }
    }

    // We should never reach here
    return false;
}

int RDMAAdapter::modify_qp_to_init(struct ibv_qp_attr &attr)
{
    attr.qp_state = IBV_QPS_INIT;
    attr.pkey_index = 0;
    attr.port_num = ctx->dev_port;
    // Allow incoming RDMA writes on this QP
    attr.qp_access_flags = IBV_ACCESS_REMOTE_WRITE;

    if (ibv_modify_qp(ctx->qp, &attr,
                      IBV_QP_STATE |
                          IBV_QP_PKEY_INDEX |
                          IBV_QP_PORT |
                          IBV_QP_ACCESS_FLAGS))
    {

        fprintf(stderr, "Fail to modify QP to INIT\n");
        exit(-1);
    }
    return 0;
}
bool RDMAAdapter::modify_qp_to_rtr(struct write_lat_dest *rem_dest)
{
    struct ibv_qp_attr attr;
    memset(&attr, 0, sizeof(attr));

    attr.qp_state = IBV_QPS_RTR;
    attr.path_mtu = IBV_MTU_1024;
    // Remote QP number
    attr.dest_qp_num = rem_dest->qpn;
    // Packet Sequence Number of the received packets
    attr.rq_psn = rem_dest->psn;
    attr.max_dest_rd_atomic = 1;
    attr.min_rnr_timer = 12;

    // Address vector
    attr.ah_attr.is_global = 0;
    attr.ah_attr.dlid = rem_dest->lid;
    attr.ah_attr.sl = 0;
    attr.ah_attr.src_path_bits = 0;
    attr.ah_attr.port_num = ctx->dev_port;

    if (rem_dest->gid.global.interface_id)
    {
        attr.ah_attr.is_global = 1;
        // Set attributes of the Global Routing Headers (GRH)
        // When using RoCE, GRH must be configured!
        attr.ah_attr.grh.hop_limit = 1;
        attr.ah_attr.grh.dgid = rem_dest->gid;
        attr.ah_attr.grh.sgid_index = ctx->gid_index;
        attr.ah_attr.grh.traffic_class = 96;
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
    LOG(INFO) << "Modify QP state to RTR";

    return true;
}
bool RDMAAdapter::modify_qp_to_rts(struct write_lat_dest *my_dest)
{
    struct ibv_qp_attr attr;
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