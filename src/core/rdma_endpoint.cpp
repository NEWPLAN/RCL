#include "rdma_endpoint.h"
#include "util.h"
#include <unistd.h>
#include <byteswap.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <random>

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint64_t htonll(uint64_t x) { return bswap_64(x); }
static inline uint64_t ntohll(uint64_t x) { return bswap_64(x); }
#elif __BYTE_ORDER == __BIG_ENDIAN
static inline uint64_t htonll(uint64_t x) { return x; }
static inline uint64_t ntohll(uint64_t x) { return x; }
#else
#error __BYTE_ORDER is neither __LITTLE_ENDIAN nor __BIG_ENDIAN
#endif

std::mt19937_64 *InitRng()
{
    std::random_device device("/dev/urandom");
    return new std::mt19937_64(device());
}

long long New64()
{
    static std::mt19937_64 *rng = InitRng();
    // static mutex mu;
    // mutex_lock l(mu);
    return (*rng)();
}

RDMAEndpoint::RDMAEndpoint(PreConnector *pre)
{

    this->connector_resource_.MAX_S_WR = 128;

    this->connector_resource_.MAX_R_WR = 128;

    this->connector_resource_.pre = pre;
    create_resources();
}

void RDMAEndpoint::create_resources()
{
    struct ibv_device **dev_list = NULL;
    struct ibv_qp_init_attr qp_init_attr;
    struct ibv_device *ib_dev = NULL;
    size_t size;
    int i;
    int mr_flags = 0;
    int cq_size = 1024;
    int num_devices;

    char msg_buf[10240] = {0};

    LOG(INFO) << "searching for IB devices in host";

    /* get device names in the system */
    dev_list = ibv_get_device_list(&num_devices);
    connector_resource_.dev_list = dev_list;
    if (!dev_list)
    {
        release_resources("failed to get IB devices list, exit");
    }
    /* if there isn't any IB device in host */
    if (!num_devices)
    {
        release_resources("failed to get IB devices list, exit");
    }

    LOG(INFO) << "Found # of devices: " << num_devices;

    auto &config_pre = this->connector_resource_.pre->config;

    /* search for the specific device we want to work with */
    for (i = 0; i < num_devices; i++)
    {
        if (!this->connector_resource_.pre->config.dev_name)
        {
            config_pre.dev_name = strdup(ibv_get_device_name(dev_list[i]));
            LOG(INFO) << "device not specified, using first one found: " << config_pre.dev_name;
        }
        if (!strcmp(ibv_get_device_name(dev_list[i]), config_pre.dev_name))
        {
            ib_dev = dev_list[i];
            break;
        }
    }
    /* if the device wasn't found in host */
    if (!ib_dev)
    {
        sprintf(msg_buf, "IB device %s wasn't found ", config_pre.dev_name);
        release_resources(msg_buf);
    }
    /* get device handle */
    connector_resource_.ib_ctx = ibv_open_device(ib_dev);
    if (!connector_resource_.ib_ctx)
    {
        release_resources("failed to open device ");
    }
    /* We are now done with device list, free it */
    ibv_free_device_list(dev_list);
    dev_list = NULL;
    ib_dev = NULL;
    /* query port properties */
    if (ibv_query_port(connector_resource_.ib_ctx, config_pre.ib_port, &connector_resource_.port_attr))
    {
        sprintf(msg_buf, "Failed ibv_query_port on port  %d", config_pre.ib_port);
        release_resources(msg_buf);
    }
    /* allocate Protection Domain */
    connector_resource_.pd = ibv_alloc_pd(connector_resource_.ib_ctx);
    if (!connector_resource_.pd)
    {
        release_resources("ibv_alloc_pd failed");
    }
    /* each side will send only one WR, so Completion Queue with 1 entry is enough */
    cq_size = 1024; //TODO define CQ_SIZE here
    if (cq_size < connector_resource_.MAX_S_WR + connector_resource_.MAX_R_WR)
    {
        cq_size = connector_resource_.MAX_S_WR + connector_resource_.MAX_R_WR + 4;
    }
    LOG(INFO) << "Creating CQ";
    connector_resource_.cq = ibv_create_cq(connector_resource_.ib_ctx, cq_size, NULL, NULL, 0);
    if (!connector_resource_.cq)
    {
        release_resources("failed to create CQ entries ");
    }
    /* allocate the memory buffer that will hold the data */
    size = 1024 * 1024 + 16;
    connector_resource_.buf = (char *)malloc(size);
    if (!connector_resource_.buf)
    {
        release_resources("failed to allocate mem buffer ");
    }
    memset(connector_resource_.buf, 0, size);
    /* only in the client side put the message in the memory buffer */
    if (config_pre.server_name)
    {
        strcpy(connector_resource_.buf, "Hello from client sides");
        LOG(INFO) << "Sending msg: " << connector_resource_.buf;
    }
    else
        memset(connector_resource_.buf, 0, size);
    /* register the memory buffer */
    mr_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
    connector_resource_.mr = ibv_reg_mr(connector_resource_.pd, connector_resource_.buf, size, mr_flags);
    if (!connector_resource_.mr)
    {
        sprintf(msg_buf, "ibv_reg_mr failed with mr_flags=0x%x", mr_flags);
        release_resources(msg_buf);
    }
    sprintf(msg_buf, "MR was registered with addr=%p, lkey=0x%x, rkey=0x%x, flags=0x%x",
            connector_resource_.buf, connector_resource_.mr->lkey, connector_resource_.mr->rkey, mr_flags);
    LOG(INFO) << msg_buf;

    /* create the Queue Pair */
    memset(&qp_init_attr, 0, sizeof(qp_init_attr));
    qp_init_attr.qp_type = IBV_QPT_RC;
    qp_init_attr.sq_sig_all = 1;
    qp_init_attr.send_cq = connector_resource_.cq;
    qp_init_attr.recv_cq = connector_resource_.cq;
    qp_init_attr.cap.max_send_wr = connector_resource_.MAX_S_WR;
    qp_init_attr.cap.max_recv_wr = connector_resource_.MAX_R_WR;
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_recv_sge = 1;
    connector_resource_.qp = ibv_create_qp(connector_resource_.pd, &qp_init_attr);
    if (!connector_resource_.qp)
    {
        release_resources("failed to create QP");
    }
    sprintf(msg_buf, "QP was created, QP number=0x%x", connector_resource_.qp->qp_num);
    LOG(INFO) << msg_buf;
    return;
}

