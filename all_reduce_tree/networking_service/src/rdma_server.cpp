
#include <thread>
#include <chrono>
#include <glog/logging.h>
#include "rdma_server.h"

#include "ip_qos_helper.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/ip.h>

#include "pre_connector.h"
#include "rdma_adapter.h"
#include "config.h"
#include "rdma_session.h"
#include <queue>

void RDMAServer::run()
{
    do
    {
        LOG(INFO) << "Running in Server";
        std::this_thread::sleep_for(std::chrono::seconds(5));

    } while (true);
}

void RDMAServer::pre_connect()
{
    LOG(INFO) << "[On] Preparing socket...";
    if (this->root_sock == 0)
    {
        this->root_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (this->root_sock <= 0)
            LOG(FATAL) << "Error of creating socket...";
        LOG(INFO) << "Creating root socket: " << this->root_sock;
    }
    newplan::IPQoSHelper::set_and_check_ip_tos(this->root_sock, 0x10);
    newplan::IPQoSHelper::set_and_check_ip_priority(this->root_sock, 4);
    newplan::IPQoSHelper::set_socket_reuse(this->root_sock);
    LOG(INFO) << "[Done] Preparing socket...";

    LOG(INFO) << "[On] Checking config...";
    this->config_check();
    LOG(INFO) << "[Done] Checking config...";
}
void RDMAServer::connecting()
{

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr("0.0.0.0");
    sin.sin_port = htons(this->conf_.tcp_port);

    if (bind(root_sock, (struct sockaddr *)&sin,
             sizeof(sin)) < 0)

        LOG(FATAL) << "Error when binding to socket";

    if (listen(root_sock, 1024) < 0)
        LOG(FATAL) << "Error when listen on socket";

    LOG(INFO) << "The server is waiting for connections";
    int connected = 0;

    do //loop wait and build new rdma connections
    {
        struct sockaddr_in cin;
        socklen_t len = sizeof(cin);
        int client_fd;

        if ((client_fd = accept(this->root_sock,
                                (struct sockaddr *)&cin,
                                &len)) == -1)
            LOG(FATAL) << "Error of accepting new connection";

        handle_with_new_connect(client_fd, cin);
        connected++;
        if (conf_.single_recv && connected == conf_.num_senders)
        {
            LOG(INFO) << "Happy to see all connections have been established: " << connected;
            break;
        }

    } while (true);
    is_connected = true;
}

int RDMAServer::do_reduce(Group &group)
{
    CHECK(group.size() == 2) << "the group is broken";
    std::queue<RDMASession *> *inflight_queue = group[0];
    std::queue<RDMASession *> *ready_queue = group[1];

    struct ibv_wc wc;
    while (!inflight_queue->empty())
    {
        RDMASession *sess = inflight_queue->front();
        inflight_queue->pop();
        int session_ready = false;
        if (sess->peek_status(&wc))
        {
            switch (wc.opcode)
            {
            case IBV_WC_SEND:
            {
                LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[out] request: send";
                break;
            }
            case IBV_WC_RECV:
            {
                LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[in ] request: Recv";
                break;
            }
            case IBV_WC_RDMA_WRITE:
            {
                LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[out] request: write";
                break;
            }
            case IBV_WC_RECV_RDMA_WITH_IMM:
            {
                LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[in ] request: write_with_IMM, " << wc.imm_data;
                session_ready = true;
                sess->pull_ctrl(1);
                //push_ctrl(index);
                ready_queue->push(sess);
                break;
            }
            default:
                LOG_EVERY_N(INFO, 1) << "Unknown opcode" << wc.opcode;
            }
        }
        if (!session_ready)
            inflight_queue->push(sess);
    }

    LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[in] reduced";
    return 0;
}
int RDMAServer::broadcast(Group &group, bool data_channel)
{
    CHECK(group.size() == 2) << "the group is broken";
    std::queue<RDMASession *> *inflight_queue = group[0];
    std::queue<RDMASession *> *ready_queue = group[1];

    while (!ready_queue->empty())
    {
        auto each_sess = ready_queue->front();
        if (data_channel)
            each_sess->push_data(0, conf_.DATA_MSG_SIZE / 20);
        else
            each_sess->push_ctrl(0);
        inflight_queue->push(each_sess);
        ready_queue->pop();
    }
    LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[out] broadcasted";
    return 0;
}

