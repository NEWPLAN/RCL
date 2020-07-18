#include "rdma_endpoint.h"
#include "util.h"

#include <random>

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

RDMAEndpoint::RDMAEndpoint(ibv_pd *pd, ibv_cq *cq,
                           ibv_context *context,
                           int ib_port, int cq_size,
                           RDMAMemMgr *mem_mgr)
    : pd_(pd), ib_port_(ib_port), connected_(false), mempool_(mem_mgr)
{
    // create the Queue Pair
    struct ibv_qp_init_attr qp_init_attr;
    memset(&qp_init_attr, 0, sizeof(qp_init_attr));

    qp_init_attr.qp_type = IBV_QPT_RC;
    // qp_init_attr.sq_sig_all = 1;
    qp_init_attr.send_cq = cq;
    qp_init_attr.recv_cq = cq;
    qp_init_attr.cap.max_send_wr = cq_size;
    qp_init_attr.cap.max_recv_wr = cq_size;
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_recv_sge = 1;

    qp_ = ibv_create_qp(pd_, &qp_init_attr);
    if (!qp_)
    {
        LOG(FATAL) << "failed to create QP";
        return;
    }
    char data_info[1024];
    sprintf(data_info, "QP was created, QP number=0x%x\0", qp_->qp_num);
    LOG(INFO) << data_info;

    // Local address
    struct ibv_port_attr attr;
    if (ibv_query_port(context, ib_port_, &attr))
    {
        LOG(FATAL) << "ibv_query_port failed  on port" << ib_port_;
        return;
    }

    union ibv_gid my_gid;
    int gid_index = 3;
    if (ibv_query_gid(context, ib_port_, gid_index, &my_gid))
    {
        char buf_res[1024];
        sprintf(buf_res, "could not get gid for port %d, index %d\n", ib_port, gid_index);
        LOG(FATAL) << buf_res;
    }

    self_.lid = attr.lid;
    self_.qpn = qp_->qp_num;
    self_.psn = static_cast<uint32_t>(New64()) & 0xffffff;
    memcpy(self_.gid, &my_gid, 16);

    //modify_qp_to_init();
}