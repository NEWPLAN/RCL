#include "rdma_adapter.h"
#include "config.h"
#include <chrono>
#include <fcntl.h>
#include <glog/logging.h>
#include <thread>
#include <unistd.h>

int NPRDMAdapter::poll_completion(struct ibv_wc &wc)
{
    unsigned long start_time_msec;
    unsigned long cur_time_msec;
    struct timeval cur_time;
    int poll_result;
    int rc = 0;
    /* poll the completion for a while before giving up of doing it .. */
    gettimeofday(&cur_time, NULL);
    start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
    //int index = 0;
    do
    {
        poll_result = ibv_poll_cq(this->res.cq, 1, &wc);
        gettimeofday(&cur_time, NULL);
        cur_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
        //} while ((poll_result == 0) && (++index > 10000000));
    } while ((poll_result == 0) && ((cur_time_msec - start_time_msec) < MAX_POLL_CQ_TIMEOUT));
    if (poll_result < 0)
    {
        /* poll CQ failed */
        fprintf(stderr, "poll CQ failed\n");
        rc = 1;
    }
    else if (poll_result == 0)
    { /* the CQ is empty */
        fprintf(stderr, "completion wasn't found in the CQ after timeout\n");
        rc = 1;
    }
    else
    {
        /* CQE found */
        LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "completion was found in CQ with status 0x" << wc.status;
        //fprintf(stdout, "completion was found in CQ with status 0x%x\n", wc.status);
        /* check the completion status (here we don't care about the completion opcode */
        if (wc.status != IBV_WC_SUCCESS)
        {
            fprintf(stderr, "got bad completion with status: 0x%x, vendor syndrome: 0x%x\n", wc.status,
                    wc.vendor_err);
            rc = 1;
        }
    }
    return rc;
}

int NPRDMAdapter::peek_status(struct ibv_wc *wc)
{
    int rc = 1;
    int poll_result;
    poll_result = ibv_poll_cq(this->res.cq, 1, wc);
    if (poll_result < 0)
    { /* failed to pull cq */
        LOG(FATAL) << "poll CQ failed";
        rc = 0;
    }
    else if (poll_result == 0)
    { /* the CQ is empty */
        //LOG_EVERY_N(WARNING, SHOWN_LOG_EVERN_N) << "completion wasn't found in the CQ";
        rc = 0;
    }
    else
    {
        /* CQE found */
#if defined DEBUG
        LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "completion was found in CQ with status 0x" << wc->status;
#endif
        /* check the completion status*/
        if (wc->status != IBV_WC_SUCCESS)
        {
            fprintf(stderr, "got bad completion with status: 0x%x, vendor syndrome: 0x%x\n", wc->status,
                    wc->vendor_err);
            rc = 0;
        }
    }
    return rc;
}

int NPRDMAdapter::poll_completion(struct ibv_wc *wc)
{
    unsigned long start_time_msec;
    unsigned long cur_time_msec;
    struct timeval cur_time;
    int poll_result;
    int rc = 0;
    /* poll the completion for a while before giving up of doing it .. */
    gettimeofday(&cur_time, NULL);
    start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
    //int index = 0;
    do
    {
        poll_result = ibv_poll_cq(this->res.cq, 1, wc);
        gettimeofday(&cur_time, NULL);
        cur_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
        //} while ((poll_result == 0) && (++index > 10000000));
    } while ((poll_result == 0) && ((cur_time_msec - start_time_msec) < MAX_POLL_CQ_TIMEOUT));
    if (poll_result < 0)
    {
        /* poll CQ failed */
        fprintf(stderr, "poll CQ failed\n");
        rc = 1;
    }
    else if (poll_result == 0)
    { /* the CQ is empty */
        fprintf(stderr, "completion wasn't found in the CQ after timeout\n");
        rc = 1;
    }
    else
    {
        /* CQE found */
#if defined DEBUG
        LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "completion was found in CQ with status 0x" << wc->status;
#endif
        //fprintf(stdout, "completion was found in CQ with status 0x%x\n", wc.status);
        /* check the completion status (here we don't care about the completion opcode */
        if (wc->status != IBV_WC_SUCCESS)
        {
            fprintf(stderr, "got bad completion with status: 0x%x, vendor syndrome: 0x%x\n", wc->status,
                    wc->vendor_err);
            rc = 1;
        }
    }
    return rc;
}

