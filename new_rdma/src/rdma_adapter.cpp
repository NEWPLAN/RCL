#include "rdma_adapter.h"
#include <glog/logging.h>
#include <malloc.h>

RDMAAdapter::RDMAAdapter()
{
    LOG(INFO) << "Creating RDMA adapter";
}

RDMAAdapter::~RDMAAdapter()
{
    LOG(INFO) << "Destroying RDMA adapter";
}

RDMAAdapter *RDMAAdapter::init_adapter()
{
    struct ibv_device **dev_list = NULL;
    int num_devices;

    // Get IB device list
    dev_list = ibv_get_device_list(&num_devices);
    if (!dev_list)
    {
        LOG(FATAL) << "Fail to get IB device list";
        exit(-2);
        //goto err;
    }
    else if (num_devices == 0)
    {
        LOG(FATAL) << "No IB devices found";
        exit(-1);
        //goto clean_dev_list;
    }

    int ib_dev_id = 0;
    // ib_dev_id = ib_dev_id_by_name(ctx->ib_dev_name, dev_list, num_devices);
    // if (ib_dev_id < 0)
    // {
    //     fprintf(stderr, "Fail to find IB device %s\n", ctx->ib_dev_name);
    //     goto clean_dev_list;
    // }

    this->dev_name = std::string(ibv_get_device_name(dev_list[ib_dev_id]));

    ctx.ctx = ibv_open_device(dev_list[ib_dev_id]);

    if (ctx.ctx == NULL)
    {
        LOG(FATAL) << "Error of opening dev: " << dev_name;
        //goto clean_dev_list
    }

    {
        //creating completion events
    }

    ctx.pd = ibv_alloc_pd(ctx.ctx);
    if (!(ctx.pd))
    {
        LOG(FATAL) << "Failed to allocate protection domain";
        //clean_comp_channel
    }

    LOG(INFO) << "Open IB device: " << this->dev_name;

    register_mem(0, 0);

    // Query device attributes
    if (ibv_query_device(ctx.ctx, &(ctx.dev_attr)) != 0)
    {
        LOG(FATAL) << "Fail to query device attributes";
        //goto clean_data_mr;
    }

    ctx.dev_port = 1;

    if (ibv_query_port(ctx.ctx, ctx.dev_port, &(ctx.port_attr)) != 0)
    {
        LOG(FATAL) << "Failed to query dev port";
    }

    //create cq
    // Create a completion queue
    ctx.cq = ibv_create_cq(ctx.ctx,
                           4096, //ctx->dev_attr.max_cqe,
                           NULL, NULL, 0);
    if (!(ctx.cq))
    {
        LOG(FATAL) << "Fail to create the completion queue";
        //goto clean_data_mr;
    }

    create_QP();
    modify_qp_to_init();

    ibv_free_device_list(dev_list);
    // ibv_close_device(ctx.ctx);

    return this;

    //
}

void RDMAAdapter::create_QP()
{
    // Create a queue pair (QP)
    struct ibv_qp_attr attr;
    struct ibv_qp_init_attr init_attr;
    memset(&init_attr, 0, sizeof(init_attr));
    {
        init_attr.send_cq = ctx.cq;
        init_attr.recv_cq = ctx.cq;
        init_attr.cap.max_send_wr = 1024;
        init_attr.cap.max_recv_wr = 1024;
        init_attr.cap.max_send_sge = 1;
        init_attr.cap.max_recv_sge = 1;
        init_attr.qp_type = IBV_QPT_RC;
    }

    ctx.qp = ibv_create_qp(ctx.pd, &init_attr);
    if (!(ctx.qp))
    {
        LOG(FATAL) << "Fail to create QP";
    }

    ctx.inline_msg = 0;

    ctx.send_flags = IBV_SEND_SIGNALED;
    if (ctx.inline_msg)
    {
        ibv_query_qp(ctx.qp, &attr, IBV_QP_CAP, &init_attr);

        if (init_attr.cap.max_inline_data >= ctx.ctrl_buf_size &&
            init_attr.cap.max_inline_data >= ctx.data_buf_size)
        {
            ctx.send_flags |= IBV_SEND_INLINE;
        }
        else
        {
            fprintf(stderr, "Fail to set IBV_SEND_INLINE because max inline data size is %d\n",
                    init_attr.cap.max_inline_data);
        }
    }
}

