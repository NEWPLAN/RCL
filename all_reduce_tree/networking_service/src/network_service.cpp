#include "network_service.h"
#include <glog/logging.h>

#include "rdma_base.h"
#include "rdma_adapter.h"
#include "config.h"

#include "endnode.h"
#include "rdma_client.h"
#include "rdma_server.h"
#include <vector>
#include <thread>
#include "double_binary_tree.h"

namespace communication
{
    int NetworkingService::start_service_async()
    {
        CHECK(is_initialized) << "Please initialize the service before runing";
        if (is_launched)
        {
            LOG(WARNING) << "Networking service has already been launched";
            return 0;
        }
        is_launched = true;
        switch (conf.all_reduce)
        {
        case SERVER_CLIENT:
            return server_client_service();
            break;
        case FULL_MESH_ALLREDUCE:
            return full_mesh_service();
            break;
        case RING_ALLREDUCE:
            return ring_service();
            break;
        case DOUBLE_BINARY_TREE_ALLREDUCE:
            return double_binary_tree_service();
            break;
        case TREE_ALLREDUCE:
            return tree_service();
            break;
        case UNKNOWN_ALLREDUCE:
            break;
        }
        LOG(FATAL) << "Unknown service type: "
                   << conf.service_type;
        return 0;
    }