int NPRDMAdapter::poll_completion()
{
    struct ibv_wc wc;
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
        poll_result = ibv_poll_cq(this->res.cq, 1, &wc);
        gettimeofday(&cur_time, NULL);
        cur_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
    } while ((poll_result == 0) && ((cur_time_msec - start_time_msec) < MAX_POLL_CQ_TIMEOUT));
    if (poll_result < 0)
    {
        /* poll CQ failed */
        fprintf(stderr, "poll CQ failed\n");
        rc = 1;
    }
    else if (poll_result == 0)
    { /* the CQ is empty */
        fprintf(stderr, "completion wasn't found in the CQ after timeout\n");
        rc = 1;
    }
    else
    {
        /* CQE found */
        //fprintf(stdout, "completion was found in CQ with status 0x%x\n", wc.status);
        /* check the completion status (here we don't care about the completion opcode */
        if (wc.status != IBV_WC_SUCCESS)
        {
            fprintf(stderr, "got bad completion with status: 0x%x, vendor syndrome: 0x%x\n", wc.status,
                    wc.vendor_err);
            rc = 1;
        }
    }
    return rc;
}

int NPRDMAdapter::post_send(enum ibv_wr_opcode opcode)
{
    struct ibv_send_wr sr;
    struct ibv_sge sge;
    struct ibv_send_wr *bad_wr = NULL;
    int rc;
    /* prepare the scatter/gather entry */

    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t)this->res.buf;
    sge.length = MSG_SIZE;
    sge.lkey = this->res.mr->lkey;
    /* prepare the send work request */

    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    sr.wr_id = 0;
    sr.sg_list = &sge;
    sr.num_sge = 1;
    sr.opcode = opcode;
    sr.send_flags = IBV_SEND_SIGNALED;
    if (opcode != IBV_WR_SEND)
    {
        sr.wr.rdma.remote_addr = this->res.remote_props.addr;
        sr.wr.rdma.rkey = this->res.remote_props.rkey;
    }

    /* there is a Receive Request in the responder side, so we won't get any into RNR flow */
    rc = ibv_post_send(this->res.qp, &sr, &bad_wr);
    if (rc)
        fprintf(stderr, "failed to post SR\n");
    else
    {
        switch (opcode)
        {
        case IBV_WR_SEND:
            fprintf(stdout, "Send Request was posted\n");
            break;
        case IBV_WR_RDMA_READ:
            fprintf(stdout, "RDMA Read Request was posted\n");
            break;
        case IBV_WR_RDMA_WRITE:
            fprintf(stdout, "RDMA Write Request was posted\n");
            break;
        default:
            fprintf(stdout, "Unknown Request was posted\n");
            break;
        }
    }
    return rc;
}

int NPRDMAdapter::post_ctrl_recv(int index)
{
    struct ibv_recv_wr rr;
    struct ibv_sge sge;
    struct ibv_recv_wr *bad_wr;
    if (index >= config.MSG_BLOCK_NUM)
    {
        LOG(WARNING) << "You should limited the index in [0, " << config.MSG_BLOCK_NUM
                     << "], you are specifying the index: " << index;
        index %= config.MSG_BLOCK_NUM;
    }
    /* prepare the scatter/gather entry */

    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t)(this->res.ctrl_buf + index * config.CTRL_MSG_SIZE);
    sge.length = config.CTRL_MSG_SIZE;
    sge.lkey = this->res.ctrl_mr->lkey;
    /* prepare the receive work request */

    memset(&rr, 0, sizeof(rr));
    rr.next = NULL;
    {
        rr.wr_id = CTRL_RECV;
        rr.wr_id += index;
    }
    rr.sg_list = &sge;
    rr.num_sge = 1;

    /* post the Receive Request to the RQ */
    if (ibv_post_recv(this->res.qp, &rr, &bad_wr))
        LOG(FATAL) << "[error] failed to post RR, with index=" << index << ", " << strerror(errno);
    //else
    //    LOG(INFO) << "Receive Request was posted, with index=" << index;
    return 0;
}

