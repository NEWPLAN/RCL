#ifndef __NEWPLAN_RDMA_ENDPOINT_H__
#define __NEWPLAN_RDMA_ENDPOINT_H__

namespace newplan
{
static char *RDMA_TYPE_STRING[] =
    {
        "Unknown Type",
        "Server",
        "Client",
        "Undefined"};
enum EndType
{
    ENDPOINT_UNKNOWN,
    ENDPOINT_SERVER,
    ENDPOINT_CLIENT,
};
class RDMAEndpoint
{
public:
    explicit RDMAEndpoint(EndType type) : type_(type) {}
    virtual ~RDMAEndpoint() {}

    virtual void run() = 0;
    virtual void run_async() = 0;

public:
    EndType get_type() const { return type_; }

private:
    EndType type_;
};

class RDMAServer : public RDMAEndpoint
{
public:
    explicit RDMAServer() : RDMAEndpoint(ENDPOINT_SERVER) {}
    virtual ~RDMAServer() {}

    virtual void run_async() override;
    virtual void run() override;
};

class RDMAClient : public RDMAEndpoint
{
public:
    explicit RDMAClient() : RDMAEndpoint(ENDPOINT_CLIENT) {}
    virtual ~RDMAClient() {}

    virtual void run_async() override;
    virtual void run() override;
};
}; // namespace newplan

#endif