#ifndef __NEWPLAN_RDMA_ENDNODE_H__
#define __NEWPLAN_RDMA_ENDNODE_H__
#include "rdma_base.h"
#include "config.h"
#include <vector>
#include <thread>
class RDMAEndNode
{
public:
    RDMAEndNode() {}
    virtual ~RDMAEndNode() {}

public:
    virtual void setup(Config conf) { this->conf_ = conf; };
    virtual void connect_to_peer()
    {
        pre_connect();
        connecting();
        post_connect();
    };
    virtual void run() = 0;

protected:
    virtual void pre_connect() = 0;
    virtual void connecting() = 0;
    virtual void post_connect() = 0;

protected:
    int root_sock = 0;
    struct Config conf_;
    std::vector<std::thread *> service_threads;
};

#endif /* __NEWPLAN_RDMA_ENDNODE_H__ */