#include <fcntl.h>
#include <libgen.h>

#include "common.h"
#include "messages.h"
#include "NetUtil.h"

static void write_remote(struct rdma_cm_id *id, uint32_t len)
{
  struct SessionContext *ctx = (struct SessionContext *)id->context;

  struct ibv_send_wr wr, *bad_wr = NULL;
  struct ibv_sge sge;

  memset(&wr, 0, sizeof(wr));

  wr.wr_id = (uintptr_t)id;
  wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
  wr.send_flags = IBV_SEND_SIGNALED;
  wr.imm_data = htonl(len);
  wr.wr.rdma.remote_addr = ctx->peer_addr;
  wr.wr.rdma.rkey = ctx->peer_rkey;

  if (len)
  {
    wr.sg_list = &sge;
    wr.num_sge = 1;

    sge.addr = (uintptr_t)ctx->buffer;
    sge.length = len;
    sge.lkey = ctx->buffer_mr->lkey;
  }

  TEST_NZ(ibv_post_send(id->qp, &wr, &bad_wr));
}

static void post_receive(struct rdma_cm_id *id)
{
  struct SessionContext *ctx = (struct SessionContext *)id->context;

  struct ibv_recv_wr wr, *bad_wr = NULL;
  struct ibv_sge sge;

  memset(&wr, 0, sizeof(wr));

  wr.wr_id = (uintptr_t)id;
  wr.sg_list = &sge;
  wr.num_sge = 1;

  sge.addr = (uintptr_t)ctx->msg;
  sge.length = sizeof(*ctx->msg);
  sge.lkey = ctx->msg_mr->lkey;

  TEST_NZ(ibv_post_recv(id->qp, &wr, &bad_wr));
}

static void send_next_chunk(struct rdma_cm_id *id)
{
  struct SessionContext *ctx = (struct SessionContext *)id->context;

  ssize_t size = 0;
  ctx->buffer[0] = '1';
  size = BUFFER_SIZE;
  if (size == -1)
    rc_die("Loading IP failed\n");

  write_remote(id, size);
}

static void send_file_name(struct rdma_cm_id *id)
{
  struct SessionContext *ctx = (struct SessionContext *)id->context;
  sprintf(ctx->buffer, ctx->ip_addr, 12);
  //strcpy(ctx->buffer, ctx->file_name);

  write_remote(id, 12 + 1);
}

static void on_pre_conn(struct rdma_cm_id *id)
{
  struct SessionContext *ctx = (struct SessionContext *)id->context;

  if (posix_memalign((void **)&ctx->buffer, sysconf(_SC_PAGESIZE), BUFFER_SIZE) != 0)
  {
    printf("Error of malloc DATA memory: %lu\n", BUFFER_SIZE);
    exit(0);
  }
  TEST_Z(ctx->buffer_mr = ibv_reg_mr(rc_get_pd(ctx), ctx->buffer, BUFFER_SIZE, 0));

  if (posix_memalign((void **)&ctx->msg, sysconf(_SC_PAGESIZE), sizeof(*ctx->msg)) != 0)
  {
    printf("Error of malloc ACK memory: %lu\n", sizeof(*ctx->msg));
    exit(0);
  }
  TEST_Z(ctx->msg_mr = ibv_reg_mr(rc_get_pd(ctx), ctx->msg, sizeof(*ctx->msg), IBV_ACCESS_LOCAL_WRITE));

  post_receive(id);
}
#include <glog/logging.h>
static void on_completion(struct ibv_wc *wc)
{
  struct rdma_cm_id *id = (struct rdma_cm_id *)(uintptr_t)(wc->wr_id);
  struct SessionContext *ctx = (struct SessionContext *)id->context;

  if (wc->opcode & IBV_WC_RECV)
  {
    if (ctx->msg->id == MSG_MR)
    {
      ctx->peer_addr = ctx->msg->data.mr.addr;
      ctx->peer_rkey = ctx->msg->data.mr.rkey;

      printf("received MR, sending Local IP %s\n", ctx->ip_addr);
      send_file_name(id);
    }
    else if (ctx->msg->id == MSG_READY)
    {
      LOG_EVERY_N(INFO, 1000) << "Receive Ready, send Next chunk";
      send_next_chunk(id);
    }
    else if (ctx->msg->id == MSG_DONE)
    {
      printf("received DONE, disconnecting\n");
      rc_disconnect(id);
      return;
    }

    post_receive(id);
  }
}

#include <string>
#include <iostream>
#include <cstdlib>

int main(int argc, char **argv)
{
  struct SessionContext ctx;
  memset(&ctx, 0, sizeof(struct SessionContext));

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <server-address>\n", argv[0]);
    return 1;
  }

  std::string local_ip = NetTool::get_ip("12.12.12.1", 24);
  std::cout << "Loading local IP: " << local_ip << std::endl;
  sprintf(ctx.ip_addr, "%s", local_ip.c_str());

  rc_init(
      on_pre_conn,
      NULL, // on connect
      on_completion,
      NULL); // on disconnect

  rc_client_loop(argv[1], DEFAULT_PORT, &ctx);

  return 0;
}