void RDMAServer::post_connect()
{
    if (!is_connected)
        LOG(FATAL) << "failed to connect ...";
    switch (conf_.all_reduce)
    {
    case FULL_MESH_ALLREDUCE:
        full_mesh_allreduce();
        break;
    case TREE_ALLREDUCE:
        tree_allreduce();
        break;
    case RING_ALLREDUCE:
        ring_allreduce();
        break;
    case DOUBLE_BINARY_TREE_ALLREDUCE:
        double_binary_tree_allreduce();
        break;
    default:
        LOG(FATAL) << "UNKNOWN allreduce, you should speficy the topology with flag: --topo";
    }
    // below is not used anymore
}

void RDMAServer::handle_with_new_connect(int sock,
                                         struct sockaddr_in cin)
{
    std::string client_ip = std::string(inet_ntoa(cin.sin_addr));
    int client_port = htons(cin.sin_port);
    LOG(INFO) << "receive a connecting request from "
              << client_ip << ":" << client_port;
    if (conf_.single_recv)
    {
        RDMASession *server_sess = RDMASession::new_rdma_session(sock,
                                                                 client_ip,
                                                                 client_port,
                                                                 conf_);
        sess_group.push_back(server_sess);
    }
    else
    { // move to the tread queue;
        auto connection_thread = new std::thread([=]() {
            RDMASession *server_sess = RDMASession::new_rdma_session(sock,
                                                                     client_ip,
                                                                     client_port,
                                                                     conf_);
            server_sess->session_init();
            server_sess->run_tests_recv_side();
            LOG(FATAL) << "Error of in the server side";
            if (0)
            { // for debug, would never use it again.

                NPRDMAPreConnector *pre_con = new NPRDMAPreConnector(sock,
                                                                     client_ip,
                                                                     client_port);
                {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    NPRDMAdapter *adpter = new NPRDMAdapter(this->conf_);
                    adpter->resources_init(pre_con);
                    adpter->resources_create();
                    adpter->connect_qp();
                    LOG(INFO) << "Has connected with client: " << pre_con->get_peer_ip();
                    std::this_thread::sleep_for(std::chrono::seconds(1));
#ifndef DEBUG_TWO_CHANNEL
                    server_test(adpter);
#else
                    two_channel_test(adpter);
#endif
                }

                char local[128] = {0};
                char remote[128] = {0};
                int index = 0;
                do
                {
                    memset(local, 0, 128);
                    memset(remote, 0, 128);
                    sprintf(local, "[%d] Hello from server.", index++);
                    pre_con->sock_sync_data(48, local, remote);
                    LOG(INFO) << "exchange with: ["
                              << client_ip << ":" << client_port << "]" << remote;
                    std::this_thread::sleep_for(std::chrono::seconds(1));

                } while (true);
            }
        });
        service_threads.push_back(connection_thread);
    }
}

