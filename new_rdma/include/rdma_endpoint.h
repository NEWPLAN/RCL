#ifndef __NEWPLAN_RDMA_ENDPOINT_H__
#define __NEWPLAN_RDMA_ENDPOINT_H__

#include "tmp_rdma_session.h"
#include <vector>

namespace newplan
{
    static char *RDMA_TYPE_STRING[] =
        {
            (char *)"Unknown Type",
            (char *)"Server",
            (char *)"Client",
            (char *)"Undefined"};
    enum EndType
    {
        ENDPOINT_UNKNOWN,
        ENDPOINT_SERVER,
        ENDPOINT_CLIENT,
    };
    class RDMAEndpoint
    {
    public:
        explicit RDMAEndpoint(EndType type)
            : type_(type) { init_endpoint(); }
        virtual ~RDMAEndpoint() {}

        void init_endpoint();

        virtual void run() = 0;
        virtual void run_async() = 0;

        virtual void before_connect() = 0;
        virtual void do_connect() = 0;
        virtual void after_connect() = 0;

        virtual void start_service(NPRDMASession *sess) = 0;

    public:
        void store_session(NPRDMASession *new_sees)
        {
            rdma_session.push_back(new_sees);
        }
        NPRDMASession *get_session(int index)
        {
            if (index < 0 || index >= rdma_session.size())
            {
                LOG(ERROR) << "Invalid index: " << index
                           << ", must less than: "
                           << rdma_session.size();
                return nullptr;
            }
            return rdma_session[index];
        }

    protected:
        short get_socket() const { return socket_fd; }

    public:
        EndType get_type() const { return type_; }

    private:
        EndType type_ = ENDPOINT_UNKNOWN;
        short socket_fd = 0;
        std::vector<NPRDMASession *> rdma_session;
    };

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

        virtual void start_service(NPRDMASession *sess) override;

    private:
        std::string server_ip_;
        short port_;
        std::vector<std::thread *> connection_threads;
    };

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

        virtual void start_service(NPRDMASession *sess) override;

    private:
        PreConnector *pre_connecting();

    private:
        std::string server_ip_;
        short port_;
        bool is_connected_ = false;
    };
}; // namespace newplan

#endif