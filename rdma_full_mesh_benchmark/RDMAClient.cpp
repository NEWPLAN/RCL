#include "RDMABase.h"
#include "RDMAClient.h"

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

#include <iostream>
#include <iomanip>

#include <thread>
#include <fcntl.h>

RDMAClient::RDMAClient(RDMAAdapter &rdma_adapter)
{
    this->rdma_adapter_ = rdma_adapter;
    std::cout << "Creating RDMAClient" << std::endl;
}
RDMAClient::RDMAClient(const std::string &server_ip, const std::string &client_ip, BlockingQueue<int> *q)
{
    this->rdma_adapter_.set_server_ip(server_ip.c_str());
    this->rdma_adapter_.set_client_ip(client_ip.c_str());
    this->job_queue = q;
    std::cout << "Creating RDMAClient" << std::endl;
}
RDMAClient::~RDMAClient()
{
    std::cout << "Destroying RDMAClient" << std::endl;
}

void RDMAClient::setup()
{
    std::cout << "Configure RDMAClient" << std::endl;
    this->_init();
}
void RDMAClient::start_service()
{
    std::cout << "RDMAClient starts service" << std::endl;
}

void RDMAClient::_send_loops()
{
    std::cout << "In RDMAClient send loops" << std::endl;
}

void RDMAClient::_init()
{
    std::cout << "Initing RDMAClient" << std::endl;

    struct rdma_cm_id *conn = NULL;
    struct rdma_event_channel *ec = NULL;

    std::string local_eth = this->rdma_adapter_.client_ip; /*get each lev ip*/

    memset(&ser_in, 0, sizeof(ser_in));
    memset(&local_in, 0, sizeof(local_in));

    /*bind remote socket*/
    ser_in.sin_family = AF_INET;
    ser_in.sin_port = htons(this->rdma_adapter_.server_port); /*connect to public port remote*/
    inet_pton(AF_INET,
              this->rdma_adapter_.server_ip.c_str(),
              &ser_in.sin_addr);

    /*bind local part*/
    local_in.sin_family = AF_INET;

    std::cout << local_eth.c_str() << "----->" << this->rdma_adapter_.server_ip.c_str() << std::endl;
    inet_pton(AF_INET, local_eth.c_str(), &local_in.sin_addr);

    TEST_Z(ec = rdma_create_event_channel());
    TEST_NZ(rdma_create_id(ec, &conn,
                           NULL, RDMA_PS_TCP));
    TEST_NZ(rdma_resolve_addr(conn,
                              (struct sockaddr *)(&local_in),
                              (struct sockaddr *)(&ser_in),
                              TIMEOUT_IN_MS));

    conn->context = (void *)this->ctx;

    this->event_loop(ec);
    // DictXiong: 应该不会运行到这儿了? 
    std::cout << "RDMAClient is launched" << std::endl;
    while (1)
    {
        std::cout << "main thread sleep for 10 seconds" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    rdma_destroy_event_channel(ec);
}

void RDMAClient::event_loop(struct rdma_event_channel *ec)
{
    struct rdma_cm_event *event = NULL;
    struct rdma_conn_param cm_params;

    build_params(&cm_params);

    int connect_count = 0;
    struct rdma_cm_id *conn = NULL;
    int connect_timeout = 300;

    while (rdma_get_cm_event(ec, &event) == 0)
    {
        struct rdma_cm_event event_copy;
        memcpy(&event_copy, event, sizeof(*event));
        rdma_ack_cm_event(event);

        switch (event_copy.event)
        {
        case RDMA_CM_EVENT_ADDR_RESOLVED:
        {
            // NEWPLAN: traffic class configuration
            //LOG_INFO("In %s\n", "RDMA_CM_EVENT_ADDR_RESOLVED");
            build_connection(event_copy.id);
            uint8_t tos = 0; //https://blog.csdn.net/sunshuying1010/article/details/103661289
            if (rdma_set_option(event_copy.id, RDMA_OPTION_ID,
                                RDMA_OPTION_ID_TOS,
                                &tos, sizeof(uint8_t)))
            {
                std::cout << "Failed to set ToS(Type of Service) option for RDMA CM connection." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(2));
                exit(0);
            }

            TEST_NZ(rdma_resolve_route(event_copy.id,
                                       TIMEOUT_IN_MS));
            break;
        }
        case RDMA_CM_EVENT_ROUTE_RESOLVED:
        {
            //LOG_INFO("In %s\n", "RDMA_CM_EVENT_ROUTE_RESOLVED");
            TEST_NZ(rdma_connect(event_copy.id, &cm_params));
            break;
        }
        case RDMA_CM_EVENT_CONNECT_REQUEST:
        {
            printf("Error: client never request a connection\n");
            exit(-1);
            break;
        }
        case RDMA_CM_EVENT_ESTABLISHED:
        {
            //LOG_INFO("In %s\n", "RDMA_CM_EVENT_ESTABLISHED");
            on_pre_conn(event_copy.id);
            this->send_thread = new std::thread([this, event_copy]() {
                this->poll_cq((void *)(event_copy.id));
            });

            struct sockaddr *peer_addr = rdma_get_peer_addr(event_copy.id);
            struct sockaddr *local_addr = rdma_get_local_addr(event_copy.id);
            struct sockaddr_in *server_addr = (struct sockaddr_in *)peer_addr;
            struct sockaddr_in *client_addr = (struct sockaddr_in *)local_addr;

            printf("[%s:%d] has connected to [%s:%d]\n",
                   inet_ntoa(client_addr->sin_addr),
                   ntohs(client_addr->sin_port),
                   inet_ntoa(server_addr->sin_addr),
                   ntohs(server_addr->sin_port));

            //  exit if connected to remote;
            //break;
            break;
        }
        case RDMA_CM_EVENT_REJECTED: // DictXiong: 什么时候会到这儿来?
        {
            //LOG_INFO("In %s\n", "RDMA_CM_EVENT_REJECTED");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            connect_count++;
            struct RDMAContext *ctx_ = (struct RDMAContext *)(event_copy.id->context);
            rdma_destroy_qp(event_copy.id);
            rdma_destroy_id(event_copy.id);
            rdma_destroy_event_channel(ec);

            if (connect_count > 10 * connect_timeout) //after 300 seconds, it will exit.
            {
                std::cerr << "Fail to connect to server " << this->rdma_adapter_.server_ip
                          << " in pasted " << connect_timeout
                          << " seconds, check your network system carefully." << std::endl;
                exit(-1);
            }
            else
            {
                if (connect_count % 10 == 0)
                    std::cout << "[" << std::setw(3) << connect_timeout - connect_count / 10
                              << "] Fail to connect to server [" << this->rdma_adapter_.server_ip
                              << ":" << this->rdma_adapter_.server_port
                              << "], make sure it is launched." << std::endl;

                TEST_Z(ec = rdma_create_event_channel());
                TEST_NZ(rdma_create_id(ec, &conn, NULL, RDMA_PS_TCP));
                TEST_NZ(rdma_resolve_addr(conn, (struct sockaddr *)(&local_in),
                                          (struct sockaddr *)(&ser_in), TIMEOUT_IN_MS));

                conn->context = (void *)ctx_;
            }
            break;
        }
        case RDMA_CM_EVENT_DISCONNECTED:
        {
            LOG_INFO("In %s\n", "RDMA_CM_EVENT_DISCONNECTED");
            rdma_destroy_qp(event_copy.id);
            on_disconnect(event_copy.id);
            rdma_destroy_id(event_copy.id);
            exit(-1);
        }
        default:
            break;
        }
    }
}

