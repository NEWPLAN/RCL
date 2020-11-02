#include "RDMAServer.h"
#include <iostream>
#include <rdma/rdma_cma.h>

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <iostream>

#include <thread>
#include <fcntl.h>
//#include "../utils/ATimer.h"

RDMAServer::RDMAServer(RDMAAdapter &rdma_adapter)
{
    this->rdma_adapter_ = rdma_adapter;
    LOG_INFO("Creating RDMAServer\n");
}
RDMAServer::RDMAServer(const std::string &server_ip)
{
    this->rdma_adapter_.set_server_ip(server_ip.c_str());
}

RDMAServer::~RDMAServer()
{
    LOG_INFO("Destroy RDMAServer\n");
}
// 阻塞. 
void RDMAServer::setup()
{
    LOG_INFO("Setup RDMAServer\n");
    this->_init();
}
void RDMAServer::start_service()
{
    LOG_INFO("RDMA is starting service\n");
}

void RDMAServer::server_event_loops()
{
    struct rdma_cm_event *event = NULL;
    struct rdma_conn_param cm_params;

    LOG_INFO("RDMAServer is inited, waiting connections from client\n");

    build_params(&cm_params);

    while (rdma_get_cm_event(this->rdma_adapter_.event_channel,
                             &event) == 0)
    {
        struct rdma_cm_event event_copy;

        memcpy(&event_copy, event, sizeof(*event));
        rdma_ack_cm_event(event);

        switch (event_copy.event)
        {
        case RDMA_CM_EVENT_CONNECT_REQUEST:
        {
            LOG_INFO("Server: RDMA_CM_EVENT_CONNECT_REQUEST\n");
            build_connection(event_copy.id);
            on_pre_conn(event_copy.id);
            //add qos here
            TEST_NZ(rdma_accept(event_copy.id, &cm_params));
            break;
        }
        case RDMA_CM_EVENT_ESTABLISHED:
        {
            LOG_INFO("Server: RDMA_CM_EVENT_ESTABLISHED\n");
            on_connection(event_copy.id);
            this->rdma_adapter_.recv_rdma_cm_id.push_back(event_copy.id);
            break;
        }
        case RDMA_CM_EVENT_DISCONNECTED:
        {
            LOG_INFO("Server: RDMA_CM_EVENT_DISCONNECTED\n");
            rdma_destroy_qp(event_copy.id);
            on_disconnect(event_copy.id);
            rdma_destroy_id(event_copy.id);
            break;
        }
        default:
            rc_die("unknown event server\n");
            break;
        }
    }

    while (1)
    {
        LOG_INFO("RDMAServer will never come here\n");
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    printf("RDMA recv loops exit now...\n");
    return;
}
void RDMAServer::_init()
{
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;                              /*ipv4*/
    sin.sin_port = htons(this->rdma_adapter_.server_port); /*server listen public ports*/
    sin.sin_addr.s_addr = INADDR_ANY;                      /*listen any connects*/

    LOG_INFO("Listen port: %d\n", this->rdma_adapter_.server_port);

    TEST_Z(this->rdma_adapter_.event_channel = rdma_create_event_channel());
    TEST_NZ(rdma_create_id(this->rdma_adapter_.event_channel,
                           &this->rdma_adapter_.listener,
                           NULL, RDMA_PS_TCP));

    TEST_NZ(rdma_bind_addr(this->rdma_adapter_.listener,
                           (struct sockaddr *)&sin));

    TEST_NZ(rdma_listen(this->rdma_adapter_.listener, 100));

    this->aggregator_thread = new std::thread([this]() { this->server_event_loops(); });

    while (1)
    {
        int time_duration = 5;
        std::this_thread::sleep_for(std::chrono::seconds(time_duration));
        this->show_performance(time_duration);
    }
}

inline void parse_from_imm_data(uint32_t imm_data,
                                uint32_t *window_id,
                                uint32_t *buffer_id,
                                uint32_t *data_size)
{
    //***************************************
    //          immediate data:
    //        256*256*65536 = 4G
    // _____________________________________
    // |   31-24   |   23-16   |    15-0   |
    // | buffer id | window id | data size |
    //***************************************
    uint32_t imm_data_recv = ntohl(imm_data);
    *data_size = imm_data_recv & 0XFFFF;
    *buffer_id = (imm_data_recv >> 24) & 0xFF;
    *window_id = (imm_data_recv >> 16) & 0xFF;
    return;
}
/* DictXiong: 暂时不删, 是因为还可以用来参考
#include "timer.h"
void RDMAServer::sync_thread_func()
{
    LOG(INFO) << "Here is for notify poll-cq-thread";
    std::this_thread::sleep_for(std::chrono::seconds(1));
    int ready_num = 0;
    char *bitmap = new char[32];
    int max_QP = ctx_group.size();

    uint64_t epoch = 1;

    memset(bitmap, 0, 32);
    int sig = 1;

    for (auto &each_ctx : ctx_group)
    {
        each_ctx->q1->push(sig);
    }
    std::vector<std::chrono::high_resolution_clock::time_point> time_record;

#define EPOCH_ROUND 3997

    while (true)
    {
        for (int index = 0; index < max_QP; index++)
        {
            if (bitmap[index] == 0)
            {
                int value;
                if (ctx_group[index]->q2->try_pop(&value) == true)
                {
                    ready_num++;
                    bitmap[index] = 1;
                    LOG_EVERY_N(INFO, 10000) << "Recv data size: " << value << " round: " << epoch;
                    if (epoch == EPOCH_ROUND)
                    {
                        time_record.push_back(std::chrono::high_resolution_clock::now());
                    }
                }
            }
        }
        if (ready_num == max_QP)
        {
            if (epoch == EPOCH_ROUND - 1)
            {
                time_record.clear();
                time_record.push_back(std::chrono::high_resolution_clock::now());
            }
            for (auto &each_ctx : ctx_group)
            {
                size_t size = 0;

                if ((each_ctx->q2->nonblocking_size(&size) == true) && (size != 0))
                {
                    LOG(WARNING) << "MUST be empty";
                }

                each_ctx->q1->push(sig); //signal to notify it should send now
            }
            ready_num = 0;
            memset(bitmap, 0, 32);

            if (epoch == EPOCH_ROUND)
            {
                char buf[512] = {0};
                sprintf(buf, "%s", "[us] 0");
                for (int index = 1; index <= max_QP; index++)
                {
                    uint64_t time_use = std::chrono::duration_cast<std::chrono::microseconds>(time_record[index] - time_record[0]).count();
                    sprintf(buf + strlen(buf), ":%lu ", time_use);
                }
                epoch = 0;
                time_record.clear();
                LOG(INFO) << buf;
            }
            epoch++;
        }
    }
}
*/

void *RDMAServer::poll_cq(void *_id)
{
    struct ibv_cq *cq = NULL;
    //struct ibv_wc wc;
    struct ibv_wc wcs[MAX_DATA_IN_FLIGHT * 2];
    struct rdma_cm_id *id = (rdma_cm_id *)_id;

    struct RDMAContext *ctx = (struct RDMAContext *)id->context;
    if (ctx == NULL || ctx == 0 || ctx == nullptr)
    {
        LOG_INFO("Error in get ctx: %x, %s\n", ctx, __FUNCTION__);
        exit(-1);
    }

    // ctx->ready_signal = COLLECTED_DATA;

    // { //NEWPLAN is waiting here to synchronize the communication with peer
    //     LOG(INFO) << "sub poll cq thread is waiting for main threads";
    //     std::unique_lock<std::mutex> lck(mtx);
    //     while (!ready)
    //         cv.wait(lck); // 当前线程被阻塞, 当全局标志位变为 true 之后,
    //     LOG(INFO) << "sub poll cq thread has been waked up...";
    // }

    void *ev_ctx = NULL;

    {
        TEST_NZ(ibv_get_cq_event(ctx->comp_channel,
                                 &cq, &ev_ctx));
        ibv_ack_cq_events(cq, 1);
        TEST_NZ(ibv_req_notify_cq(cq, 0));

        while (true)
        {
            int nc = ibv_poll_cq(cq, MAX_DATA_IN_FLIGHT * 2, wcs);
            if (nc < 0)
            {
                LOG(FATAL) << "Error of poll cq: with entries: " << nc;
            }
            if (nc == 0)
                continue;

            for (int index = 0; index < nc; index++)
            {
                if (wcs[index].status == IBV_WC_SUCCESS)
                {
                    LOG_EVERY_N(INFO, 1) << "IBV_WC_SUCCESS, wr id: " << wcs[index].wr_id << ", imm: " << wcs[index].imm_data << ", opcode: " << wcs[index].opcode;
                    if (wcs[index].opcode == IBV_WC_SEND)
                    {
                    }
                    else if (wcs[index].opcode == IBV_WC_RECV_RDMA_WITH_IMM)
                    {
                        //add_performance(wcs[index].imm_data);
                        LOG_EVERY_N(INFO, 1) << "Receive msg: " << wcs[index].imm_data;
                        if (wcs[index].imm_data == 23)
                        {
                            ctx->buffer[23] = 0;
                            printf("Connection info: %s\n", ctx->buffer + 1);
                        }
                        post_receive(id);
                        on_imm_recv(&wcs[index]);
                        // DictXiong: 似乎也不用发 MSG_READY 回去.
                    }
                    else
                    {
                        LOG(INFO) << "Unknown message";
                    }
                }
                else
                {
                    rc_die("poll_cq: status is not IBV_WC_SUCCESS");
                }
            }
        }
    }
    // DictXiong: will come here?
    std::cout << "WOW I'm really here RDMAServer.cpp:342";
    return NULL;
}
void RDMAServer::on_connection(struct rdma_cm_id *id)
{
    struct RDMAContext *ctx = (struct RDMAContext *)id->context;

    ctx->msg[0].id = MSG_MR;

    ctx->msg[0].data.mr.addr = (uintptr_t)ctx->buffer_mr->addr;
    ctx->msg[0].data.mr.rkey = ctx->buffer_mr->rkey;

    send_message(id, 0, IMM_MR);
    //send_imm(id, IMM_MR); // RE, 并且 token_id != 0 就会 RE. 
}

// RDMABase::build_connection 调用了它
void RDMAServer::build_context(struct rdma_cm_id *id)
{
    RDMABase::build_context(id);

    struct RDMAContext *ctx = (struct RDMAContext *)id->context;
    ctx->client_index = num_clients++;
    new std::thread([this, id]() {
        this->poll_cq((void *)id);
    });
    auto que = new BlockingQueue<comm_job>;
    job_queues.push_back(que);
    new std::thread([this, id, que]() {
        this->poll_job_queue(id, que);
    });
}

// 对于每一个 Client, 发送消息.
void RDMAServer::poll_job_queue(struct rdma_cm_id *id, BlockingQueue<comm_job> *que)
{
    while (true)
    {
        auto job = que->pop();
        if (job.type == comm_job::SEND_IMM)
        {
            LOG_EVERY_N(INFO, 1) << "Send " << job.data << " to client " << ((struct RDMAContext *)(id->context))->client_index;
            send_imm(id, job.data);
        }
        else
        {
            rc_die("Unknown job type");
        }
    }
    
}
// DictXiong: 在这里申请了内存
void RDMAServer::on_pre_conn(struct rdma_cm_id *id)
{

    struct RDMAContext *ctx = (struct RDMAContext *)id->context;
    size_t register_buf_size = WINDOWS_NUM * BUFFER_SIZE * MAX_DATA_IN_FLIGHT;
    int ret = 0;

    ret = posix_memalign((void **)&ctx->buffer,
                         sysconf(_SC_PAGESIZE),
                         register_buf_size);
    if (ret)
    {
        fprintf(stderr, "posix_memalign: %s\n",
                strerror(ret));
        exit(-1);
    }
    TEST_Z(ctx->buffer_mr = ibv_reg_mr(rc_get_pd(id), ctx->buffer,
                                       register_buf_size,
                                       IBV_ACCESS_LOCAL_WRITE |
                                           IBV_ACCESS_REMOTE_WRITE));

    ret = posix_memalign((void **)&ctx->msg, sysconf(_SC_PAGESIZE),
                         sizeof(struct message) * MAX_DATA_IN_FLIGHT);
    if (ret)
    {
        fprintf(stderr, "posix_memalign: %s\n",
                strerror(ret));
        exit(-1);
    }
    TEST_Z(ctx->msg_mr = ibv_reg_mr(rc_get_pd(id), ctx->msg,
                                    sizeof(struct message) * MAX_DATA_IN_FLIGHT,
                                    IBV_ACCESS_LOCAL_WRITE));

    for (int msg_index = 0; msg_index < MAX_DATA_IN_FLIGHT; msg_index++)
    {
        post_receive(id);
    }
    // LOG_INFO("Server register buffer info:\nblock size: %d;\nblock number: %d;\nwindow num: %u;\nbase address: %p\n",
    //          BUFFER_SIZE, MAX_DATA_IN_FLIGHT, WINDOWS_NUM, ctx->buffer);
}

void RDMAServer::on_disconnect(struct rdma_cm_id *id)
{
    struct RDMAContext *new_ctx = (struct RDMAContext *)id->context;

    ibv_dereg_mr(new_ctx->buffer_mr);
    free(new_ctx->buffer);

    ibv_dereg_mr(new_ctx->msg_mr);
    free(new_ctx->msg);

    printf("Disconnection: Say goodbye with %s\n",
           new_ctx->connection_id);
    free(new_ctx);
    id->context = 0;
}

void RDMAServer::post_receive(struct rdma_cm_id *id)
{
    struct ibv_recv_wr wr, *bad_wr = NULL;

    memset(&wr, 0, sizeof(wr));

    wr.wr_id = (uintptr_t)id;
    wr.sg_list = NULL;
    wr.num_sge = 0;

    TEST_NZ(ibv_post_recv(id->qp, &wr, &bad_wr));
}

void RDMAServer::send_message(struct rdma_cm_id *id, uint32_t token_id, uint32_t imm_data)
{
    struct RDMAContext *ctx = (struct RDMAContext *)id->context;
    struct ibv_send_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    memset(&wr, 0, sizeof(wr));

    wr.wr_id = token_id; // for debugging.
    wr.opcode = IBV_WR_SEND_WITH_IMM;
    wr.imm_data = imm_data;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.send_flags = IBV_SEND_SIGNALED;
    //wr.send_flags = IBV_SEND_INLINE;

    sge.addr = (uintptr_t)&ctx->msg[token_id];
    sge.length = sizeof(struct message);
    sge.lkey = ctx->msg_mr->lkey;

    TEST_NZ(ibv_post_send(id->qp, &wr, &bad_wr));

    // It may failed, resulted from cannot allocate memory.
    // In most case, it is because of the send queue is full,
    // therefore, you cannot put a send event into the sq.
    // If the cq is all from recv event, these no space
    // to hold another send work request when it completed.
    // To this end, we should poll_cq immediately.
    // refer to: https://zhuanlan.zhihu.com/p/101250614
}

void RDMAServer::send_imm(struct rdma_cm_id *id, uint32_t imm_data)
{
    struct RDMAContext *ctx = (struct RDMAContext *)id->context;
    struct ibv_send_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    memset(&wr, 0, sizeof(wr));

    wr.wr_id = WR_SEND_ONLY_IMM; 
    wr.opcode = IBV_WR_SEND_WITH_IMM;
    wr.imm_data = imm_data;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.send_flags = IBV_SEND_SIGNALED;

    sge.addr = (uintptr_t)&ctx->msg[1];
    sge.length = sizeof(struct message);
    sge.lkey = ctx->msg_mr->lkey;

    TEST_NZ(ibv_post_send(id->qp, &wr, &bad_wr));
}

void RDMAServer::process_message(struct RDMAContext *ctx, uint32_t token,
                                 uint8_t *buf, uint32_t len)
{
    std::cout << "似乎正常情况下也不会到这里 RDMAServer::process_message";
    if (token != buf[len - 1] || token != buf[0]) //check data
    {
        LOG_INFO("Unknown error: Recv buffer id: %u, size: %u, data:[%d - %d]\n",
                 token, len, buf[0], buf[len - 1]);
        exit(-1);
    }
    { //process data here
        if (ctx->connection_id[0] == 0)
        { //get peer info.
            memcpy(ctx->connection_id, buf + 1, len - 1);
            ctx->connection_id[len - 2] = 0;
            LOG_INFO("Get peer address info\n");
        }
        //count recv data
        ctx->recv_bytes += len;
        if (ctx->recv_bytes > 10000000000)
        {
            struct timeval now;
            gettimeofday(&now, NULL);
            float time_cost = (now.tv_usec - ctx->start.tv_usec) / 1000000.0 + now.tv_sec - ctx->start.tv_sec;
            ctx->start = now;

            printf("[%s] Recv rate: %.2f Gbps\n",
                   ctx->connection_id,
                   8.0 * ctx->recv_bytes / 1000.0 / 1000.0 / 1000.0 / time_cost);

            ctx->recv_bytes = 0;
        }
        {
            this->add_performance(len);
        }
    }
    //reset buffer.
    buf[0] = buf[len - 1] = 0;
}

// DictXiong: 应该不会调用这个
void RDMAServer::on_completion(struct ibv_wc *wc)
{
    struct rdma_cm_id *id = (struct rdma_cm_id *)(uintptr_t)wc->wr_id;
    struct RDMAContext *ctx = (struct RDMAContext *)id->context;

    if (wc->opcode == IBV_WC_RECV_RDMA_WITH_IMM)
    {
        uint32_t imm_data_recv = ntohl(wc->imm_data);
        uint32_t size = imm_data_recv & 0XFFFFFF; // DictXiong: err... isn't 0xFFFF?
        uint32_t buffer_id = (imm_data_recv >> 24) & 0xFF;
        //uint32_t window_id = (imm_data_recv >> 16) & 0xFF;

        if (buffer_id != ctx->buffer[buffer_id * BUFFER_SIZE] ||
            buffer_id != ctx->buffer[buffer_id * BUFFER_SIZE + size - 1]) // DictXiong: ??? why not ctx->buffer[window_id * WINDOW_SIZE + buffer_id * BUFFER_SIZE + ...]
        {
            printf("Unknown error: Recv buffer id: %u, size: %u, data:[%d - %d]\n",
                   buffer_id, size, ctx->buffer[buffer_id * BUFFER_SIZE],
                   ctx->buffer[buffer_id * BUFFER_SIZE + size - 1]);
            exit(-1);
        }

        if (size == 0)
        {
            ctx->msg[buffer_id].id = MSG_DONE;
            send_message(id, buffer_id);
        }
        else
        {
            if (ctx->connection_id[0] == 0)
            {
                memcpy(ctx->connection_id, ctx->buffer + 1, size);
                ctx->connection_id[size - 2] = 0;
            }

            ctx->recv_bytes += size;
            if (ctx->recv_bytes > 10000000000)
            {
                struct timeval now;
                gettimeofday(&now, NULL);
                float time_cost = (now.tv_usec - ctx->start.tv_usec) / 1000000.0 + now.tv_sec - ctx->start.tv_sec;
                ctx->start = now;

                printf("[%s] Recv rate: %.2f Gbps\n",
                       ctx->connection_id,
                       8.0 * ctx->recv_bytes / 1000.0 / 1000.0 / 1000.0 / time_cost);

                ctx->recv_bytes = 0;
            }

            {
                this->add_performance(size);
            }
            int base_addr = buffer_id * BUFFER_SIZE;
            ctx->buffer[base_addr] = ctx->buffer[base_addr + size - 1] = 0;
            post_receive(id);

            ctx->msg[buffer_id].id = MSG_READY;
            ctx->msg[buffer_id].data.batch_index[0] = buffer_id;
            {
                ctx->msg[buffer_id].data.batch_index[MSG_NUM_OFFEST] = 1;
            }
            send_message(id, buffer_id);
        }
    }
}

void RDMAServer::broadcast_imm(uint32_t imm)
{
    LOG_EVERY_N(INFO, 1) << "Broadcast " << imm << " to " << job_queues.size() <<" clients";
    for (auto i : job_queues)
    {
        i->push(imm);
    }
}