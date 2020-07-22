#include "common.h"

const int TIMEOUT_IN_MS = 500;

//static struct context *s_ctx = NULL;
static pre_conn_cb_fn s_on_pre_conn_cb = NULL;
static connect_cb_fn s_on_connect_cb = NULL;
static completion_cb_fn s_on_completion_cb = NULL;
static disconnect_cb_fn s_on_disconnect_cb = NULL;

static void build_context(struct rdma_cm_id *id);
static void build_qp_attr(struct context *qp_ctx,
                          struct ibv_qp_init_attr *qp_attr);
static void event_loop(struct rdma_event_channel *ec, int exit_on_disconnect);
static void *poll_cq(void *);

void build_connection(struct rdma_cm_id *id)
{
  struct ibv_qp_init_attr qp_attr;

  build_context(id);

  struct SessionContext *ctx = (struct SessionContext *)id->context;
  if (ctx == 0)
  {
    LOG(FATAL) << "Cannot process the empty Session CTX";
  }
  struct context *qp_ctx = ctx->qp_ctx;

  build_qp_attr(qp_ctx, &qp_attr);

  TEST_NZ(rdma_create_qp(id, qp_ctx->pd, &qp_attr));
}
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
void build_context(struct rdma_cm_id *id)
{

  struct sockaddr *peer_addr = rdma_get_peer_addr(id);
  struct sockaddr *local_addr = rdma_get_local_addr(id);
  struct sockaddr_in *server_addr = (struct sockaddr_in *)peer_addr;
  struct sockaddr_in *client_addr = (struct sockaddr_in *)local_addr;

  printf("----[%s:%d] has connected to [%s:%d]----\n",
         inet_ntoa(client_addr->sin_addr),
         ntohs(client_addr->sin_port),
         inet_ntoa(server_addr->sin_addr),
         ntohs(server_addr->sin_port));

  struct SessionContext *ctx = (struct SessionContext *)id->context;
  if (ctx == 0)
  {
    LOG(WARNING) << "The RDMA Session Ctx is empty";
    ctx = (struct SessionContext *)malloc(sizeof(struct SessionContext));
    memset(ctx, 0, sizeof(struct SessionContext));
    id->context = ctx;
  }

  struct context *qp_ctx = ctx->qp_ctx;

  struct ibv_context *verbs = id->verbs;
  if (qp_ctx)
  {
    if (qp_ctx->ctx != verbs)
      rc_die("cannot handle events in more than one context.");

    return;
  }

  qp_ctx = (struct context *)malloc(sizeof(struct context));
  memset(qp_ctx, 0, sizeof(struct context));
  ctx->qp_ctx = qp_ctx;

  qp_ctx->ctx = verbs;

  TEST_Z(qp_ctx->pd = ibv_alloc_pd(qp_ctx->ctx));
  TEST_Z(qp_ctx->comp_channel = ibv_create_comp_channel(qp_ctx->ctx));
  TEST_Z(qp_ctx->cq = ibv_create_cq(qp_ctx->ctx, 10 * 4, NULL, qp_ctx->comp_channel, 0)); /* cqe=10 is arbitrary */
  TEST_NZ(ibv_req_notify_cq(qp_ctx->cq, 0));

  TEST_NZ(pthread_create(&qp_ctx->cq_poller_thread, NULL, poll_cq, (void *)qp_ctx));
}

void build_params(struct rdma_conn_param *params)
{
  memset(params, 0, sizeof(*params));

  params->initiator_depth = params->responder_resources = 1;
  params->rnr_retry_count = 7; /* infinite retry */
}

void build_qp_attr(struct context *qp_ctx,
                   struct ibv_qp_init_attr *qp_attr)
{
  memset(qp_attr, 0, sizeof(*qp_attr));

  qp_attr->send_cq = qp_ctx->cq;
  qp_attr->recv_cq = qp_ctx->cq;
  qp_attr->qp_type = IBV_QPT_RC;

  qp_attr->cap.max_send_wr = 10;
  qp_attr->cap.max_recv_wr = 10;
  qp_attr->cap.max_send_sge = 1;
  qp_attr->cap.max_recv_sge = 1;
}

