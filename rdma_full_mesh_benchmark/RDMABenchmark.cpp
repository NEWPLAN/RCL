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
    new std::thread([rserver](){
        rserver->setup();
    });

    while (1)
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        rserver->broadcast_imm(IMM_TEST);
    }
}

void client_functions(std::vector<std::string> ip)
{
    std::vector< BlockingQueue<uint32_t>* > job_queues;
    if (ip.size() == 0)
    {
        std::cout << "Error of local IP" << std::endl;
    }
    std::string local_ip = ip[0];
    for (size_t i = 1; i < ip.size(); i++)
    {
        job_queues.push_back(new BlockingQueue<uint32_t>);
        std::string server_ip = ip[i];

        std::cout << "Connecting to: " << server_ip << std::endl;
        new std::thread([server_ip, local_ip, &job_queues]() {
            RDMAClient *rclient;
            rclient = new RDMAClient(server_ip, local_ip, job_queues.back());
            // 测试 bind_recv_imm
            rclient->bind_recv_imm(IMM_TEST, [](ibv_wc* wc){
                std::cout << "芜湖起飞芜湖起飞芜湖起飞芜湖起飞\n" ;
            });
            rclient->setup();
        });
    }
    for (int j = 0; j != 1; j++)
        for (auto i : job_queues)
        {
            i->push(536870908);
        }
    while (1)
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

int main(int argc, char const *argv[])
{ //./main self-ip, remote ip
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