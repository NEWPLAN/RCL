#ifndef __NEWPLAN_RDMA_CLIENT_H__
#define __NEWPLAN_RDMA_CLIENT_H__
#include "endnode.h"

#include "rdma_adapter.h"

class RDMAClient : public RDMAEndNode
{
public:
    RDMAClient(){};
    virtual ~RDMAClient(){};

public:
    //virtual void connect_to_peer() override;
    virtual void run() override;

protected:
    virtual void pre_connect();
    virtual void connecting();
    virtual void post_connect();

protected: //temp
    void client_test(NPRDMAdapter *);
    void two_channel_test(NPRDMAdapter *);

private:
    void config_check();

    int is_connected = false;
};
#endif