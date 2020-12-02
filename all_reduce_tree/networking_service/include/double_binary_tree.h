#ifndef __DOUBLE_BINARY_TREE_topo_H__
#define __DOUBLE_BINARY_TREE_topo_H__
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
namespace double_binary_tree
{
    static int compare_reverse(const std::string &a, const std::string &b)
    {
        return a > b;
    }

    static int get_index(const std::string &node,
                         const std::vector<std::string> &cluster)
    {
        size_t index = 0;
        bool found = false;
        for (index = 0; index < cluster.size(); index++)
        {
            if (cluster[index] == node)
            {
                found = true;
                break;
            }
        }
        if (found)
            return index;
        return -1;
    }

    static bool get_connection(const std::string &node,
                               std::vector<std::string> &cluster,
                               std::vector<std::string> &out,
                               std::vector<std::string> &in)
    {
        size_t offset = get_index(node, cluster);
        if (offset == 0)
        {
            in.push_back(cluster[1]);
            return true;
        }
        if (offset < cluster.size() / 2)
        {
            in.push_back(cluster[2 * offset]);
            in.push_back(cluster[2 * offset + 1]);
        }
        out.push_back(cluster[offset / 2]);

        return true;
    }

    bool get_tree_neighbors(const std::string &local_ip,
                            const std::vector<std::string> &cluster_,
                            std::vector<std::vector<std::vector<std::string>>> &trees)
    {
        std::vector<std::string> cluster, cluster_reverse;
        cluster = cluster_;
        cluster.push_back(local_ip);
        cluster_reverse = cluster;

        std::sort(cluster.begin(), cluster.end());
        std::sort(cluster_reverse.begin(),
                  cluster_reverse.end(), compare_reverse);

        trees.resize(2);

        LOG(INFO) << "Tree1: ";
        std::vector<std::string> upper, downstream;
        get_connection(local_ip, cluster, upper, downstream);
        for (auto &each_edge : upper)
            std::cout << local_ip << "-->" << each_edge << " ";
        for (auto &each_edge : downstream)
            std::cout << each_edge << "-->" << local_ip << " ";
        std::cout << std::endl;
        trees[0] = {upper, downstream};

        upper.clear();
        downstream.clear();

        LOG(INFO) << "Tree2: ";
        get_connection(local_ip, cluster_reverse, upper, downstream);
        for (auto &each_edge : upper)
            std::cout << local_ip << "-->" << each_edge << " ";
        for (auto &each_edge : downstream)
            std::cout << each_edge << "-->" << local_ip << " ";
        std::cout << std::endl;

        trees[1] = {upper, downstream};
        return true;
    }

    int test_case_main(void)
    {
        std::vector<std::string> cluster, cluster_reverse;
        for (int index = 101; index <= 116; index++)
        {
            std::string base = "12.12.12.";
            cluster.push_back(base + std::to_string(index));
        }
        cluster_reverse = cluster;

        std::sort(cluster.begin(), cluster.end());
        std::sort(cluster_reverse.begin(), cluster_reverse.end(), compare_reverse);

#if defined SHOW_CLUSTER
        std::cout << "Cluster:";
        for (auto &each_node : cluster)
        {
            std::cout << " " << each_node;
        }
        std::cout << std::endl;

        std::cout << "Cluster_reverse:";
        for (auto &each_node : cluster_reverse)
        {
            std::cout << " " << each_node;
        }
        std::cout << std::endl;
#endif
        std::cout << "Tree 1:" << std::endl;
        for (auto &each_node : cluster)
        {
            std::vector<std::string> upper, downstream;
            get_connection(each_node, cluster, upper, downstream);
            std::cout << "Node: " << each_node << ":    ";
            for (auto &each_edge : upper)
                std::cout << each_node << "-->" << each_edge << " ";
            for (auto &each_edge : downstream)
                std::cout << each_edge << "-->" << each_node << " ";
            std::cout << std::endl;
        }
        std::cout << "\n\nTree 2:" << std::endl;
        for (auto &each_node : cluster)
        {
            std::vector<std::string> upper, downstream;
            get_connection(each_node, cluster_reverse, upper, downstream);
            std::cout << "Node: " << each_node << ":    ";
            for (auto &each_edge : upper)
                std::cout << each_node << "-->" << each_edge << " ";
            for (auto &each_edge : downstream)
                std::cout << each_edge << "-->" << each_node << " ";
            std::cout << std::endl;
        }
        return 0;
    }
} // namespace double_binary_tree

// int main(void)
// {
//     return double_binary_tree::test_case_main();
// }

#endif //