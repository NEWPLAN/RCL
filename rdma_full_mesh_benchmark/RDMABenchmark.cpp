#include <iostream>
#include <atomic>
#include "RDMABase.h"
#include "RDMAServer.h"
#include "RDMAClient.h"
#include "NetUtil.hpp"
#include "timer.h"
#include <sstream>
#include <fstream>
#include <vector>
#include <ctime>

template<typename T>
void write_vector_to_file(std::vector<T> vec, std::string filename)
{
    std::ofstream f(filename, std::ios::out);
    for (auto &i : vec)
    {
        f << i << std::endl;
    }
    f.close();
}

void help(void)
{
    std::cout << "Useage:\n";
    std::cout << "For Server: ./rdma --server server_ip\n";
    std::cout << "For Client: ./rdma --server server_ip --client client_ip" << std::endl;
    return;
}

int single_benchmark(int argc, char const *argv[])
{
    RDMAAdapter rdma_adapter;

    RDMAClient *rclient;
    RDMAServer *rserver;

    switch (argc)
    {
    case 3:
        rdma_adapter.set_server_ip(argv[2]);

        rserver = new RDMAServer(rdma_adapter);
        rserver->setup();
    case 5:
        rdma_adapter.set_server_ip(argv[2]);
        rdma_adapter.set_client_ip(argv[4]);

        rclient = new RDMAClient(rdma_adapter);
        rclient->setup();
    default:
        help();
        exit(-1);
        break;
    }

    return 0;
}

#include <vector>
#include <string>

void server_functions(std::vector<std::string> ip)
{
    RDMAServer *rserver;
    rserver = new RDMAServer("0.0.0.0", SERVER_PORT);
    newplan::Timer t;
    rserver->bind_recv_imm(IMM_TEST, [](ibv_wc *wc){
        std::cout << "芜湖! 服务端起飞! \n";
    });
    rserver->bind_recv_imm(IMM_CLIENT_SEND_DONE, [&t](ibv_wc *wc){
        t.Stop();
        std::cout << "Time used: " << t.MilliSeconds() << std::endl;
    });
    new std::thread([rserver](){
        rserver->setup();
    });

    while (1)
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        rserver->broadcast_imm(IMM_CLIENT_WRITE_START);
        t.Start();
    }
}