void RDMAEndpoint::release_resources(std::string info)
{
    if (connector_resource_.qp)
    {
        ibv_destroy_qp(connector_resource_.qp);
        connector_resource_.qp = NULL;
    }
    if (connector_resource_.qp)
    {
        ibv_destroy_qp(connector_resource_.qp);
        connector_resource_.qp = NULL;
    }
    if (connector_resource_.mr)
    {
        ibv_dereg_mr(connector_resource_.mr);
        connector_resource_.mr = NULL;
    }
    if (connector_resource_.buf)
    {
        free(connector_resource_.buf);
        connector_resource_.buf = NULL;
    }
    if (connector_resource_.cq)
    {
        ibv_destroy_cq(connector_resource_.cq);
        connector_resource_.cq = NULL;
    }
    if (connector_resource_.pd)
    {
        ibv_dealloc_pd(connector_resource_.pd);
        connector_resource_.pd = NULL;
    }
    if (connector_resource_.ib_ctx)
    {
        ibv_close_device(connector_resource_.ib_ctx);
        connector_resource_.ib_ctx = NULL;
    }
    if (connector_resource_.dev_list)
    {
        ibv_free_device_list(connector_resource_.dev_list);
        connector_resource_.dev_list = NULL;
    }
    LOG(FATAL) << info;
}