void event_loop(struct rdma_event_channel *ec, int exit_on_disconnect)
{
  struct rdma_cm_event *event = NULL;
  struct rdma_conn_param cm_params;

  build_params(&cm_params);

  while (rdma_get_cm_event(ec, &event) == 0)
  {
    struct rdma_cm_event event_copy;

    memcpy(&event_copy, event, sizeof(*event));
    rdma_ack_cm_event(event);

    if (event_copy.event == RDMA_CM_EVENT_ADDR_RESOLVED)
    {
      build_connection(event_copy.id);

      if (s_on_pre_conn_cb)
        s_on_pre_conn_cb(event_copy.id);

      {
        uint8_t tos = 96; //https://blog.csdn.net/sunshuying1010/article/details/103661289
        if (rdma_set_option(event_copy.id, RDMA_OPTION_ID,
                            RDMA_OPTION_ID_TOS, &tos, sizeof(uint8_t)))
        {
          LOG(FATAL) << "Failed to set ToS(Type of Service) option for RDMA CM connection.";
        }
      }

      TEST_NZ(rdma_resolve_route(event_copy.id, TIMEOUT_IN_MS));
    }
    else if (event_copy.event == RDMA_CM_EVENT_ROUTE_RESOLVED)
    {
      TEST_NZ(rdma_connect(event_copy.id, &cm_params));
    }
    else if (event_copy.event == RDMA_CM_EVENT_CONNECT_REQUEST)
    {
      build_connection(event_copy.id);

      if (s_on_pre_conn_cb)
        s_on_pre_conn_cb(event_copy.id);

      TEST_NZ(rdma_accept(event_copy.id, &cm_params));
    }
    else if (event_copy.event == RDMA_CM_EVENT_ESTABLISHED)
    {
      if (s_on_connect_cb)
        s_on_connect_cb(event_copy.id);
    }
    else if (event_copy.event == RDMA_CM_EVENT_DISCONNECTED)
    {
      rdma_destroy_qp(event_copy.id);

      if (s_on_disconnect_cb)
        s_on_disconnect_cb(event_copy.id);

      rdma_destroy_id(event_copy.id);

      if (exit_on_disconnect)
        break;
    }
    else
    {
      rc_die("unknown event\n");
    }
  }
}

void *poll_cq(void *qp_ctx_)
{
  struct ibv_cq *cq;
  struct ibv_wc wc;
  void *ctx = 0;
  struct context *qp_ctx = (struct context *)qp_ctx_;

  while (1)
  {
    TEST_NZ(ibv_get_cq_event(qp_ctx->comp_channel, &cq, &ctx));
    ibv_ack_cq_events(cq, 1);
    TEST_NZ(ibv_req_notify_cq(cq, 0));

    while (ibv_poll_cq(cq, 1, &wc))
    {
      if (wc.status == IBV_WC_SUCCESS)
        s_on_completion_cb(&wc);
      else
        rc_die("poll_cq: status is not IBV_WC_SUCCESS");
    }
  }

  return NULL;
}

void rc_init(pre_conn_cb_fn pc, connect_cb_fn conn, completion_cb_fn comp, disconnect_cb_fn disc)
{
  s_on_pre_conn_cb = pc;
  s_on_connect_cb = conn;
  s_on_completion_cb = comp;
  s_on_disconnect_cb = disc;
}

void rc_client_loop(const char *host, const char *port, void *context)
{
  struct addrinfo *addr;
  struct rdma_cm_id *conn = NULL;
  struct rdma_event_channel *ec = NULL;
  struct rdma_conn_param cm_params;

  TEST_NZ(getaddrinfo(host, port, NULL, &addr));

  TEST_Z(ec = rdma_create_event_channel());
  TEST_NZ(rdma_create_id(ec, &conn, NULL, RDMA_PS_TCP));
  TEST_NZ(rdma_resolve_addr(conn, NULL, addr->ai_addr, TIMEOUT_IN_MS));

  freeaddrinfo(addr);

  conn->context = context;

  build_params(&cm_params);

  event_loop(ec, 1); // exit on disconnect

  rdma_destroy_event_channel(ec);
}

void rc_server_loop(const char *port)
{
  struct sockaddr_in6 addr;
  struct rdma_cm_id *listener = NULL;
  struct rdma_event_channel *ec = NULL;
  //uint8_t reuse = 1;

  memset(&addr, 0, sizeof(addr));
  addr.sin6_family = AF_INET6;
  addr.sin6_port = htons(atoi(port));

  TEST_Z(ec = rdma_create_event_channel());
  TEST_NZ(rdma_create_id(ec, &listener, NULL, RDMA_PS_TCP));
  // if (rdma_set_option(listener, RDMA_OPTION_ID, RDMA_OPTION_ID_REUSEADDR, &reuse, sizeof(uint8_t)) != 0)
  // {
  //   printf("can not set reuse for IP\n");
  //   exit(0);
  // }
  TEST_NZ(rdma_bind_addr(listener, (struct sockaddr *)&addr));
  TEST_NZ(rdma_listen(listener, 10)); /* backlog=10 is arbitrary */

  event_loop(ec, 0); // don't exit on disconnect

  rdma_destroy_id(listener);
  rdma_destroy_event_channel(ec);
}

void rc_disconnect(struct rdma_cm_id *id)
{
  rdma_disconnect(id);
}

void rc_die(const char *reason)
{
  fprintf(stderr, "%s\n", reason);
  exit(EXIT_FAILURE);
}

struct ibv_pd *rc_get_pd(struct SessionContext *ctx)
{
  return ctx->qp_ctx->pd;
}
