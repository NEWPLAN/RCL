#ifndef __NEWPLAN_RDMA_CLIENT_H__
#define __NEWPLAN_RDMA_CLIENT_H__
#include "rdma_endpoint.h"
namespace newplan
{
    class RDMAClient : public RDMAEndpoint
    {
    public:
        explicit RDMAClient(short port,
                            std::string ip_addr)
            : RDMAEndpoint(ENDPOINT_CLIENT)
        {
            server_ip_ = ip_addr;
            port_ = port;
        }
        virtual ~RDMAClient() {}

        virtual void run_async() override;
        virtual void run() override;

        virtual void before_connect() override;
        virtual void do_connect() override;
        virtual void after_connect() override;

        virtual void start_service(RDMASession *sess) override;
        virtual void start_service_default(RDMASession *sess) override;
        virtual void start_service_single_channel(RDMASession *sess) override;

    private:
        PreConnector *pre_connecting();

    private:
        std::string server_ip_;
        short port_;
        bool is_connected_ = false;
    };

};     // namespace newplan
#endif // __NEWPLAN_RDMA_CLIENT