void RDMAServer::two_channel_test(NPRDMAdapter *adpter)
{
    LOG(INFO) << " [debug] two channel ";
    struct resources *res = adpter->get_context();

    char temp_char;
    NPRDMAPreConnector *pre_con = adpter->get_pre_connector();
    int index = 0;
    if (pre_con->sock_sync_data(1, (char *)"O", &temp_char))
        LOG(FATAL) << "sync error before RDMA ops";
    adpter->post_ctrl_recv(index);
    if (pre_con->sock_sync_data(1, (char *)"O", &temp_char))
        LOG(FATAL) << "sync error After RDMA ops";
    LOG(INFO) << "Prepared everything for benchmark\n\n\n";
    struct ibv_wc wc;
    do
    {
        // Wait for completion events.
        // If we use busy polling, this step is skipped.
        if (conf_.use_event)
        {
            struct ibv_cq *ev_cq;
            void *ev_ctx;
            LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[pre] get CQ event";

            if (ibv_get_cq_event(res->channel, &ev_cq, &ev_ctx))
                LOG(FATAL) << "Fail to get cq_event";
            if (ev_cq != res->cq)
                LOG(FATAL) << "CQ event for unknown CQ " << ev_cq;
            ibv_ack_cq_events(res->cq, 1);
            if (ibv_req_notify_cq(res->cq, 0))
                LOG(FATAL) << "Cannot request CQ notification";
            LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[post] get CQ event";
        }
        //adapter->post_data_write_with_imm(index, 0xff00 + index);
        if (adpter->poll_completion(wc))
        {
            LOG(WARNING) << "poll completion failed\n\n\n\n\n\n\n\n";
            continue;
        }
        switch (wc.opcode)
        {
        case IBV_WC_SEND:
        {
            LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[out] request: send";
            break;
        }
        case IBV_WC_RECV:
        {
            LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[in ] request: Recv";
            break;
        }
        case IBV_WC_RDMA_WRITE:
        {
            LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[out] request: write";
            break;
        }
        case IBV_WC_RECV_RDMA_WITH_IMM:
        {
            LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "[in ] request: write_with_IMM, "
                                                 << wc.imm_data;
            adpter->post_ctrl_recv(1);
            adpter->post_ctrl_send(index);
            break;
        }
        default:
            LOG_EVERY_N(INFO, 1) << "Unknown opcode" << wc.opcode;
        }

    } while (true);
}

void RDMAServer::server_test(NPRDMAdapter *adpter)
{
    struct resources *res = adpter->get_context();
    char temp_char;
    NPRDMAPreConnector *pre_con = adpter->get_pre_connector();

    /* let the server post the sr */
    if (adpter->post_send(IBV_WR_SEND))
        LOG(FATAL) << "failed to post sr";

    /* in both sides we expect to get a completion */
    if (adpter->poll_completion())
        LOG(FATAL) << "poll completion failed";

    /* after polling the completion we have the message in the client buffer too */
    /* setup server buffer with read message */
    strcpy(res->buf, RDMAMSGR);

    /* Sync so we are sure server side has data ready before client tries to read it */
    if (pre_con->sock_sync_data(1, (char *)"R", &temp_char))
        LOG(FATAL) << "sync error before RDMA ops";

    // /* Sync so server will know that client is done mucking with its memory */
    if (pre_con->sock_sync_data(1, (char *)"W", &temp_char))
        LOG(FATAL) << "sync error after RDMA ops";

    LOG(INFO) << "[in] server was written: " << res->buf;

    if (adpter->resources_destroy())
        LOG(FATAL) << "failed to destroy resources";
}

void RDMAServer::config_check()
{
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (this->conf_.tcp_port == 0)
    {
        LOG(FATAL) << "Illegal server listening port: "
                   << this->conf_.tcp_port;
    }
    LOG(INFO) << "Server is listen on: 0.0.0.0: "
              << this->conf_.tcp_port;

    std::this_thread::sleep_for(std::chrono::seconds(1));
}