void RDMAEndpoint::connect()
{
    auto &pre_connect = this->connector_resource_.pre;
    auto &res = this->connector_resource_;

    struct cm_con_data_t local_con_data;
    struct cm_con_data_t remote_con_data;
    struct cm_con_data_t tmp_con_data;
    union ibv_gid my_gid;

    char msg_buf[10240] = {0};

    if (pre_connect->config.gid_index >= 0)
    {
        if (ibv_query_gid(res.ib_ctx, pre_connect->config.ib_port,
                          pre_connect->config.gid_index, &my_gid))
        {
            sprintf(msg_buf, "could not get gid for port %d, index %d\n",
                    pre_connect->config.ib_port,
                    pre_connect->config.gid_index);
            LOG(FATAL) << msg_buf;
            return;
        }
    }
    else
    {
        LOG(INFO) << "No GID used in this setting";
        memset(&my_gid, 0, sizeof(my_gid));
    }

    local_con_data.addr = htonll((uintptr_t)res.buf);
    local_con_data.rkey = htonl(res.mr->rkey);
    local_con_data.qp_num = htonl(res.qp->qp_num);
    local_con_data.lid = htons(res.port_attr.lid);
    memcpy(local_con_data.gid, &my_gid, 16);

    tmp_con_data = pre_connect->exchange_qp_data(local_con_data);

    remote_con_data.addr = ntohll(tmp_con_data.addr);
    remote_con_data.rkey = ntohl(tmp_con_data.rkey);
    remote_con_data.qp_num = ntohl(tmp_con_data.qp_num);
    remote_con_data.lid = ntohs(tmp_con_data.lid);
    memcpy(remote_con_data.gid, tmp_con_data.gid, 16);
    /* save the remote side attributes, we will need it for the post SR */
    res.remote_props = remote_con_data;
    printf("--------------------------data exchange-----------------------------\n");
    fprintf(stdout, "Local Info:\n");
    fprintf(stdout, "\tLocal address = 0x%" PRIx64 "\n", (uintptr_t)res.buf);
    fprintf(stdout, "\tLocal rkey = 0x%x\n", res.mr->rkey);
    fprintf(stdout, "\tLocal QP number = 0x%x\n", res.qp->qp_num);
    fprintf(stdout, "\tLocal LID = 0x%x\n", res.port_attr.lid);
    if (pre_connect->config.gid_index >= 0)
    {
        uint8_t *p = local_con_data.gid;
        fprintf(stdout, "\tLocal GID =%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
                p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
    }

    fprintf(stdout, "Remote Info:\n");
    fprintf(stdout, "\tRemote address = 0x%" PRIx64 "\n", remote_con_data.addr);
    fprintf(stdout, "\tRemote rkey = 0x%x\n", remote_con_data.rkey);
    fprintf(stdout, "\tRemote QP number = 0x%x\n", remote_con_data.qp_num);
    fprintf(stdout, "\tRemote LID = 0x%x\n", remote_con_data.lid);
    if (pre_connect->config.gid_index >= 0)
    {
        uint8_t *p = remote_con_data.gid;
        fprintf(stdout, "\tRemote GID =%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n", p[0],
                p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
    }
    printf("-------------------------------------------------------------------\n");
    /* modify the QP to init */
    modify_qp_to_init();
    LOG(INFO) << "QP state was changed to INIT";

    /* let the client post RR to be prepared for incoming messages */
    if (!pre_connect->config.server_name)
    {
        //post recv here for server
        if (post_receive(1024 * 1024))
        {
            LOG(FATAL) << "Error of post receive";
        }
    }
    /* modify the QP to RTR */
    modify_qp_to_rtr();
    LOG(INFO) << "QP state was changed to RTR";

    modify_qp_to_rts();
    LOG(INFO) << "QP state was changed to RTS";
    /* sync to make sure that both sides are in states that they can connect to prevent packet loose */
    tmp_con_data = pre_connect->exchange_qp_data(local_con_data); //wait for synchronization
    connected_ = true;
}

int RDMAEndpoint::modify_qp_to_init()
{
    auto &config = connector_resource_.pre->config;

    struct ibv_qp_attr attr;
    int flags;
    int rc;
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_INIT;
    attr.port_num = config.ib_port;
    attr.pkey_index = 0;
    attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE |
                           IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
    flags = IBV_QP_STATE |
            IBV_QP_PKEY_INDEX |
            IBV_QP_PORT |
            IBV_QP_ACCESS_FLAGS;

    rc = ibv_modify_qp(connector_resource_.qp, &attr, flags);
    if (rc)
        LOG(FATAL) << "failed to modify QP state to INIT ";
    return rc;
}
int RDMAEndpoint::modify_qp_to_rtr()
{
    auto &config = connector_resource_.pre->config;
    auto &remote_info = connector_resource_.remote_props;
    struct ibv_qp_attr attr;
    int flags;
    int rc;

    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_RTR;
    attr.path_mtu = IBV_MTU_1024;
    attr.dest_qp_num = remote_info.qp_num;
    attr.rq_psn = 0;
    attr.max_dest_rd_atomic = 1;
    attr.min_rnr_timer = 0x12;
    attr.ah_attr.is_global = 0;
    attr.ah_attr.dlid = remote_info.lid;
    attr.ah_attr.sl = 0;
    attr.ah_attr.src_path_bits = 0;
    attr.ah_attr.port_num = config.ib_port;
    if (config.gid_index >= 0)
    {
        attr.ah_attr.is_global = 1;
        attr.ah_attr.port_num = 1;
        memcpy(&attr.ah_attr.grh.dgid, remote_info.gid, 16);
        attr.ah_attr.grh.flow_label = 0;
        attr.ah_attr.grh.hop_limit = 1;
        attr.ah_attr.grh.sgid_index = config.gid_index;
        attr.ah_attr.grh.traffic_class = 0;
    }
    flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
            IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;
    rc = ibv_modify_qp(connector_resource_.qp, &attr, flags);
    if (rc)
    {
        LOG(FATAL) << "failed to modify QP state to RTR";
        return rc;
    }
    return 0;
}
int RDMAEndpoint::modify_qp_to_rts()
{
    struct ibv_qp_attr attr;
    int flags;
    int rc;
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_RTS;
    attr.timeout = 0x14;
    // attr.retry_cnt = 6;
    // attr.rnr_retry = 0;
    attr.retry_cnt = 7;
    attr.rnr_retry = 7;
    attr.sq_psn = 0;
    attr.max_rd_atomic = 1;
    flags = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
            IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;
    rc = ibv_modify_qp(connector_resource_.qp, &attr, flags);
    if (rc)
        fprintf(stderr, "failed to modify QP state to RTS\n");
    return rc;
}
#include <sys/time.h>
#define MAX_POLL_CQ_TIMEOUT 2000
int RDMAEndpoint::process_CQ()
{
    auto &res = connector_resource_;
    struct ibv_wc wc[1024];
    unsigned long start_time_msec;
    unsigned long cur_time_msec;
    struct timeval cur_time;
    int poll_result;
    int rc = 0;
    /* poll the completion for a while before giving up of doing it .. */
    gettimeofday(&cur_time, NULL);
    start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
    do
    {
        poll_result = ibv_poll_cq(res.cq, 1024, static_cast<ibv_wc *>(wc));
        if (poll_result > 0)
            break;
        if (poll_result < 0)
            LOG(FATAL) << "should never poll negatively";
        gettimeofday(&cur_time, NULL);
        cur_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
    } while ((poll_result == 0) && ((cur_time_msec - start_time_msec) < MAX_POLL_CQ_TIMEOUT));

    for (int index = 0; index < poll_result; index++)
    {
        if (wc[index].status != IBV_WC_SUCCESS)
        {
            fprintf(stderr, "got bad completion with status: 0x%x, vendor syndrome: 0x%x\n", wc[index].status,
                    wc[index].vendor_err);
            exit(-1);
            rc = 1;
        }
        switch (wc[index].opcode)
        {
        case IBV_WC_RDMA_WRITE:
            data_in_flight--;
            if (data_in_flight < 0)
                LOG(FATAL) << "data_in_flight should never be negative " << data_in_flight;
            //LOG_EVERY_N(INFO, 100000) << "IBV_WC_RDMA_WRITE " << data_in_flight;
            break;
        case IBV_WC_RECV:
            LOG_EVERY_N(INFO, 1000) << "IBV_WC_RECV " << connector_resource_.buf;
            break;
        default:
            break;
        }
    }
    return rc;
}

int RDMAEndpoint::write_remote(int msg_size)
{
    return this->post_send(IBV_WR_RDMA_WRITE, msg_size);
}

int RDMAEndpoint::send_remote(int msg_size)
{
    int rc = 0;
    return rc;
}

int RDMAEndpoint::post_receive(int msg_size)
{
    auto &res = connector_resource_;
    struct ibv_recv_wr rr;
    struct ibv_sge sge;
    struct ibv_recv_wr *bad_wr;
    int rc;
    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t)res.buf;
    sge.length = 1024 * 1024;
    sge.lkey = res.mr->lkey;
    /* prepare the receive work request */
    memset(&rr, 0, sizeof(rr));
    rr.next = NULL;
    rr.wr_id = 555;
    rr.sg_list = &sge;
    rr.num_sge = 1;
    /* post the Receive Request to the RQ */
    rc = ibv_post_recv(res.qp, &rr, &bad_wr);
    if (rc)
        LOG(WARNING) << "failed to post RR";
    else
        LOG(INFO) << "Receive Request was posted";
    return rc;
}
int RDMAEndpoint::post_send(int opcode, int msg_size)
{
    auto &res = connector_resource_;
    enum ibv_wr_opcode op_code = (enum ibv_wr_opcode)opcode;
    struct ibv_send_wr sr;
    struct ibv_sge sge;
    struct ibv_send_wr *bad_wr = NULL;
    int rc;
    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t)res.buf;
    sge.length = msg_size;
    sge.lkey = res.mr->lkey;
    /* prepare the send work request */
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    sr.wr_id = 0;
    sr.sg_list = &sge;
    sr.num_sge = 1;
    sr.opcode = op_code;
    sr.send_flags = IBV_SEND_SIGNALED;
    if (op_code != IBV_WR_SEND)
    {
        sr.wr.rdma.remote_addr = res.remote_props.addr;
        sr.wr.rdma.rkey = res.remote_props.rkey;
    }
    /* there is a Receive Request in the responder side, so we won't get any into RNR flow */
    rc = ibv_post_send(res.qp, &sr, &bad_wr);
    if (rc)
        LOG(WARNING) << "failed to post SR";
    else
    {
        switch (op_code)
        {
        case IBV_WR_SEND:
            LOG(INFO) << "Send Request was posted";
            break;
        case IBV_WR_RDMA_READ:
            LOG(INFO) << "RDMA Read Request was posted";
            break;
        case IBV_WR_RDMA_WRITE:
            //LOG(INFO) << "RDMA Write Request was posted";
            break;
        case IBV_WR_RDMA_WRITE_WITH_IMM:
            LOG(INFO) << "RDMA Write with IMM Request was posted";
            break;
        default:
            LOG(INFO) << "Unknown Request was posted";
            break;
        }
    }
    data_in_flight += 1;
    return rc;
}
