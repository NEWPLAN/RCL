#include <iostream>
#include "RDMABase.h"
#include "RDMAServer.h"
#include "RDMAClient.h"
#include "NetUtil.hpp"

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
    rserver = new RDMAServer("0.0.0.0");
    auto t = std::chrono::high_resolution_clock::now();
    rserver->bind_recv_imm(IMM_TEST, [](ibv_wc *wc){
        std::cout << "芜湖! 服务端起飞! \n";
    });
    rserver->bind_recv_imm(IMM_CLIENT_SEND_DONE, [&t](ibv_wc *wc){
        std::cout << "Time used: " << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - t).count() << std::endl;
    });
    new std::thread([rserver](){
        rserver->setup();
    });

    while (1)
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        rserver->broadcast_imm(IMM_CLIENT_WRITE_START);
        t = std::chrono::high_resolution_clock::now();
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
            auto t = std::chrono::high_resolution_clock::now();
            rclient = new RDMAClient(server_ip, local_ip, job_queue);
            // 测试 bind_recv_imm
            rclient->bind_recv_imm(IMM_TEST, [job_queue](ibv_wc* wc){
                std::cout << "芜湖! 客户端起飞!\n" ;
            });
            rclient->bind_recv_imm(IMM_CLIENT_WRITE_START, [job_queue, &t](ibv_wc* wc){
                job_queue -> push(comm_job(comm_job::WRITE, 536870908));
                t = std::chrono::high_resolution_clock::now();
            });
            rclient->set_when_write_finished([job_queue, &t](){
                job_queue -> push(comm_job(comm_job::SEND_IMM, IMM_CLIENT_SEND_DONE));
                std::cout << "(Client) Time used: " << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - t).count() << std::endl;
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

    return 0;
}