int RDMAServer::tree_allreduce()
{
#define INFLIGHT_QUEUE 0
#define READY_QUEUE 1

    LOG(INFO) << "In the tree allreduce";
    std::vector<Group> session_sub_groups;
    session_sub_groups.resize(conf_.sub_groups.size());
    for (size_t index = 0; index < conf_.sub_groups.size(); index++)
    {
        std::queue<RDMASession *> *ready_queue = new std::queue<RDMASession *>();
        std::queue<RDMASession *> *inflight_queue = new std::queue<RDMASession *>();
        session_sub_groups[index] = {inflight_queue, ready_queue};
    }

    char buf[102400] = {0}, *data_ptr = buf;

    LOG(INFO) << "In the main thread for recviver";

    for (auto &each_sess : sess_group)
    {
        each_sess->session_init();
        each_sess->sync_data((char *)"0", "sync error before RDMA ops");
        each_sess->pull_ctrl(0);
        each_sess->sync_data((char *)"0", "sync error After RDMA ops");
        //inflight_queue->push(each_sess);
        {
            std::string &peer_ip = each_sess->get_peer_ip();
            size_t group_id = 0;
            bool found = false;
            for (group_id = 0; group_id < conf_.sub_groups.size(); ++group_id)
            {

                for (auto &node_id : conf_.sub_groups[group_id])
                {
                    if (node_id == peer_ip)
                        found = true;
                }
                if (found)
                    break;
            }
            CHECK(found) << "Cannot find the group for: " << peer_ip;
            session_sub_groups[group_id][INFLIGHT_QUEUE]->push(each_sess);
            LOG(INFO) << peer_ip << " belongs to the group: " << group_id;
            sprintf(data_ptr, "[%ld] %s\n", group_id, peer_ip.c_str());
            data_ptr = buf + strlen(data_ptr);
        }
    }
    LOG(INFO) << "Prepared everything for benchmark\n\n\n";
    //Group allreduce_group = {inflight_queue, ready_queue};

    int group_size = session_sub_groups.size();

    do
    {
        for (int index = 0; index < group_size; index++)
        {
            do_reduce(session_sub_groups[index]);
        }
        for (int index = group_size - 1; index >= 0; index--)
        {
            broadcast(session_sub_groups[index]);
        }
        LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "Group info:\n"
                                             << buf;
    } while (true);
    return 0;
}
int RDMAServer::ring_allreduce()
{
    LOG(INFO) << "In the ring allreduce";

    std::queue<RDMASession *> *ready_queue = new std::queue<RDMASession *>();
    std::queue<RDMASession *> *inflight_queue = new std::queue<RDMASession *>();

    LOG(INFO) << "In the main thread for recviver";

    for (auto &each_sess : sess_group)
    {
        each_sess->session_init();
        each_sess->sync_data((char *)"0", "sync error before RDMA ops");
        each_sess->pull_ctrl(0);
        each_sess->sync_data((char *)"0", "sync error After RDMA ops");
        inflight_queue->push(each_sess);
    }
    LOG(INFO) << "Prepared everything for benchmark\n\n\n";
    Group allreduce_group = {inflight_queue, ready_queue};

    do
    {
        do_reduce(allreduce_group);
        broadcast(allreduce_group);
    } while (true);
    return 0;
}
int RDMAServer::full_mesh_allreduce()
{
    LOG(INFO) << "In the full-mesh allreduce";
    std::queue<RDMASession *> *ready_queue = new std::queue<RDMASession *>();
    std::queue<RDMASession *> *inflight_queue = new std::queue<RDMASession *>();

    LOG(INFO) << "In the main thread for recviver";
    //std::this_thread::sleep_for(std::chrono::seconds(1));

    for (auto &each_sess : sess_group)
    {
        each_sess->session_init();
        each_sess->sync_data((char *)"0", "sync error before RDMA ops");
        each_sess->pull_ctrl(0);
        each_sess->sync_data((char *)"0", "sync error After RDMA ops");
        inflight_queue->push(each_sess);
    }
    LOG(INFO) << "Prepared everything for benchmark\n\n\n";
    Group allreduce_group = {inflight_queue, ready_queue};

    do
    {
        do_reduce(allreduce_group);
        broadcast(allreduce_group);
    } while (true);
    return 0;
}

int RDMAServer::double_binary_tree_allreduce()
{
    LOG(INFO) << "In the double binary tree allreduce";
    std::queue<RDMASession *> *ready_queue = new std::queue<RDMASession *>();
    std::queue<RDMASession *> *inflight_queue = new std::queue<RDMASession *>();

    LOG(INFO) << "In the main thread for recviver";
    //std::this_thread::sleep_for(std::chrono::seconds(1));

    for (auto &each_sess : sess_group)
    {
        each_sess->session_init();
        each_sess->sync_data((char *)"0", "sync error before RDMA ops");
        each_sess->pull_ctrl(0);
        each_sess->sync_data((char *)"0", "sync error After RDMA ops");
        inflight_queue->push(each_sess);
    }
    LOG(INFO) << "Prepared everything for benchmark\n\n\n";
    Group allreduce_group = {inflight_queue, ready_queue};
    do
    {
        do_reduce(allreduce_group);
        broadcast(allreduce_group);
    } while (true);
    return 0;
}