#include "common.h"

// print error information and exit
void die(const char *reason)
{
        fprintf(stderr, "%s\n", reason);
        exit(EXIT_FAILURE);
}

// Initialize Queue Pair (QR) attributions 
void build_qp_attr(struct ibv_qp_init_attr *qp_attr, struct ibv_cq *cq)
{       
        memset(qp_attr, 0, sizeof(*qp_attr));

        qp_attr->send_cq = cq;		// send completion queue
        qp_attr->recv_cq = cq;          // receive completion queue
        qp_attr->qp_type = IBV_QPT_RC;  // reliable connection-oriented 
 
        qp_attr->cap.max_send_wr = MAX_SEND_WR;  
        qp_attr->cap.max_recv_wr = MAX_RECV_WR;  
        qp_attr->cap.max_send_sge = 1;
        qp_attr->cap.max_recv_sge = 1;
}

// Register send memory of a connection
void register_send_memory(struct connection *conn, struct ibv_pd *pd)
{
	TEST_Z(conn->send_region = malloc(conn->msg_size));

	TEST_Z(conn->send_mr = ibv_reg_mr(
               pd, 
               conn->send_region, 
               conn->msg_size, 
               IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE));
}

// Register receive memory of a connection
void register_recv_memory(struct connection *conn, struct ibv_pd *pd)
{

        TEST_Z(conn->recv_region = malloc(conn->msg_size));

        TEST_Z(conn->recv_mr = ibv_reg_mr(
               pd, 
               conn->recv_region, 
               conn->msg_size, 
               IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE));        
}

// Free send memory of a connection
void free_send_memory(struct connection *conn)
{
        ibv_dereg_mr(conn->send_mr);
        free(conn->send_region);
}

// Free receive memory of a connection
void free_recv_memory(struct connection *conn)
{
        ibv_dereg_mr(conn->recv_mr);
        free(conn->recv_region);
}

// Post a send operation to the send queue
void post_send(struct connection *conn)
{
        struct ibv_send_wr wr, *bad_wr = NULL;
        struct ibv_sge sge;

        memset(&wr, 0, sizeof(wr));
        wr.wr_id = (uintptr_t)conn;
        wr.opcode = IBV_WR_SEND;
        wr.sg_list = &sge;
        wr.num_sge = 1;
        wr.send_flags = IBV_SEND_SIGNALED;

        sge.addr = (uintptr_t)conn->send_region;
        sge.length = conn->msg_size;
        sge.lkey = conn->send_mr->lkey;

        TEST_NZ(ibv_post_send(conn->qp, &wr, &bad_wr));     
}

// Post a receive operation to the receive queue
void post_receive(struct connection *conn)
{
        struct ibv_recv_wr wr, *bad_wr = NULL;
        struct ibv_sge sge;

        wr.wr_id = (uintptr_t)conn;
        wr.next = NULL;
        wr.sg_list = &sge;
        wr.num_sge = 1;

        sge.addr = (uintptr_t)conn->recv_region;
        sge.length = conn->msg_size;
        sge.lkey = conn->recv_mr->lkey;

        TEST_NZ(ibv_post_recv(conn->qp, &wr, &bad_wr));
}
