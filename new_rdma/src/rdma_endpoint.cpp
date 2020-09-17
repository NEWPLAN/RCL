#include "rdma_endpoint.h"
#include <glog/logging.h>
namespace newplan
{

    void RDMAEndpoint::init_endpoint()
    {
        if (socket_fd != 0)
        {
            LOG(WARNING) << "already init the endpoint";
            return;
        }

        socket_fd = socket(AF_INET, SOCK_STREAM, 0);

        if (socket_fd < 0)
        {
            LOG(FATAL) << "Error in creating socket";
        }

        int one = 1;
        if (setsockopt(socket_fd, SOL_SOCKET,
                       SO_REUSEADDR, &one, sizeof(one)) < 0)
        {
            close(socket_fd);
            LOG(FATAL) << "reusing socket failed";
        }

        LOG(INFO) << "Has created the socket for pre_connector";
    }

}; // namespace newplan