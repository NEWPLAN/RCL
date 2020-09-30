#ifndef __NEWPLAN_RDMA_CONTEXT_H__
#define __NEWPLAN_RDMA_CONTEXT_H__
#include <string>

//High-level structure for user operations;
class RDMAContext
{
public:
    RDMAContext();
    virtual ~RDMAContext();

    virtual bool write_remote(void *data_holder) = 0;
    virtual bool read_remote(void *data_holder) = 0;
    virtual bool send_remote(void *data_holder) = 0;
    virtual bool recv_remote(void *data_holder) = 0;
    virtual bool query_event(uint64_t event_token) = 0;

protected:
    std::string context_name;
};
#endif // RDMACONTEXT