void *RDMAClient::poll_cq(void *_id)
{
    struct ibv_cq *cq = NULL;
    //struct ibv_wc wc;
    struct ibv_wc wcs[MAX_DATA_IN_FLIGHT * 2];

    struct rdma_cm_id *id = (rdma_cm_id *)_id;

    struct RDMAContext *ctx = (struct RDMAContext *)id->context;
    //struct RDMAContext *ctx = s_ctx;

    void *ev_ctx = NULL;

    TEST_NZ(ibv_get_cq_event(ctx->comp_channel, &cq, &ev_ctx));
    ibv_ack_cq_events(cq, 1);
    TEST_NZ(ibv_req_notify_cq(cq, 0));

    while (true)
    {
        // DictXiong: 现在理论上每次只有一个 cqe.
        int ne = ibv_poll_cq(cq, MAX_DATA_IN_FLIGHT * 2, wcs);
        if (ne < 0)
        {
            LOG(FATAL) << "Fail to poll CQ with entries: " << ne;
        }
        else if (ne == 0)
        {
            continue;
        }
        else
        {
            for (int index = 0; index < ne; index++)
            {
                if (wcs[index].status == IBV_WC_SUCCESS)
                {
                    std::cout << "WC with imm = " << wcs[index].imm_data << " and opcode "
                        << wcs[index].opcode << "\n";
                    if (wcs[index].opcode == IBV_WC_RDMA_WRITE)
                    { //判断write请求完成
                        LOG_EVERY_N(INFO, 100000) << "IBV_WC_RDMA_WRITE, wr id: " << wcs[index].wr_id;
                        std::cout << "Client write " << wcs[index].imm_data << " finished\n";
                        uint32_t len = job_queue->pop();
                        write_large_block(len);
                    }
                    // DictXiong: 接收到来自服务端的消息
                    else if (wcs[index].opcode == IBV_WC_RECV_RDMA_WITH_IMM)
                    {
                        post_receive(1);
                        LOG_EVERY_N(INFO, 100000) << "IBV_WC_RECV_RDMA_WITH_IMM, wr id: " << wcs[index].wr_id;
                    }
                    else if (wcs[index].opcode == IBV_WC_RECV)
                    {
                        LOG_EVERY_N(INFO, 100000) << "IBV_WC_RECV, wr id: " << wcs[index].wr_id
                                                  << " imm_data: " << wcs[index].imm_data;
                        post_receive(1);
                        // 似乎即使带有立即数也会到 IBV_WC_RECV 条件分支中... 咱也不知道为啥
                        // DictXiong: 这...666似乎是某些初始化? 魔数杀我
                        if (wcs[index].imm_data == 666) 
                        {
                            uint32_t msg_id = wcs[index].wr_id;
                            LOG(INFO) << "Sending to peer the local information";
                            ctx->peer_addr = ctx->msg[msg_id].data.mr.addr;
                            ctx->peer_rkey = ctx->msg[msg_id].data.mr.rkey;

                            printf("Server is ready to send\n");
                            send_file_name(id);
                        }
                        /* DictXiong: we won't response to this any more. WE decide when to send.
                        else if (wcs[index].imm_data == 888)
                        {
                            LOG_EVERY_N(INFO, 10000) << "Write large buffer to peer";
                            //1MB
                            write_large_block(1024 * 1000);
                        }
                        */
                    }

                    //on_completion(&wcs[index]);
                }
                else
                {
                    rc_die("poll_cq: status is not IBV_WC_SUCCESS");
                }
            }
        }
    }
    // DictXiong: will come here?
    std::cout << "Wow! I'm realy here RDMAClient.cpp:303";
    while (1)
    {
        TEST_NZ(ibv_get_cq_event(ctx->comp_channel, &cq, &ev_ctx));
        ibv_ack_cq_events(cq, 1);
        TEST_NZ(ibv_req_notify_cq(cq, 0));
        int nc = 0;

        do
        {
            nc = ibv_poll_cq(cq, MAX_DATA_IN_FLIGHT * 2, wcs);
            for (int index = 0; index < nc; index++)
            {
                if (wcs[index].status == IBV_WC_SUCCESS)
                {
                    if (wcs[index].opcode == IBV_WC_RDMA_WRITE)
                    { //判断write请求完成
                        //printf("IBV_WC_RDMA_WRITE, wr id: %lu\n", wcs[index].wr_id);
                    }

                    on_completion(&wcs[index]);
                }
                else
                {
                    rc_die("poll_cq: status is not IBV_WC_SUCCESS");
                }
            }

        } while (nc > 0);
    }
    return NULL;
}

