#ifndef __NEWPLAN_TEMP_RDMA_ADAPTER_H__
#define __NEWPLAN_TEMP_RDMA_ADAPTER_H__
#include <stdlib.h>
#include "rdma_base.h"
#include "rdma_context.h"
#include "pre_connector.h"
#include "config.h"

#define CTRL_SEND 0x0111000000000000
#define DATA_SEND 0x0222000000000000
#define CTRL_RECV 0x0333000000000000
#define DATA_RECV 0x0444000000000000
#define DATA_WRITE 0x0777000000000000
#define DATA_READ 0x0888000000000000

class NPRDMAdapter : public RDMAContext
{

public:
    explicit NPRDMAdapter(Config &con) { this->config = con; }
    virtual ~NPRDMAdapter() {}

public:
    int connect_qp();
    int resources_destroy();
    int resources_create();
    void resources_init(NPRDMAPreConnector *con);

public:
    struct ibv_mr *regisger_mem(void **, int, int access_flags = 0);

public:
    // virtual void init_context() override;
    // virtual void connect_qp() override;
    // virtual void release_context() override;

    int post_ctrl_recv(int index = 0);
    int post_ctrl_send(int index = 0, bool dumb = false);

    int post_data_write(int index = 0, int msg_size = 0);
    int post_data_write_with_imm(int index = 0,
                                 int imm = 0,
                                 int msg_size = 0);
    int post_data_read(int index = 0);

    int post_receive();
    int post_send(enum ibv_wr_opcode opcode);
    int poll_completion();
    int poll_completion(struct ibv_wc &wc);
    int poll_completion(struct ibv_wc *wc);

    int peek_status(struct ibv_wc *wc);

public: //temp
    struct resources *get_context() { return &res; }
    NPRDMAPreConnector *get_pre_connector() { return connector; }

private:
    int modify_qp_to_rts(struct ibv_qp *qp);
    int modify_qp_to_rtr(struct ibv_qp *qp,
                         uint32_t remote_qpn,
                         uint16_t dlid,
                         uint8_t *dgid);
    int modify_qp_to_init(struct ibv_qp *qp);
    bool create_event_channel();

private:
    Config config;
    struct resources res;
    NPRDMAPreConnector *connector = nullptr;
};

#endif