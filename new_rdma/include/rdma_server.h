#ifndef __NEWPLAN_RDMA_SERVER_H__
#define __NEWPLAN_RDMA_SERVER_H__
#include "rdma_endpoint.h"
namespace newplan
{

    class RDMAServer : public RDMAEndpoint
    {
    public:
        explicit RDMAServer(short listen_port,
                            std::string ip_addr = "0.0.0.0")
            : RDMAEndpoint(ENDPOINT_SERVER)
        {
            server_ip_ = ip_addr;
            port_ = listen_port;
        }
        virtual ~RDMAServer() {}

        virtual void run_async() override;
        virtual void run() override;

        virtual void before_connect() override;
        virtual void do_connect() override;
        virtual void after_connect() override;

        virtual void start_service(RDMASession *sess) override;
        virtual void start_service_default(RDMASession *sess) override;
        virtual void start_service_single_channel(RDMASession *sess) override;

    private:
        std::string server_ip_;
        short port_;
        std::vector<std::thread *> connection_threads;
    };

};     // namespace newplan
#endif // __NEWPLAN_RDMA_SERVER_H__