void client_functions(std::vector<std::string> ip)
{
    std::vector< BlockingQueue<comm_job>* > job_queues;
    if (ip.size() == 0)
    {
        LOG(FATAL) << "Error of local IP";
    }
    std::string local_ip = ip[0];
    for (size_t i = 1; i < ip.size(); i++)
    {
        auto job_queue = new BlockingQueue<comm_job>;
        job_queues.push_back(job_queue);
        std::string server_ip = ip[i];

        std::cout << "Connecting to: " << server_ip << std::endl;
        new std::thread([server_ip, local_ip, job_queue]() {
            RDMAClient *rclient;
            newplan::Timer t;
            rclient = new RDMAClient(server_ip, local_ip, SERVER_PORT, job_queue);
            // 测试 bind_recv_imm
            rclient->bind_recv_imm(IMM_TEST, [job_queue](ibv_wc* wc){
                std::cout << "芜湖! 客户端起飞!\n" ;
            });
            rclient->bind_recv_imm(IMM_CLIENT_WRITE_START, [job_queue, &t](ibv_wc* wc){
                job_queue -> push(comm_job(comm_job::WRITE, 536870908));
                t.Start();
            });
            rclient->set_when_write_finished([job_queue, &t](){
                job_queue -> push(comm_job(comm_job::SEND_IMM, IMM_CLIENT_SEND_DONE));
                t.Stop();
                std::cout << "(Client) Time used: " << t.MilliSeconds() << std::endl;
                std::cout << "客户端发完了! \n";
            });
            rclient->setup();
        });
    }
    std::this_thread::sleep_for(std::chrono::seconds(7));
    for (int j = 0; j != 1; j++)
        for (auto i : job_queues)
        {
            i->push(comm_job(comm_job::SEND_IMM, IMM_TEST));
        }
    while (1)
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

/**
 * 受 master 控制的数据发送
 * @param ips 整个 cluster 的 ip, 包括自己
 * @param master_ip master 的 ip
 * @return 没有返回值
 */
void master_control(std::vector<std::string> ips, std::string master_ip, uint32_t data_size = 536870908)
{
    if (ips.empty() || master_ip.empty())
    {
        LOG(FATAL) << "Error ip parameters";
    }

    std::string local_ip = NetTool::get_ip("12.12.12.101", 24);
    std::vector< BlockingQueue<comm_job>* > client_job_queues;
    BlockingQueue<comm_job>* control_queue = new BlockingQueue<comm_job>;
    const int count_clients = ips.size() - 1;
    std::atomic<int> jobs_left; // 当前还没有完成发送的客户端
    std::atomic<int> clients_left; //当前还没有发送完成的客户机
    // 辨析: 一个机器上会有 n-1 个客户端. 一个客户机上所有的客户端发送完毕之后 (jobs_left == 0), 发消息给 master; master 收到所有客户机的消息之后 (clients_left == 0), 停止计时. 
    const uint8_t tos_data = 64;
    const uint8_t tos_control = 128;
    std::vector<uint32_t> results;

    // Create server
    RDMAServer *rserver = new RDMAServer("0.0.0.0", SERVER_PORT);
    rserver->set_tos(tos_data);
    new std::thread([rserver](){
        rserver->setup();
    });

    // Create clients
    for (const auto &i : ips)
    {
        if (i == local_ip) continue;
        auto job_queue = new BlockingQueue<comm_job>;
        client_job_queues.push_back(job_queue);

        std::cout << "Connecting to: " << i << std::endl;
        RDMAClient *rclient = new RDMAClient(i, local_ip, SERVER_PORT, job_queue);
        rclient->set_tos(tos_data);
        rclient->set_when_write_finished([&jobs_left, control_queue](){
            jobs_left--;
            if (jobs_left == 0)
            {
                // 如果所有客户端都完成了发送, 那么由 control_client 向 master 发送消息
                control_queue->push(comm_job(comm_job::SEND_IMM, IMM_CLIENT_SEND_DONE));
            }
        });
        new std::thread([rclient](){
            rclient->setup();
        });
    }

    //+master-control
    // build control_client
    RDMAClient* control_client = new RDMAClient(master_ip, local_ip, CONTROL_PORT, control_queue);
    control_client->set_tos(tos_control);
    control_client->bind_recv_imm(IMM_CLIENT_WRITE_START, [&client_job_queues, &jobs_left, &count_clients, &data_size](ibv_wc *wc){
        for (auto &i:client_job_queues)
        {
            i->push(comm_job(comm_job::WRITE, data_size));
        }
        jobs_left = count_clients;
    });
    new std::thread([control_client](){
        control_client->setup();
    });
    // master
    if (master_ip == local_ip)
    {
        newplan::Timer *timer = new newplan::Timer();
        RDMAServer* master = new RDMAServer("0.0.0.0", CONTROL_PORT);
        master->set_tos(tos_control);
        master->bind_recv_imm(IMM_CLIENT_SEND_DONE, [timer, &clients_left, count_clients, master, &results](ibv_wc *wc){
            clients_left--;
            if (clients_left == 0)
            {
                timer->Stop();
                LOG(INFO) << "timer stopped";
                std::cout << "(Master) ---------- epoch time: " << timer->MicroSeconds() << "us ----------" << std::endl;
                results.push_back(timer->MicroSeconds());
                if (results.size() >= 10)
                {
                    std::stringstream ss;
                    ss << count_clients + 1 << "." << time(NULL);
                    std::string filename;
                    ss >> filename;
                    write_vector_to_file(results, filename);
                    LOG(INFO) << "Data collecting finished";
                    exit(0);
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
                clients_left = count_clients + 1;
                master->broadcast_imm(IMM_CLIENT_WRITE_START);
                timer->Start();
                LOG(INFO) << "timer started";
            }
        });
        new std::thread([master](){
            master->setup();
        });
        std::this_thread::sleep_for(std::chrono::seconds(10));
        clients_left = count_clients + 1;
        master->broadcast_imm(IMM_CLIENT_WRITE_START);
        timer->Start();
        LOG(INFO) << "timer started";
    }
    while(true)
    {
        std::this_thread::sleep_for(std::chrono::minutes(2));
    }
}

int main(int argc, char const *argv[])
{ //./main self-ip, remote ip
    FLAGS_logtostderr = 1;
    google::InitGoogleLogging(argv[0]);
    if (argc < 2)
    {
        help();
    }
    else if (strcmp(argv[1], "--server") == 0)
    {
        RDMAAdapter rdma_adapter;

        rdma_adapter.set_server_ip("0.0.0.0");

        RDMAServer *rserver;

        rserver = new RDMAServer(rdma_adapter);
        rserver->setup();
        while (1)
        {
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }
    else if (strcmp(argv[1], "--client") == 0)
    {

        RDMAClient *rclient;
        RDMAAdapter rdma_adapter;
        std::string localip = NetTool::get_ip("12.12.12.101", 24);
        rdma_adapter.set_server_ip(argv[2]);
        rdma_adapter.set_client_ip(localip.c_str());
        rclient = new RDMAClient(rdma_adapter);
        rclient->setup();
    }
    else if (strcmp(argv[1], "--control") == 0)
    {
        uint32_t data_size = 536870908; // 欲发送的数据块大小, in bytes
        std::string master = argv[2];
        std::cout << "Master ip: " << master << std::endl;
        std::vector<std::string> ips;
        for (int i = 2; i < argc; i++)
        {
            std::string s = argv[i];
            if (s == "--size")
            {
                std::stringstream ss;
                ss << std::string(argv[i+1]);
                ss >> data_size;
                if (data_size <= 0)
                {
                    LOG(FATAL) << "error data size";
                }
                std::cout << "Data size: " << data_size << std::endl;
                break;
            }
            ips.emplace_back(argv[i]);
            std::cout << "Cluster server ip: " << ips.back() << std::endl;
        }
        std::cout << "----------" << std::endl;
        master_control(ips, master, data_size);
    }
    else if (strcmp(argv[1], "--cluster") == 0)
    {

        std::vector<std::string> ip_address;

        std::string localip = NetTool::get_ip("12.12.12.101", 24);
        std::cout << "local IP: " << localip << std::endl;
        ip_address.push_back(localip);

        for (int index = 2; index < argc; index++)
        {
            std::string ip = std::string(argv[index]);
            std::cout << "IP: " << ip << std::endl;
            if (localip != ip)
                ip_address.push_back(ip);
        }
        {
            std::cout << "Showing IP address:" << std::endl;
            for (auto &ip : ip_address)
            {
                std::cout << " " << ip << std::endl;
            }
        }

        std::thread *server_thread; //
        std::thread *client_thread;
        server_thread = new std::thread([ip_address]() {
            server_functions(ip_address);
        });
        client_thread = new std::thread([ip_address]() {
            client_functions(ip_address);
        });
        server_thread->join();
        client_thread->join();
    }
    else
    {
        LOG(FATAL) << "unknown parameter: " << std::string(argv[1]);
    }
    

    return 0;
}