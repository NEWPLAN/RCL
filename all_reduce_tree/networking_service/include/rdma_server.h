#ifndef __NEWPLAN_RDMASERVER__H__
#define __NEWPLAN_RDMASERVER__H__
#include "endnode.h"
#include "rdma_adapter.h"
#include "rdma_session.h"
#include <queue>

using Group = std::vector<std::queue<RDMASession *> *>;

class RDMAServer : public RDMAEndNode
{

public:
    RDMAServer(){};
    virtual ~RDMAServer(){};

public:
    //virtual void connect_to_peer() override;
    virtual void run() override;

protected:
    virtual void pre_connect();
    virtual void connecting();
    virtual void post_connect();

protected: //temp
    void server_test(NPRDMAdapter *);
    void two_channel_test(NPRDMAdapter *);

private:
    int do_reduce(Group &);
    int broadcast(Group &, bool data_channel = false);

    int tree_allreduce();
    int ring_allreduce();
    int full_mesh_allreduce();
    int double_binary_tree_allreduce();

private:
    void config_check();
    void handle_with_new_connect(int, struct sockaddr_in);

    int is_connected = false;
    std::vector<RDMASession *> sess_group;
};
#endif //