    /*assigning a server and a client at each physical node*/
    int NetworkingService::full_mesh_service()
    {
        CHECK(is_initialized) << "Please initialize the service before runing";
        std::vector<std::thread *> service_threads;
        LOG(INFO) << "In the full mesh service";
        {
            LOG(INFO) << "launching server service";
            Config new_config = conf;
            new_config.serve_as_client = false;
            new_config.server_name = "";
            std::thread *server = new std::thread([new_config]() {
                RDMAEndNode *npoint = new RDMAServer();
                npoint->setup(new_config);
                npoint->connect_to_peer();
                npoint->run();
                delete npoint;
            });
            service_threads.push_back(server);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        {
            LOG(INFO) << "launching client service";
            for (auto &each_server : conf.cluster)
            {
                CHECK(each_server != conf.local_ip)
                    << "Cannot speficy connection with your own";
                Config new_config = conf;
                new_config.set_server_name(each_server);
                std::thread *client = new std::thread([new_config]() {
                    RDMAEndNode *npoint = new RDMAClient();
                    npoint->setup(new_config);
                    npoint->connect_to_peer();
                    npoint->run();
                    delete npoint;
                });
                service_threads.push_back(client);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        for (auto &each_thread : service_threads)
            each_thread->join();

        return 0;
    }

    /*single server(receiver) and multiple clients (senders) */
    int NetworkingService::server_client_service()
    {
        CHECK(is_initialized) << "Please initialize the service before runing";
        LOG(INFO) << "In the full mesh service";
        RDMAEndNode *npoint = nullptr;
        if (conf.serve_as_client)
        {
            LOG(INFO) << "You are in client side";
            npoint = new RDMAClient();
        }
        else
        {
            LOG(INFO) << "You are in server side";
            npoint = new RDMAServer();
        }

        npoint->setup(conf);
        npoint->connect_to_peer();
        npoint->run();

        delete npoint;
        return 0;
    }

    /*the ring topology*/
    int NetworkingService::ring_service()
    {
        CHECK(is_initialized) << "Please initialize the service before runing";

        std::string next_hop = get_neighboring_node(conf.local_ip,
                                                    conf.cluster);
        LOG(INFO) << "Next hop: " << conf.local_ip << "-->" << next_hop;
        conf.num_senders = 1;

        std::vector<std::thread *> service_threads;
        LOG(INFO) << "In the ring service";
        {
            LOG(INFO) << "launching server service";
            Config new_config = conf;
            new_config.serve_as_client = false;
            new_config.server_name = "";
            std::thread *server = new std::thread([new_config]() {
                RDMAEndNode *npoint = new RDMAServer();
                npoint->setup(new_config);
                npoint->connect_to_peer();
                npoint->run();
                delete npoint;
            });
            service_threads.push_back(server);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        {
            LOG(INFO) << "launching client service";

            CHECK(next_hop != conf.local_ip)
                << "Cannot speficy connection with your own";
            Config new_config = conf;
            new_config.set_server_name(next_hop);
            std::thread *client = new std::thread([new_config]() {
                RDMAEndNode *npoint = new RDMAClient();
                npoint->setup(new_config);
                npoint->connect_to_peer();
                npoint->run();
                delete npoint;
            });
            service_threads.push_back(client);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        for (auto &each_thread : service_threads)
            each_thread->join();

        return 0;
    }

    static std::vector<std::vector<std::string>> extract_sub_groups(const std::string &local_ip,
                                                                    const int tree_width,
                                                                    const std::vector<std::string> &cluster,
                                                                    bool filter_local = false)
    {
        std::vector<std::vector<std::string>> sub_groups;
        std::vector<std::string> tmp_cluster = cluster;
        tmp_cluster.push_back(local_ip);
        std::sort(tmp_cluster.begin(), tmp_cluster.end());
        size_t location_index = 0;
        size_t cluster_size = tmp_cluster.size();
        { //security check:
            size_t tmp_val = cluster_size;
            while (tmp_val > 0)
            {
                if (tmp_val == (size_t)tree_width)
                    break;
                if (tmp_val % tree_width != 0)
                {
                    LOG(FATAL) << "the cluster_size(" << cluster_size
                               << ") must be pow of " << tree_width;
                    break;
                }
                tmp_val /= tree_width;
            }
        }

        for (location_index = 0; location_index < cluster_size; location_index++)
        {
            if (tmp_cluster[location_index] == local_ip)
                break;
        }
        LOG(INFO) << local_ip << " is indexed by " << location_index
                  << ", tree width: " << tree_width;

        size_t step = 1;
        while (step < cluster_size)
        {
            size_t wind_offset = location_index % (step);                        //get offset
            size_t base = location_index - location_index % (step * tree_width); //get left margin
            std::vector<std::string> tmp_group;
            for (size_t left_margin = base; left_margin < cluster_size; left_margin += step)
            {
                if (tmp_group.size() == (size_t)tree_width)
                    break;
                // LOG(INFO) << left_margin << ", " << left_margin + wind_offset;
                tmp_group.push_back(tmp_cluster[left_margin + wind_offset]);
            }

            step *= tree_width; //doubling window_size
            sub_groups.push_back(tmp_group);
            tmp_group.clear();
        }

        std::vector<std::vector<std::string>> filtered_group = sub_groups;

        if (filter_local)
        {
            filtered_group.clear();
            for (auto &sub_group : sub_groups)
            {
                std::vector<std::string> tmp_group;
                for (auto &each_ip : sub_group)
                {
                    if (each_ip != local_ip)
                        tmp_group.push_back(each_ip);
                }
                filtered_group.push_back(tmp_group);
            }
        }

        for (auto &sub_group : filtered_group)
        {
            for (auto &each_ip : sub_group)
                std::cout << ", " << each_ip;
            std::cout << std::endl;
        }

        return filtered_group;
    }

    /*the tree topology*/
    int NetworkingService::tree_service()
    {
        CHECK(is_initialized) << "Please initialize the service before runing";
        LOG(INFO) << "In the tree service: " << conf.local_ip;
        conf.print_config();
        conf.sub_groups = extract_sub_groups(conf.local_ip,
                                             conf.tree_width,
                                             conf.cluster,
                                             true);
        int num_size = 0;
        for (auto &each_group : conf.sub_groups)
            num_size += each_group.size();
        LOG(INFO) << "# of receiver is: " << num_size;

        std::vector<std::thread *> service_threads;
        {
            LOG(INFO) << "launching server service";
            Config new_config = conf;
            new_config.serve_as_client = false;
            new_config.server_name = "";
            new_config.num_senders = num_size;
            std::thread *server = new std::thread([new_config]() {
                RDMAEndNode *npoint = new RDMAServer();
                npoint->setup(new_config);
                npoint->connect_to_peer();
                npoint->run();
                delete npoint;
            });
            service_threads.push_back(server);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        {
            LOG(INFO) << "launching client service";
            for (auto &each_group : conf.sub_groups)
                for (auto &each_server : each_group)
                {
                    CHECK(each_server != conf.local_ip)
                        << "Cannot speficy connection with your own";
                    Config new_config = conf;
                    new_config.set_server_name(each_server);
                    std::thread *client = new std::thread([new_config]() {
                        RDMAEndNode *npoint = new RDMAClient();
                        npoint->setup(new_config);
                        npoint->connect_to_peer();
                        npoint->run();
                        delete npoint;
                    });
                    service_threads.push_back(client);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
        }

        for (auto &each_thread : service_threads)
            each_thread->join();

        return 0;
    }

    /*the double binary tree topology*/
    int NetworkingService::double_binary_tree_service()
    {
        CHECK(is_initialized) << "Please initialize the service before runing";
        LOG(INFO) << "In the double binary tree service: " << conf.local_ip;
        std::vector<std::vector<std::vector<std::string>>> trees;
        double_binary_tree::get_tree_neighbors(conf.local_ip,
                                               conf.cluster, trees);

        int num_size = trees[0][1].size() + trees[1][1].size();
        LOG(INFO) << "# of receiver is: " << num_size;

        std::vector<std::thread *> service_threads;
        {
            LOG(INFO) << "launching server service";
            Config new_config = conf;
            new_config.serve_as_client = false;
            new_config.server_name = "";
            new_config.num_senders = num_size;
            std::thread *server = new std::thread([new_config]() {
                RDMAEndNode *npoint = new RDMAServer();
                npoint->setup(new_config);
                npoint->connect_to_peer();
                npoint->run();
                delete npoint;
            });
            service_threads.push_back(server);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        {
            LOG(INFO) << "launching client service";
            for (auto &each_tree : trees)
                for (auto &each_server : each_tree[0])
                {
                    CHECK(each_server != conf.local_ip)
                        << "Cannot speficy connection with your own";
                    Config new_config = conf;
                    new_config.set_server_name(each_server);
                    std::thread *client = new std::thread([new_config]() {
                        RDMAEndNode *npoint = new RDMAClient();
                        npoint->setup(new_config);
                        npoint->connect_to_peer();
                        npoint->run();
                        delete npoint;
                    });
                    service_threads.push_back(client);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
        }

        for (auto &each_thread : service_threads)
            each_thread->join();

        return 0;
    }

} // namespace communication