void RDMAClient::on_pre_conn(struct rdma_cm_id *id)
{
    ctx = (struct RDMAContext *)id->context;
    size_t register_buf_size = WINDOWS_NUM * BUFFER_SIZE * MAX_DATA_IN_FLIGHT;
    int ret = 0;

    ctx->id = id;

    ret = posix_memalign((void **)&ctx->buffer,
                         sysconf(_SC_PAGESIZE),
                         register_buf_size);
    if (ret)
    {
        fprintf(stderr, "posix_memalign: %s\n",
                strerror(ret));
        exit(-1);
    }
    TEST_Z(ctx->buffer_mr = ibv_reg_mr(rc_get_pd(id),
                                       ctx->buffer,
                                       register_buf_size,
                                       0));

    ret = posix_memalign((void **)&ctx->msg,
                         sysconf(_SC_PAGESIZE),
                         sizeof(struct message) * MAX_DATA_IN_FLIGHT);
    if (ret)
    {
        fprintf(stderr, "posix_memalign: %s\n",
                strerror(ret));
        exit(-1);
    }
    TEST_Z(ctx->msg_mr = ibv_reg_mr(rc_get_pd(id),
                                    ctx->msg,
                                    sizeof(struct message) * MAX_DATA_IN_FLIGHT,
                                    IBV_ACCESS_LOCAL_WRITE));

    for (int msg_index = 0; msg_index < MAX_DATA_IN_FLIGHT; msg_index++)
    {
        //post to recv queue and wait for data
        post_receive(msg_index);
    }

    // LOG_INFO("Client register buffer:\nblock size: %d;\nblock number: %d;\nwindows num: %u;\nbase address: %p\n",
    //          BUFFER_SIZE, MAX_DATA_IN_FLIGHT, WINDOWS_NUM, ctx->buffer);
}