int NPRDMAdapter::post_ctrl_send(int index, bool dumb)
{
    struct ibv_send_wr sr;
    struct ibv_sge sge;
    struct ibv_send_wr *bad_wr = NULL;
    if (index >= config.MSG_BLOCK_NUM)
    {
        LOG(WARNING) << "You should limited the index in [0, " << config.MSG_BLOCK_NUM
                     << "], you are specifying the index: " << index;
        index %= config.MSG_BLOCK_NUM;
    }

    /* prepare the scatter/gather entry */

    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t)(this->res.ctrl_buf + index * config.CTRL_MSG_SIZE);
    sge.length = config.CTRL_MSG_SIZE;
    sge.lkey = this->res.ctrl_mr->lkey;
    /* prepare the send work request */

    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    {
        sr.wr_id = CTRL_SEND;
        sr.wr_id += index;
    }
    sr.sg_list = &sge;
    sr.num_sge = 1;
    sr.opcode = IBV_WR_SEND;
    if (dumb)
    {
        LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[II] do not signal when success";
        sr.send_flags = IBV_SEND_INLINE;
    }
    else
        sr.send_flags = IBV_SEND_SIGNALED;
    /* there is a Receive Request in the responder side, so we won't get any into RNR flow */
    if (ibv_post_send(this->res.qp, &sr, &bad_wr))
        LOG(FATAL) << "[error] failed to post SR with index=" << index << ", with reason: " << strerror(errno);
    //else
    //    LOG(INFO) << "[out] Send Request was posted with index=" << index;
    return 0;
}

int NPRDMAdapter::post_data_read(int index)
{
    struct ibv_send_wr sr;
    struct ibv_sge sge;
    struct ibv_send_wr *bad_wr = NULL;
    if (index >= config.MSG_BLOCK_NUM)
    {
        LOG(WARNING) << "You should limited the index in [0, " << config.MSG_BLOCK_NUM
                     << "], you are specifying the index: " << index;
        index %= config.MSG_BLOCK_NUM;
    }
    /* prepare the scatter/gather entry */

    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t)(this->res.buf + index * config.DATA_MSG_SIZE);
    sge.length = config.DATA_MSG_SIZE;
    sge.lkey = this->res.mr->lkey;
    /* prepare the send work request */

    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    {
        sr.wr_id = DATA_READ;
        sr.wr_id += index;
    }
    sr.sg_list = &sge;
    sr.num_sge = 1;
    sr.opcode = IBV_WR_RDMA_READ;
    sr.send_flags = IBV_SEND_SIGNALED;

    sr.wr.rdma.remote_addr = this->res.remote_props.addr + index * config.DATA_MSG_SIZE;
    sr.wr.rdma.rkey = this->res.remote_props.rkey;

    /* there is a Receive Request in the responder side, so we won't get any into RNR flow */

    if (ibv_post_send(this->res.qp, &sr, &bad_wr))
        LOG(FATAL) << "[error] failed to post SR with index=" << index;
    else
        LOG(INFO) << "[out] RDMA Read Request was posted with index=" << index;
    return 0;
}

