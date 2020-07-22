#include <fcntl.h>
#include <sys/stat.h>

#include "common.h"
#include "messages.h"
#include <glog/logging.h>

static void send_message(struct rdma_cm_id *id)
{
  struct SessionContext *ctx = (struct SessionContext *)id->context;

  struct ibv_send_wr wr, *bad_wr = NULL;
  struct ibv_sge sge;

  memset(&wr, 0, sizeof(wr));

  wr.wr_id = (uintptr_t)id;
  wr.opcode = IBV_WR_SEND;
  wr.sg_list = &sge;
  wr.num_sge = 1;
  wr.send_flags = IBV_SEND_SIGNALED;

  sge.addr = (uintptr_t)ctx->msg;
  sge.length = sizeof(*ctx->msg);
  sge.lkey = ctx->msg_mr->lkey;

  TEST_NZ(ibv_post_send(id->qp, &wr, &bad_wr));
}

static void post_receive(struct rdma_cm_id *id)
{
  struct ibv_recv_wr wr, *bad_wr = NULL;

  memset(&wr, 0, sizeof(wr));

  wr.wr_id = (uintptr_t)id;
  wr.sg_list = NULL;
  wr.num_sge = 0;

  TEST_NZ(ibv_post_recv(id->qp, &wr, &bad_wr));
}

static void on_pre_conn(struct rdma_cm_id *id)
{

  LOG(INFO) << "Server on pre connection";
  struct SessionContext *ctx = (struct SessionContext *)id->context;
  if (ctx == 0)
  {
    ctx = (struct SessionContext *)malloc(sizeof(struct SessionContext));
    memset(ctx, 0, sizeof(struct SessionContext));
    id->context = ctx;
  }
  if (ctx->buffer == 0)
  {

    if (posix_memalign((void **)&ctx->buffer, sysconf(_SC_PAGESIZE), BUFFER_SIZE) != 0)
    {
      printf("Error of malloc DATA memory: %lu\n", BUFFER_SIZE);
      exit(0);
    }
    TEST_Z(ctx->buffer_mr = ibv_reg_mr(rc_get_pd(ctx), ctx->buffer, BUFFER_SIZE,
                                       IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE));
  }
  if (ctx->msg == 0)
  {
    if (posix_memalign((void **)&ctx->msg, sysconf(_SC_PAGESIZE), sizeof(*ctx->msg)) != 0)
    {
      printf("Error of malloc ACK memory: %lu\n", sizeof(*ctx->msg));
      exit(0);
    }
    TEST_Z(ctx->msg_mr = ibv_reg_mr(rc_get_pd(ctx), ctx->msg, sizeof(*ctx->msg), 0));
  }

  post_receive(id);
}

static void on_connection(struct rdma_cm_id *id)
{
  struct SessionContext *ctx = (struct SessionContext *)id->context;

  ctx->msg->id = MSG_MR;
  ctx->msg->data.mr.addr = (uintptr_t)ctx->buffer_mr->addr;
  ctx->msg->data.mr.rkey = ctx->buffer_mr->rkey;

  send_message(id);
}
#include <glog/logging.h>
static void on_completion(struct ibv_wc *wc)
{
  struct rdma_cm_id *id = (struct rdma_cm_id *)(uintptr_t)wc->wr_id;
  struct SessionContext *ctx = (struct SessionContext *)id->context;

  if (wc->opcode == IBV_WC_RECV_RDMA_WITH_IMM)
  {
    uint32_t size = ntohl(wc->imm_data);

    if (size == 0)
    {
      ctx->msg->id = MSG_DONE;
      send_message(id);

      // don't need post_receive() since we're done with this connection
    }
    else if (ctx->ip_addr[0])
    {

      LOG_EVERY_N(INFO, 1000) << "Receive Bytes: " << size;

      post_receive(id);

      ctx->msg->id = MSG_READY;
      send_message(id);
    }
    else
    {
      sprintf(ctx->ip_addr, ctx->buffer, "12");
      ctx->ip_addr[12] = 0;
      printf("Receive peer connections: %s\n", ctx->ip_addr);

      post_receive(id);

      ctx->msg->id = MSG_READY;
      send_message(id);
    }
  }
}

static void on_disconnect(struct rdma_cm_id *id)
{
  struct SessionContext *ctx = (struct SessionContext *)id->context;

  ibv_dereg_mr(ctx->buffer_mr);
  ibv_dereg_mr(ctx->msg_mr);

  free(ctx->buffer);
  free(ctx->msg);

  printf("finished transferring %s\n", ctx->ip_addr);

  free(ctx);
}

int main(int argc, char **argv)
{
  rc_init(
      on_pre_conn,
      on_connection,
      on_completion,
      on_disconnect);

  printf("waiting for connections. interrupt (^C) to exit.\n");

  rc_server_loop(DEFAULT_PORT);

  return 0;
}