void RDMAAdapter::register_mem(char *buf, int size)
{
    ctx.data_buf_size = 1024 * 1024 * 100;
    ctx.ctrl_buf_size = 1024 * 1024;
    // Allocate memory for control plane messages
    ctx.ctrl_buf = (char *)memalign(sysconf(_SC_PAGESIZE), ctx.ctrl_buf_size);
    if (!(ctx.ctrl_buf))
    {
        fprintf(stderr, "Fail to allocate memory for control plane messagees\n");
        //goto clean_pd;
    }

    // Allocate memory for data plane messages
    ctx.data_buf = (char *)memalign(sysconf(_SC_PAGESIZE), ctx.data_buf_size);
    if (!(ctx.data_buf))
    {
        fprintf(stderr, "Fail to allocate memory for data plane messagees\n");
        // goto clean_ctrl_buf;
    }

    // Register memory region for control plane messages
    int access_flags = IBV_ACCESS_LOCAL_WRITE;
    ctx.ctrl_mr = ibv_reg_mr(ctx.pd, ctx.ctrl_buf, ctx.ctrl_buf_size, access_flags);
    if (!(ctx.ctrl_mr))
    {
        fprintf(stderr, "Fail to register memory region for control plane messages\n");
        //goto clean_data_buf;
    }

    // Register memory region for data plane messages
    access_flags = IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE;
    ctx.data_mr = ibv_reg_mr(ctx.pd, ctx.data_buf, ctx.data_buf_size, access_flags);
    if (!(ctx.data_mr))
    {
        fprintf(stderr, "Fail to register memory region for data plane messages\n");
        //goto clean_ctrl_mr;
    }
}

void RDMAAdapter::deregister_mem(char *buf)
{
}

