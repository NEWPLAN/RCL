#ifndef __NEWPLAN_RDMA_ENDPOINT_H__
#define __NEWPLAN_RDMA_ENDPOINT_H__

#include "rdma_session.h"
#include <vector>

namespace newplan
{

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

        virtual void start_service(RDMASession *sess) = 0;
        virtual void start_service_default(RDMASession *sess) = 0;
        virtual void start_service_single_channel(RDMASession *sess) = 0;

    public:
        void store_session(RDMASession *new_sees)
        {
            rdma_session.push_back(new_sees);
        }
        RDMASession *get_session(int index)
        {
            if (index < 0 || index >= (int)rdma_session.size())
            {
                LOG(ERROR) << "Invalid index: " << index
                           << ", must less than: "
                           << rdma_session.size();
                return nullptr;
            }
            return rdma_session[index];
        }

    public:
        static char *get_rdma_type_str(enum EndType type)
        {
            static char *RDMA_TYPE_STRING[] =
                {
                    (char *)"Unknown Type",
                    (char *)"Server",
                    (char *)"Client",
                    (char *)"Undefined"};

            if (type < 0 || type >= sizeof(RDMA_TYPE_STRING) / sizeof(char *))
            {
                LOG(FATAL) << "Error of get RDMA type string: ";
            }
            return RDMA_TYPE_STRING[type];
        }

    protected:
        short get_socket() const { return socket_fd; }

    public:
        EndType get_type() const { return type_; }

    private:
        EndType type_ = ENDPOINT_UNKNOWN;
        short socket_fd = 0;
        std::vector<RDMASession *> rdma_session;
    };

}; // namespace newplan

#endif