void RDMAClient::on_disconnect(struct rdma_cm_id *id)
{
    struct RDMAContext *ctx = (struct RDMAContext *)id->context;
    LOG_INFO("Client is disconnecting now\n");

    ibv_dereg_mr(ctx->buffer_mr);
    free(ctx->buffer);
    ibv_dereg_mr(ctx->msg_mr);
    free(ctx->msg);

    id->context = 0;
    free(ctx);
}

void RDMAClient::post_receive(uint32_t msg_id)
{
    struct rdma_cm_id *id = ctx->id;
    struct ibv_recv_wr wr,
        *bad_wr = NULL;
    struct ibv_sge sge;

    memset(&wr, 0, sizeof(wr));

    wr.wr_id = msg_id;
    wr.sg_list = &sge;
    wr.num_sge = 1;

    sge.addr = (uintptr_t)&ctx->msg[msg_id];
    sge.length = sizeof(struct message);
    sge.lkey = ctx->msg_mr->lkey;

    TEST_NZ(ibv_post_recv(id->qp, &wr, &bad_wr));
}

void RDMAClient::write_large_block(uint32_t len)
{
    std::cout << "Client write " << len << " to remote\n";
    struct rdma_cm_id *id = ctx->id;

    struct ibv_send_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;
    memset(&wr, 0, sizeof(wr));

    // wr.wr_id = buffer_id; //before
    wr.wr_id = 12345;
    wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.imm_data = len;

    if (len >= WINDOWS_NUM * BUFFER_SIZE * MAX_DATA_IN_FLIGHT)
    {
        LOG(FATAL) << "Write buffer size too much: " << len;
    }

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

void RDMAClient::write_remote(uint32_t buffer_id,
                              uint32_t window_id,
                              uint32_t len)
{
    struct rdma_cm_id *id = ctx->id;

    struct ibv_send_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    memset(&wr, 0, sizeof(wr));

    uint32_t imm_data_prepared = buffer_id << 24;
    imm_data_prepared &= 0XFF000000;
    imm_data_prepared += len;
    imm_data_prepared += ((window_id << 16) & 0X00FF0000);

#ifdef DEBUG_REVERSE_ORDER
    LOG_INFO("RDMAWrite: buffer_id:%u, window_id:%u, data size: %u\n",
             buffer_id, window_id, len);
#endif

    // wr.wr_id = buffer_id; //before
    wr.wr_id = 12345;
    wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.imm_data = htonl(imm_data_prepared);

    int base_addr = buffer_id * BUFFER_SIZE + window_id * BUFFER_SIZE * MAX_DATA_IN_FLIGHT;

    wr.wr.rdma.remote_addr = ctx->peer_addr + base_addr;
    wr.wr.rdma.rkey = ctx->peer_rkey;

    if (len)
    {
        wr.sg_list = &sge;
        wr.num_sge = 1;

        sge.addr = (uintptr_t)ctx->buffer + base_addr;
        sge.length = len;
        sge.lkey = ctx->buffer_mr->lkey;
    }

    TEST_NZ(ibv_post_send(id->qp, &wr, &bad_wr));
}

void RDMAClient::send_next_chunk(uint32_t buffer_id, uint32_t window_id)
{
    ssize_t size = 0;
    size = BUFFER_SIZE;

    if (size == -1)
        rc_die("read() failed\n");

    std::cout << "F*ck. Who called me?";
    //write_remote(buffer_id, window_id, size);
}

void RDMAClient::send_file_name(struct rdma_cm_id *id)
{
    struct RDMAContext *ctx = (struct RDMAContext *)id->context;
    this->ip_addr_ = rdma_adapter_.get_client_ip();
    sprintf((char *)ctx->buffer, "0%s0", this->ip_addr_.c_str());
    printf("Sending file name: %s\n", this->ip_addr_.c_str());
    ctx->buffer[0] = 0;
    ctx->buffer[this->ip_addr_.length() + 1] = 0;
    // DictXiong: 23 似乎是个魔数
    write_large_block(23);
}
// DictXiong: 似乎也不会被调用? 
void RDMAClient::on_completion(struct ibv_wc *wc)
{
    struct rdma_cm_id *id = ctx->id;
    uint32_t msg_id = wc->wr_id;

    uint32_t window_id = ctx->window_id % WINDOWS_NUM;
    ctx->window_id = window_id;

    if (wc->opcode & IBV_WC_RECV)
    { //收到对方的ACK回复确认。
        if (ctx->msg[msg_id].id == MSG_MR)
        {
            ctx->peer_addr = ctx->msg[msg_id].data.mr.addr;
            ctx->peer_rkey = ctx->msg[msg_id].data.mr.rkey;

            printf("Received MR, ready to send\n");
            send_file_name(id);
            /* DictXiong: no. WE decide when to send.
            for (int index = 1; index < MAX_DATA_IN_FLIGHT; index++)
            {
                //uint32_t window_id = index;
                int base_addr = index * BUFFER_SIZE + window_id * BUFFER_SIZE * MAX_DATA_IN_FLIGHT;
                ctx->buffer[base_addr + 0] = index;
                ctx->buffer[base_addr + BUFFER_SIZE - 1] = index;
                send_next_chunk(index, window_id);
            }
            */
            //ctx->window_id++;
        }
        else if (ctx->msg[msg_id].id == MSG_READY)
        {
            //batch_index:
            // |   0-47  |    48   |    49   |
            // |buffer_id|window_id|num_token|

            /* DictXiong: now WE decide when to send.
            uint32_t num_token = ctx->msg[msg_id].data.batch_index[MSG_NUM_OFFEST];
            for (uint32_t token_id = 0; token_id < num_token; token_id++)
            {
                int buffer_id = ctx->msg[msg_id].data.batch_index[token_id];

                int base_addr = buffer_id * BUFFER_SIZE + window_id * BUFFER_SIZE * MAX_DATA_IN_FLIGHT;
                ctx->buffer[base_addr + 0] = buffer_id;
                ctx->buffer[base_addr + BUFFER_SIZE - 1] = buffer_id;
                send_next_chunk(buffer_id, window_id);

                if (buffer_id == MAX_DATA_IN_FLIGHT - 1)
                { //slice to the next window
                    ctx->window_id = (ctx->window_id + 1) % WINDOWS_NUM;
                    window_id = ctx->window_id;
                }
            }
            */
        }
        else if (ctx->msg[msg_id].id == MSG_DONE)
        {
            LOG_INFO("received DONE, disconnecting\n");
            rdma_disconnect(id);
            return;
        }

        post_receive(msg_id);
    }
}