int RDMAAdapter::modify_qp_to_init()
{
    struct ibv_qp_attr attr;
    attr.qp_state = IBV_QPS_INIT;
    attr.pkey_index = 0;
    attr.port_num = ctx.dev_port;
    // Allow incoming RDMA writes on this QP
    attr.qp_access_flags = IBV_ACCESS_REMOTE_WRITE;

    if (ibv_modify_qp(ctx.qp, &attr,
                      IBV_QP_STATE |
                          IBV_QP_PKEY_INDEX |
                          IBV_QP_PORT |
                          IBV_QP_ACCESS_FLAGS))
    {
        LOG(FATAL) << "Fail to modify QP to INIT";
        return -1;
    }
    LOG(INFO) << "Modify QP to INIT";
    return 0;
}
int RDMAAdapter::modify_qp_to_rtr()
{
    LOG(INFO) << "Modify QP to RTR";

    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_RTR;
    attr.path_mtu = IBV_MTU_1024;
    // Remote QP number
    attr.dest_qp_num = peer_addr->qpn;
    // Packet Sequence Number of the received packets
    attr.rq_psn = peer_addr->psn;
    attr.max_dest_rd_atomic = 1;
    attr.min_rnr_timer = 12;

    // Address vector
    attr.ah_attr.is_global = 0;
    attr.ah_attr.dlid = peer_addr->lid;
    attr.ah_attr.sl = 0;
    attr.ah_attr.src_path_bits = 0;
    attr.ah_attr.port_num = ctx.dev_port;

    if (peer_addr->gid.global.interface_id)
    {
        attr.ah_attr.is_global = 1;
        // Set attributes of the Global Routing Headers (GRH)
        // When using RoCE, GRH must be configured!
        attr.ah_attr.grh.hop_limit = 1;
        attr.ah_attr.grh.dgid = peer_addr->gid;
        attr.ah_attr.grh.sgid_index = ctx.gid_index;
    }

    if (ibv_modify_qp(ctx.qp, &attr,
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
    return 0;
}
int RDMAAdapter::modify_qp_to_rts()
{
    LOG(INFO) << "Modify QP to RTS";
    attr.qp_state = IBV_QPS_RTS;
    // The minimum time that a QP waits for ACK/NACK from remote QP
    attr.timeout = 14;
    attr.retry_cnt = 7;
    attr.rnr_retry = 7;
    attr.sq_psn = peer_addr->psn;
    attr.max_rd_atomic = 1;

    if (ibv_modify_qp(ctx.qp, &attr,
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

    return 0;
}

RDMAAddress *RDMAAdapter::get_adapter_info()
{

    srand48(getpid() * time(NULL));

    if (local_addr == nullptr)
    {
        LOG(INFO) << "get local addr";
        local_addr = new RDMAAddress();
        memset(local_addr, 0, sizeof(RDMAAddress));
        local_addr->lid = ctx.port_attr.lid;
        local_addr->qpn = ctx.qp->qp_num;
        local_addr->psn = lrand48() & 0xffffff;

        ctx.gid_index = get_rocev2_gid_index();
        LOG(INFO) << "gid_index: " << ctx.gid_index;

        if (ibv_query_gid(ctx.ctx, ctx.dev_port, ctx.gid_index, &(local_addr->gid)) != 0)
        {
            LOG(FATAL) << "Cannot read my device's GID (GID index: " << ctx.gid_index;
        }
    }

    return local_addr;
}

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
                 this->dev_name.c_str(), ctx.dev_port, gid_index);

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

void RDMAAdapter::qp_do_connect(RDMAAddress *peer_addr,
                                PreConnector *pre_con)
{
    char local_buf[1024] = "hello";
    char remote_buf[1024] = {0};
    this->peer_addr = peer_addr;

    this->modify_qp_to_rtr();
    pre_con->sock_sync_data(local_buf, remote_buf, 12);

    this->modify_qp_to_rts();
    pre_con->sock_sync_data(local_buf, remote_buf, 12);
}

bool RDMAAdapter::post_data_recv(uint64_t token)
{
    struct ibv_sge list;
    memset(&list, 0, sizeof(list));
    list.addr = (uintptr_t)(ctx.ctrl_buf);
    list.length = ctx.ctrl_buf_size;
    list.lkey = ctx.ctrl_mr->lkey;
    struct ibv_recv_wr wr =
        {
            .wr_id = token,
            .sg_list = &list,
            .num_sge = 1,
        };
    struct ibv_recv_wr *bad_wr = NULL;

    return ibv_post_recv(ctx.qp, &wr, &bad_wr) == 0;
}

bool RDMAAdapter::post_ctrl_recv(void)
{
    struct ibv_sge list;
    memset(&list, 0, sizeof(list));
    list.addr = (uintptr_t)(ctx.ctrl_buf);
    list.length = ctx.ctrl_buf_size;
    list.lkey = ctx.ctrl_mr->lkey;
    struct ibv_recv_wr wr =
        {
            .wr_id = 1111,
            .sg_list = &list,
            .num_sge = 1,
        };
    struct ibv_recv_wr *bad_wr = NULL;

    return ibv_post_recv(ctx.qp, &wr, &bad_wr) == 0;
}

bool RDMAAdapter::post_send(int index)
{
    bool process_state = false;
    return process_state;
}

bool RDMAAdapter::post_ctrl_send()
{
    struct ibv_sge list = {
        .addr = (uintptr_t)(ctx.ctrl_buf),
        .length = ctx.ctrl_buf_size,
        .lkey = ctx.ctrl_mr->lkey};

    struct ibv_send_wr wr = {
        .wr_id = 2222,
        .sg_list = &list,
        .num_sge = 1,
        .opcode = IBV_WR_SEND,
        .send_flags = ctx.send_flags};

    struct ibv_send_wr *bad_wr;
    return ibv_post_send(ctx.qp, &wr, &bad_wr) == 0;
}

bool RDMAAdapter::post_write(int index)
{
    struct ibv_sge list = {
        .addr = (uintptr_t)(ctx.data_buf),
        .length = ctx.data_buf_size,
        .lkey = ctx.data_mr->lkey};

    struct ibv_send_wr wr =
        {
            .wr_id = 10243,
            .sg_list = &list,
            .num_sge = 1,
            .opcode = IBV_WR_RDMA_WRITE_WITH_IMM,
            .send_flags = ctx.send_flags,
            //.wr.rdma.remote_addr = rem_mem->addr,
            //.wr.rdma.rkey = rem_mem->key};
        };

    struct ibv_send_wr *bad_wr;
    return ibv_post_send(ctx.qp, &wr, &bad_wr) == 0;
}

bool RDMAAdapter::process_CQ(int max_cqe)
{
    struct ibv_wc wc_, *wc;
    wc = &wc_;
    bool process_state = false;
    do
    {
        int ne = ibv_poll_cq(ctx.cq, 1, wc);
        if (ne < 0)
        {
            LOG(FATAL) << "Error of poll CQ " << ne;
        }
        else if (ne > 0)
        {
            if (wc->status != IBV_WC_SUCCESS)
            {
                fprintf(stderr, "Error of strerror= %s\n", strerror(errno));
                LOG(FATAL) << "wc->status!= IBV_WC_SUCCESS ";
            }
            if (wc->opcode == IBV_WC_SEND)
            {
                LOG(INFO) << "get send wc";
            }
            if (wc->opcode == IBV_WC_RECV)
            {
                LOG(INFO) << "get recv wc";
            }
            return true;
        }
        else
            continue;
    } while (true);
    return process_state;
}

bool RDMAAdapter::exchange_mem_info()
{
    if (ctx.ctrl_buf == 0 || ctx.data_buf == 0)
        LOG(FATAL) << "Bad memory allocation";

    struct MemInfo *mem_region = (struct MemInfo *)(ctx.ctrl_buf);
    mem_region->addr = (uint64_t)(ctx.data_mr->addr);
    mem_region->key = ctx.data_mr->rkey;

    if (this->post_ctrl_send() != true)
        LOG(FATAL) << "error of exchange mem_info";
    this->process_CQ(1);
    return true;
}