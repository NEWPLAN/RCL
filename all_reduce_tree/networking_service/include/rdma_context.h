#ifndef __NEWPLAN_RDMA_CONTEXT_H__
#define __NEWPLAN_RDMA_CONTEXT_H__

#include <infiniband/verbs.h>

class RDMAContext
{
public:
    virtual ~RDMAContext() {}

public:
    // virtual void init_context() = 0;
    // virtual void connect_qp() = 0;

protected:
    struct ibv_device_attr device_attr; /* Device attributes */
    struct ibv_port_attr port_attr;     /* IB port attributes */
    struct cm_con_data_t remote_props;  /* values to connect to remote side */
    struct ibv_context *ib_ctx = 0;     /* device handle */
    struct ibv_pd *pd = 0;              /* PD handle */
    struct ibv_cq *cq = 0;              /* CQ handle */
    struct ibv_qp *qp = 0;              /* QP handle */
    struct ibv_mr *mr = 0;              /* MR handle for buf */
    char *buf = 0;                      /* memory buffer pointer, used for RDMA and send ops */
};

#endif //