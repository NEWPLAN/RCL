#include "rdma_session.h"
#include "util.h"

RDMAEndpoint *RDMASession::pre_connect(PreConnector *pre)
{
    LOG(INFO) << "RDMA pre_connect";
    pre->pre_connect();
    LOG(FATAL) << "Waiting for pre connector RDMA channel";
    return nullptr;
}

RDMAEndpoint *RDMASession::new_endpoint(PreConnector *pre)
{
    RDMAEndpoint *new_endpoint = new RDMAEndpoint(pd_, cq_, context_,
                                                  pre->config.ib_port,
                                                  CQ_SIZE, nullptr);
    endpoint_list_.push_back(new_endpoint);
    // new_endpoint->connect(pre->exchange_qp_data(new_endpoint->get_local_con_data()));
    LOG(INFO) << "Successfully building RDMA channel";
    return new_endpoint;
}
void RDMASession::delete_endpoint(RDMAEndpoint *endpoint)
{
    std::vector<RDMAEndpoint *>::iterator begin = endpoint_list_.begin();
    std::vector<RDMAEndpoint *>::iterator end = endpoint_list_.end();

    for (auto i = begin; i != end; ++i)
        if (*i == endpoint)
        {
            endpoint_list_.erase(i);
            break;
        }
}

int RDMASession::open_ib_device()
{
    int i, num_devices;
    struct ibv_device *ib_dev = NULL;
    struct ibv_device **dev_list;

    LOG(INFO) << "Starting resoures initialization";
    LOG(INFO) << "Searching for IB devices in the host";

    // get device names in the system
    dev_list = ibv_get_device_list(&num_devices);
    if (!dev_list)
    {
        LOG(FATAL) << "Failed to get IB devices list";
        return 1;
    }

    // if there isn't any IB device in host
    if (!num_devices)
    {
        LOG(FATAL) << "IB device not found";
        return 1;
    }
    else
        LOG(INFO) << "Found" << num_devices << "device(s)";

    if (!dev_name_)
    {
        ib_dev = *dev_list;
        if (!ib_dev)
        {
            LOG(FATAL) << "No IB devices found";
            return 1;
        }
        LOG(INFO) << "Choose the first IB device";
    }
    else
    {
        // search for the specific device we want to work with
        for (i = 0; i < num_devices; i++)
        {
            if (!strcmp(ibv_get_device_name(dev_list[i]), dev_name_))
            {
                ib_dev = dev_list[i];
                break;
            }
        }
        // if the device wasn't found in host
        if (!ib_dev)
        {
            LOG(WARNING) << "IB device " << dev_name_ << " wasn't found";
            return 1;
        }
    }
    LOG(INFO) << "Opening the device: " << ibv_get_device_name(ib_dev);
    // get device handle
    context_ = ibv_open_device(ib_dev);
    if (!context_)
    {
        LOG(FATAL) << "Failed to open device " << dev_name_;
        return 1;
    }

    return 0;
}

void RDMASession::session_processCQ()
{
    status_ = WORK;
    while (status_ != CLOSED)
    {
        ibv_cq *cq;
        void *cq_context;
        // Pre-allocated work completions array used for polling
        ibv_wc wc_[CQ_SIZE];

        if (ibv_get_cq_event(event_channel_, &cq, &cq_context))
        {
            LOG(FATAL) << "Failed to get cq_event";
            return;
        }

        if (cq != cq_)
        {
            LOG(FATAL) << "CQ event for unknown CQ " << cq;
            return;
        }

        ibv_ack_cq_events(cq, 1);

        if (ibv_req_notify_cq(cq_, 0))
        {
            LOG(FATAL) << "Countn't request CQ notification";
            return;
        }

        int ne = ibv_poll_cq(cq_, CQ_SIZE, static_cast<ibv_wc *>(wc_));
        // VAL(ne);
        if (ne > 0)
            for (int i = 0; i < ne; i++)
            {
                if (wc_[i].status != IBV_WC_SUCCESS)
                {
                    printf("[%s:%d] Got bad completion with status: 0x%x, vendor syndrome: 0x%x\n", __FILE__,
                           __LINE__,
                           wc_[i].status,
                           wc_[i].vendor_err);
                    exit(0);
                    return;
                    // error
                }
                switch (wc_[i].opcode)
                {
                case IBV_WC_RECV_RDMA_WITH_IMM: // Recv Remote RDMA Write Message
                    LOG(INFO) << "RECV_RDMA_WITH_IMM";
                    //RDMA_Message::process_attached_message(wc_[i], this);
                    break;
                case IBV_WC_RECV: // Recv Remote RDMA Send Message
                    LOG(INFO) << "IBV_WC_RECV";
                    //RDMA_Message::process_immediate_message(wc_[i], this);
                    break;
                case IBV_WC_RDMA_WRITE: // Successfully Write RDMA Message or Data
                    LOG(INFO) << "IBV_WC_RDMA_WRITE";
                    //RDMA_Message::process_write_success(wc_[i], this);
                    break;
                case IBV_WC_SEND: // Successfully Send RDMA Message
                    LOG(INFO) << "IBV_WC_SEND";
                    //RDMA_Message::process_send_success(wc_[i], this);
                    break;
                case IBV_WC_RDMA_READ: // Successfully Read RDMA Data
                    LOG(INFO) << "IBV_WC_RDMA_READ";
                    //RDMA_Message::process_read_success(wc_[i], this);
                    break;
                default:
                    LOG(WARNING) << "Unsupported opcode";
                }
            }
    }
}