int NPRDMAdapter::post_data_write(int index, int msg_size)
{
    struct ibv_send_wr sr;
    struct ibv_sge sge;
    struct ibv_send_wr *bad_wr = NULL;
    if (index >= config.MSG_BLOCK_NUM)
    {
        LOG(WARNING) << "You should limited the index in [0, " << config.MSG_BLOCK_NUM
                     << "], you are specifying the index: " << index;
        index %= config.MSG_BLOCK_NUM;
    }
    /* prepare the scatter/gather entry */

    if (msg_size <= 0)
    {
        LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[out] data num is negative, set to default: " << config.DATA_MSG_SIZE;
        msg_size = config.DATA_MSG_SIZE;
    }

    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t)(this->res.buf + index * config.DATA_MSG_SIZE);
    sge.length = msg_size;
    sge.lkey = this->res.mr->lkey;
    /* prepare the send work request */

    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    {
        sr.wr_id = DATA_WRITE;
        sr.wr_id += index;
    }
    sr.sg_list = &sge;
    sr.num_sge = 1;
    sr.opcode = IBV_WR_RDMA_WRITE;
    sr.send_flags &= (~IBV_SEND_SIGNALED);
    //sr.send_flags = IBV_SEND_SIGNALED;

    sr.wr.rdma.remote_addr = this->res.remote_props.addr + index * config.DATA_MSG_SIZE;
    sr.wr.rdma.rkey = this->res.remote_props.rkey;

    /* there is a Receive Request in the responder side, so we won't get any into RNR flow */
    if (ibv_post_send(this->res.qp, &sr, &bad_wr))
        LOG(FATAL) << "[error] failed to post SR with index=" << index;

    return 0;
}
int NPRDMAdapter::post_data_write_with_imm(int index, int imm, int msg_size)
{
    struct ibv_send_wr sr;
    struct ibv_sge sge;
    struct ibv_send_wr *bad_wr = NULL;
    if (index >= config.MSG_BLOCK_NUM)
    {
        LOG(WARNING) << "You should limited the index in [0, " << config.MSG_BLOCK_NUM
                     << "], you are specifying the index: " << index;
        index %= config.MSG_BLOCK_NUM;
    }
    /* prepare the scatter/gather entry */
    if (msg_size <= 0)
    {
        LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[out] msg_size is ilegal, set to default: "
                                             << config.DATA_MSG_SIZE;
        msg_size = config.DATA_MSG_SIZE;
    }

    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t)(this->res.buf + index * config.DATA_MSG_SIZE);
    sge.length = msg_size;
    sge.lkey = this->res.mr->lkey;
    /* prepare the send work request */

    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    {
        sr.wr_id = DATA_WRITE;
        sr.wr_id += index;
        sr.imm_data = imm;
    }
    sr.sg_list = &sge;
    sr.num_sge = 1;
    sr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
    sr.send_flags = IBV_SEND_SIGNALED;

    sr.wr.rdma.remote_addr = this->res.remote_props.addr + index * config.DATA_MSG_SIZE;
    sr.wr.rdma.rkey = this->res.remote_props.rkey;

    /* there is a Receive Request in the responder side, so we won't get any into RNR flow */
    if (ibv_post_send(this->res.qp, &sr, &bad_wr))
        LOG(FATAL) << "[error] failed to post SR with index=" << index << ", imm=" << imm;
    //else
    //    LOG(INFO) << "[out] RDMA Write Request was posted with index=" << index << ", imm=" << imm;

    return 0;
}

int NPRDMAdapter::post_receive()
{
    struct ibv_recv_wr rr;
    struct ibv_sge sge;
    struct ibv_recv_wr *bad_wr;
    int rc;
    /* prepare the scatter/gather entry */

    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t)(this->res.buf + this->res.current_block);
    sge.length = MSG_SIZE;
    sge.lkey = this->res.mr->lkey;
    /* prepare the receive work request */

    memset(&rr, 0, sizeof(rr));
    rr.next = NULL;
    rr.wr_id = this->res.current_block;
    rr.sg_list = &sge;
    rr.num_sge = 1;
    if (0)
    { //using the round-robin
        this->res.current_block++;
        this->res.current_block %= config.MSG_BLOCK_NUM;
    }
    /* post the Receive Request to the RQ */
    rc = ibv_post_recv(this->res.qp, &rr, &bad_wr);

    if (rc)
        fprintf(stderr, "failed to post RR\n");
    else
        fprintf(stdout, "Receive Request was posted\n");
    return rc;
}

