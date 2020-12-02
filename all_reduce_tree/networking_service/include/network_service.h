#ifndef __NEWPLAN_RDMA_SERVICES_H__
#define __NEWPLAN_RDMA_SERVICES_H__
#include "config.h"
#include <vector>
#include <string>

namespace communication
{
    class NetworkingService
    {
    public:
        NetworkingService() {}
        virtual ~NetworkingService() {}

    public:
        void init_service(int argc, char **argv)
        {
            conf.parse_args(argc, argv);
            is_initialized = true;
        }
        int start_service_async();

    public:
        int full_mesh_service();
        int server_client_service();
        int ring_service();
        int tree_service();
        int double_binary_tree_service();

    private:
        std::string get_neighboring_node(const std::string &this_ip,
                                         const std::vector<std::string> &cluster)
        {
            std::string next_hop;
            for (auto &each_node : cluster)
            {
                if (this_ip < each_node)
                {
                    next_hop = each_node;
                    break;
                }
            }
            if (next_hop.length() == 0 && cluster.size() != 0)
                next_hop = cluster[0]; // the tail is connected to the head node;
            else if (cluster.size() == 0)
                LOG(FATAL) << "[Error] failed to get the next hop";

            return next_hop;
        }

    private:
        Config conf;
        bool is_initialized = false;
        bool is_launched = false;
    };
} // namespace communication
#endif