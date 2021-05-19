#include "network_service.h"
#include "utils/logging.h"

bool LogRegister::registerted = false;

#include <chrono>
#include <future>
#include <thread>

bool is_prime(int x)
{
    for (int i = 2; i < x; i++)
    {
        if (x % i == 0)
            return false;
    }
    while (true)
    {
        LOG(INFO) << "is_prime sleep in this threads";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return true;
}

int async_call_test()
{
    std::vector<std::future<bool> > post_connect_tasks;
    for (int index = 0; index < 10; index++)
        post_connect_tasks.push_back(std::async([](int index_) -> bool
                                                {
                                                    while (true)
                                                    {
                                                        LOG(INFO) << " sleep in thread: " << index_;
                                                        std::this_thread::sleep_for(std::chrono::seconds(1));
                                                    }
                                                    return 0;
                                                },
                                                index));
    LOG(INFO) << "please wait";
    std::chrono::milliseconds span(100);

    for (auto &fut : post_connect_tasks)
    {
        if (fut.wait_for(span) != std::future_status::ready)
            std::cout << ".";
    }

    std::cout << ".";
    std::cout << std::endl;
    return 0;
}

int main(int argc, char *argv[])
{
    //async_call_test();
    LogRegister::log_test();
    using NetService = communication::NetworkingService;
    NetService *ns = new NetService();
    ns->init_service(argc, argv);
    return ns->start_service_async();
}