void NPRDMAdapter::resources_init(NPRDMAPreConnector *con)
{
    LOG(INFO) << "Init Resources";
    memset(&res, 0, sizeof(struct resources));
    this->connector = con;
    { // first sync
        char local[128] = {0};
        char remote[128] = {0};

        memset(local, 0, 128);
        memset(remote, 0, 128);
        sprintf(local, "[0] Greetings from %s.", config.get_local_ip().c_str());
        connector->sock_sync_data(48, local, remote);
        LOG(INFO) << "[Exch]: " << remote;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

struct ibv_mr *NPRDMAdapter::regisger_mem(void **buf, int buf_size, int access_flags)
{
    int ret = 0;
    if (access_flags == 0)
        LOG(WARNING) << "No flags for the mem buf, choose the default configs";

    access_flags = IBV_ACCESS_LOCAL_WRITE |
                   IBV_ACCESS_REMOTE_READ |
                   IBV_ACCESS_REMOTE_WRITE;

    if (buf_size <= 0)
        LOG(FATAL) << "Error of register mem buf with size: " << buf_size;

    if (0 != (ret = posix_memalign(buf, sysconf(_SC_PAGESIZE), buf_size)))
        LOG(FATAL) << "[error] posix_memalign: " << strerror(ret);

    memset(*buf, 0, buf_size); //clean all the mem buf

    struct ibv_mr *tmp_mr = ibv_reg_mr(this->res.pd, *buf, buf_size, access_flags);
    if (tmp_mr == NULL || tmp_mr == nullptr)
        LOG(FATAL) << "[error] register mem failed";

    char msg[1024];
    sprintf(msg, "[OK] register mem @%p, with size: %d, and flags: 0x%x, lkey: 0x%x, rkey: 0x%x",
            *buf, buf_size, access_flags, tmp_mr->lkey, tmp_mr->rkey);

    LOG(INFO) << msg;

    return tmp_mr;
}

int NPRDMAdapter::resources_create()
{
    struct ibv_device **dev_list = NULL;
    struct ibv_qp_init_attr qp_init_attr;
    struct ibv_device *ib_dev = NULL;

    int i;
    int cq_size = 0;
    int num_devices;
    int rc = 0;

    /* get device names in the system */
    dev_list = ibv_get_device_list(&num_devices);
    if (!dev_list)
        LOG(FATAL) << "failed to get IB devices list";

    /* if there isn't any IB device in host */
    if (!num_devices)
        LOG(FATAL) << "found " << num_devices << " device(s)";

    LOG(INFO) << "found " << num_devices << " device(s)";

    /* search for the specific device we want to work with */
    for (i = 0; i < num_devices; i++)
    {
        if (!config.dev_name)
        {
            config.dev_name = strdup(ibv_get_device_name(dev_list[i]));
            LOG(INFO) << "device not specified, using first one found: " << config.dev_name;
        }
        if (!strcmp(ibv_get_device_name(dev_list[i]), config.dev_name))
        {
            ib_dev = dev_list[i];
            break;
        }
    }

    /* if the device wasn't found in host */
    if (!ib_dev)
        LOG(FATAL) << "Not find the IB device: " << config.dev_name;

    /* get device handle */
    this->res.ib_ctx = ibv_open_device(ib_dev);
    if (!this->res.ib_ctx)
        LOG(FATAL) << "failed to open device " << config.dev_name;

    /* We are now done with device list, free it */
    ibv_free_device_list(dev_list);
    dev_list = NULL;
    ib_dev = NULL;
    /* query port properties */
    if (ibv_query_port(this->res.ib_ctx, config.ib_port, &this->res.port_attr))
        LOG(FATAL) << "failed to bv_query_port on port " << config.ib_port;

    /*create event channel*/
    this->create_event_channel();

    /* allocate Protection Domain */
    this->res.pd = ibv_alloc_pd(this->res.ib_ctx);
    if (!this->res.pd)
        LOG(FATAL) << "ibv_alloc_pd failed";

    /* each side will send only one WR, so Completion Queue with 1 entry is enough */
    cq_size = 1024;
    this->res.cq = ibv_create_cq(this->res.ib_ctx, cq_size,
                                 this, this->res.channel, 0);
    if (!this->res.cq)
        LOG(FATAL) << "failed to create CQ entries with number: " << cq_size;

    if (config.use_event && ibv_req_notify_cq(res.cq, 0))
    {
        LOG(FATAL) << "Cannot request CQ notification";
    }
    if (config.use_event)
    {
        /* The following code will be called only once, after the Completion Event Channel 
was created，to change the blocking mode of the completion channel */
        int flags = fcntl(this->res.channel->fd, F_GETFL);
        rc = fcntl(this->res.channel->fd, F_SETFL, flags | O_NONBLOCK);
        if (rc < 0)
        {
            LOG(FATAL) << "error of  change file descriptor of Completion Event Channel";
        }
        LOG(INFO) << "Using epoll to listen the channel event";
    }

    /* allocate the memory buffer that will hold the data */
    this->res.mr = regisger_mem((void **)&(this->res.buf), config.DATA_MSG_SIZE * config.MSG_BLOCK_NUM);
    this->res.ctrl_mr = regisger_mem((void **)&(this->res.ctrl_buf), config.CTRL_MSG_SIZE * config.MSG_BLOCK_NUM);

    /* only in the server side put the message in the memory buffer */
    if (!config.serve_as_client)
    {
        strcpy(this->res.buf, MSG);
        LOG(INFO) << "[out] server send: " << this->res.buf;
    }

    /* create the Queue Pair */
    memset(&qp_init_attr, 0, sizeof(qp_init_attr));
    qp_init_attr.qp_type = IBV_QPT_RC;
    qp_init_attr.sq_sig_all = 0;

    if (this->res.signal_all)
    {
        qp_init_attr.sq_sig_all = 1;
        LOG(INFO) << "Would generate a signal for sqe";
    }
    else
        LOG(WARNING) << "Would not generate a signal for sqe";

    qp_init_attr.send_cq = this->res.cq;
    qp_init_attr.recv_cq = this->res.cq;
    qp_init_attr.cap.max_send_wr = 128;
    qp_init_attr.cap.max_recv_wr = 128;
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_recv_sge = 1;
    this->res.qp = ibv_create_qp(this->res.pd, &qp_init_attr);
    if (!this->res.qp)
    {
        fprintf(stderr, "failed to create QP\n");
        rc = 1;
        goto resources_create_exit;
    }
    fprintf(stdout, "QP was created, QP number=0x%x\n", this->res.qp->qp_num);
resources_create_exit:
    if (rc)
    {
        /* Error encountered, cleanup */
        if (this->res.qp)
        {
            ibv_destroy_qp(this->res.qp);
            this->res.qp = NULL;
        }
        if (this->res.mr)
        {
            ibv_dereg_mr(this->res.mr);
            this->res.mr = NULL;
        }
        if (this->res.buf)
        {
            free(this->res.buf);
            this->res.buf = NULL;
        }
        if (this->res.cq)
        {
            ibv_destroy_cq(this->res.cq);
            this->res.cq = NULL;
        }
        if (this->res.pd)
        {
            ibv_dealloc_pd(this->res.pd);
            this->res.pd = NULL;
        }
        if (this->res.ib_ctx)
        {
            ibv_close_device(this->res.ib_ctx);
            this->res.ib_ctx = NULL;
        }
        if (dev_list)
        {
            ibv_free_device_list(dev_list);
            dev_list = NULL;
        }
    }
    return rc;
}

int NPRDMAdapter::modify_qp_to_init(struct ibv_qp *qp)
{
    struct ibv_qp_attr attr;
    int flags;
    int rc;
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_INIT;
    attr.port_num = config.ib_port;
    attr.pkey_index = 0;
    attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE |
                           IBV_ACCESS_REMOTE_READ |
                           IBV_ACCESS_REMOTE_WRITE;
    flags = IBV_QP_STATE |
            IBV_QP_PKEY_INDEX |
            IBV_QP_PORT |
            IBV_QP_ACCESS_FLAGS;
    rc = ibv_modify_qp(qp, &attr, flags);
    if (rc)
        fprintf(stderr, "failed to modify QP state to INIT\n");
    return rc;
}

int NPRDMAdapter::modify_qp_to_rtr(struct ibv_qp *qp,
                                   uint32_t remote_qpn,
                                   uint16_t dlid,
                                   uint8_t *dgid)
{
    struct ibv_qp_attr attr;
    int flags;
    int rc;
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_RTR;
    attr.path_mtu = IBV_MTU_1024;
    attr.dest_qp_num = remote_qpn;
    attr.rq_psn = 0;
    attr.max_dest_rd_atomic = 1;
    attr.min_rnr_timer = 12; //0x12;
    attr.ah_attr.is_global = 0;
    attr.ah_attr.dlid = dlid;
    attr.ah_attr.sl = 0;
    attr.ah_attr.src_path_bits = 0;
    attr.ah_attr.port_num = config.ib_port;
    if (config.gid_idx >= 0)
    {
        attr.ah_attr.is_global = 1;
        attr.ah_attr.port_num = 1;
        memcpy(&attr.ah_attr.grh.dgid, dgid, 16);
        attr.ah_attr.grh.flow_label = 0;
        attr.ah_attr.grh.hop_limit = 1;
        attr.ah_attr.grh.sgid_index = config.gid_idx;
        attr.ah_attr.grh.traffic_class = config.traffic_class;
        LOG(WARNING) << "Using traffic class " << config.traffic_class / 32;
    }
    flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
            IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;
    rc = ibv_modify_qp(qp, &attr, flags);
    if (rc)
        fprintf(stderr, "failed to modify QP state to RTR\n");
    return rc;
}

int NPRDMAdapter::modify_qp_to_rts(struct ibv_qp *qp)
{
    struct ibv_qp_attr attr;
    int flags;
    int rc;
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_RTS;
    attr.timeout = 14;  //0x12;
    attr.retry_cnt = 7; //6;
    attr.rnr_retry = 7; //0;
    attr.sq_psn = 0;
    attr.max_rd_atomic = 1;
    flags = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
            IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;
    rc = ibv_modify_qp(qp, &attr, flags);
    if (rc)
        fprintf(stderr, "failed to modify QP state to RTS\n");
    return rc;
}

int NPRDMAdapter::connect_qp()
{
    struct cm_con_data_t local_con_data;
    struct cm_con_data_t remote_con_data;
    struct cm_con_data_t tmp_con_data;
    int rc = 0;
    union ibv_gid my_gid;
    if (config.gid_idx >= 0)
    {
        rc = ibv_query_gid(this->res.ib_ctx, config.ib_port, config.gid_idx, &my_gid);
        if (rc)
        {
            fprintf(stderr, "could not get gid for port %d, index %d\n", config.ib_port, config.gid_idx);
            return rc;
        }
    }
    else
        memset(&my_gid, 0, sizeof my_gid);
    /* exchange using TCP sockets info required to connect QPs */
    local_con_data.addr = htonll((uintptr_t)this->res.buf);
    local_con_data.rkey = htonl(this->res.mr->rkey);
    local_con_data.qp_num = htonl(this->res.qp->qp_num);
    local_con_data.lid = htons(this->res.port_attr.lid);
    memcpy(local_con_data.gid, &my_gid, 16);
    fprintf(stdout, "\nLocal LID = 0x%x\n", this->res.port_attr.lid);

    if (connector->sock_sync_data(sizeof(struct cm_con_data_t),
                                  (char *)&local_con_data,
                                  (char *)&tmp_con_data))
        LOG(FATAL) << "failed to exchange connection data between sides";

    remote_con_data.addr = ntohll(tmp_con_data.addr);
    remote_con_data.rkey = ntohl(tmp_con_data.rkey);
    remote_con_data.qp_num = ntohl(tmp_con_data.qp_num);
    remote_con_data.lid = ntohs(tmp_con_data.lid);
    memcpy(remote_con_data.gid, tmp_con_data.gid, 16);
    /* save the remote side attributes, we will need it for the post SR */
    this->res.remote_props = remote_con_data;
    printf("\n======================================================\n");
    fprintf(stdout, "Remote address = 0x%" PRIx64 "\n", remote_con_data.addr);
    fprintf(stdout, "Remote rkey = 0x%x\n", remote_con_data.rkey);
    fprintf(stdout, "Remote QP number = 0x%x\n", remote_con_data.qp_num);
    fprintf(stdout, "Remote LID = 0x%x\n", remote_con_data.lid);
    printf("======================================================\n");
    if (config.gid_idx >= 0)
    {
        uint8_t *p = remote_con_data.gid;
        fprintf(stdout, "Remote GID =%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n ",
                p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
    }
    /* modify the QP to init */
    if (modify_qp_to_init(this->res.qp))
        LOG(FATAL) << "change QP state to INIT failed";

        /* let the client post RR to be prepared for incoming messages */
#ifndef DEBUG_TWO_CHANNEL
    if (config.serve_as_client)
    {
        if (post_receive())
            LOG(FATAL) << "failed to post RR";
    }
#endif
    /* modify the QP to RTR */
    if (modify_qp_to_rtr(this->res.qp,
                         remote_con_data.qp_num,
                         remote_con_data.lid,
                         remote_con_data.gid))
        LOG(FATAL) << "failed to modify QP state to RTR";

    if (modify_qp_to_rts(this->res.qp))
        LOG(FATAL) << "failed to modify QP state to RTR";

    LOG(INFO) << "QP state was change to RTS";
    /* sync to make sure that both sides are in states that they can connect to prevent packet loose */

    char temp_char;
    if (connector->sock_sync_data(1, (char *)"Q", &temp_char))
        LOG(FATAL) << "sync error after QPs are were moved to RTS";

    return 0;
}

int NPRDMAdapter::resources_destroy()
{
    int rc = 0;
    if (this->res.qp)
        if (ibv_destroy_qp(this->res.qp))
        {
            fprintf(stderr, "failed to destroy QP\n");
            rc = 1;
        }
    if (this->res.mr)
        if (ibv_dereg_mr(this->res.mr))
        {
            fprintf(stderr, "failed to deregister MR\n");
            rc = 1;
        }
    if (this->res.ctrl_mr)
    {
        if (ibv_dereg_mr(this->res.ctrl_mr))
        {
            fprintf(stderr, "failed to deregister MR\n");
            rc = 1;
        }
    }
    if (this->res.buf)
        free(this->res.buf);
    if (this->res.ctrl_buf)
        free(this->res.ctrl_buf);

    if (this->res.cq)
        if (ibv_destroy_cq(this->res.cq))
        {
            fprintf(stderr, "failed to destroy CQ\n");
            rc = 1;
        }
    if (this->res.pd)
        if (ibv_dealloc_pd(this->res.pd))
        {
            fprintf(stderr, "failed to deallocate PD\n");
            rc = 1;
        }
    if (this->res.ib_ctx)
        if (ibv_close_device(this->res.ib_ctx))
        {
            fprintf(stderr, "failed to close device context\n");
            rc = 1;
        }

    return rc;
}

bool NPRDMAdapter::create_event_channel()
{
    // Create a completion channel
    if (config.use_event)
    {
        if (!(this->res.channel = ibv_create_comp_channel(this->res.ib_ctx)))
            LOG(FATAL) << "Cannot create completion channel";
        LOG(INFO) << "Using event channel";
    }
    else
    {
        LOG(INFO) << "Do not use the event channel";
        this->res.channel = NULL;
    }
    return true;
}