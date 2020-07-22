#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <inttypes.h>
#include "common.h"

// In a list of IB devices (dev_list), given a IB device's name
// (ib_dev_name), the function returns its ID.
static inline int ib_dev_id_by_name(char *ib_dev_name, 
                                    struct ibv_device **dev_list, 
                                    int num_devices) 
{
    for (int i = 0; i < num_devices; i++) {
        if (strcmp(ibv_get_device_name(dev_list[i]), ib_dev_name) == 0) {
            return i;
        }
    }

    return -1;
}

bool init_ctx(struct read_lat_context *ctx)
{
    if (!ctx) {
        goto err;
    }

    struct ibv_device **dev_list = NULL; 
    int num_devices;
    
    // Get IB device list
    dev_list = ibv_get_device_list(&num_devices);
    if (!dev_list) {
        fprintf(stderr, "Fail to get IB device list\n");
        goto err;

    } else if (num_devices == 0) {
        fprintf(stderr, "No IB devices found\n");
        goto clean_dev_list;
    }

    int ib_dev_id = -1;
    ib_dev_id = ib_dev_id_by_name(ctx->ib_dev_name, dev_list, num_devices);
    if (ib_dev_id < 0) {
        fprintf(stderr, "Fail to find IB device %s\n", ctx->ib_dev_name);
        goto clean_dev_list;
    }

    // Create a context for the RDMA device 
    ctx->ctx = ibv_open_device(dev_list[ib_dev_id]);
    if (ctx->ctx) {
        printf("Open IB device %s\n", ibv_get_device_name(dev_list[ib_dev_id]));

    } else {
        fprintf(stderr, "Fail to open IB device %s\n", ibv_get_device_name(dev_list[ib_dev_id]));
        goto clean_dev_list;
    }

    // Create a completion channel
    if (ctx->use_event) {
        ctx->channel = ibv_create_comp_channel(ctx->ctx);
        if (!(ctx->channel)) {
            fprintf(stderr, "Cannot create completion channel\n");
            goto clean_device;
        }
    } else {
        ctx->channel = NULL;
    }

    // Allocate protection domain 
    ctx->pd = ibv_alloc_pd(ctx->ctx);
    if (!(ctx->pd)) {
        fprintf(stderr, "Fail to allocate protection domain\n");
        goto clean_comp_channel;
    }

    // Allocate memory for control plane messages
    ctx->ctrl_buf = (char*)memalign(sysconf(_SC_PAGESIZE), ctx->ctrl_buf_size);
    if (!(ctx->ctrl_buf)) {
        fprintf(stderr, "Fail to allocate memory for control plane messagees\n");
        goto clean_pd;
    } 

    // Allocate memory for data plane messages
    ctx->data_buf = (char*)memalign(sysconf(_SC_PAGESIZE), ctx->data_buf_size);
    if (!(ctx->data_buf)) {
        fprintf(stderr, "Fail to allocate memory for data plane messagees\n");
        goto clean_ctrl_buf;
    } 

    // Register memory region for control plane messages.
    int access_flags = IBV_ACCESS_LOCAL_WRITE;
    ctx->ctrl_mr = ibv_reg_mr(ctx->pd, ctx->ctrl_buf, ctx->ctrl_buf_size, access_flags);
    if (!(ctx->ctrl_mr)) {
        fprintf(stderr, "Fail to register memory region for control plane messages\n");
        goto clean_data_buf;
    }

    // Register memory region for data plane messages.
    access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ; 
    ctx->data_mr = ibv_reg_mr(ctx->pd, ctx->data_buf, ctx->data_buf_size, access_flags);
    if (!(ctx->data_mr)) {
        fprintf(stderr, "Fail to register memory region for data plane messages\n");
        goto clean_ctrl_mr;
    }

    // Query device attributes 
    if (ibv_query_device(ctx->ctx, &(ctx->dev_attr)) != 0) {
        fprintf(stderr, "Fail to query device attributes\n");
        goto clean_data_mr;
    }

    // Query port attributes
    if (ibv_query_port(ctx->ctx, ctx->dev_port, &(ctx->port_attr)) != 0) {
        fprintf(stderr, "Fail to query port attributes\n");
        goto clean_data_mr;
    }

    // Create a completion queue
    ctx->cq = ibv_create_cq(ctx->ctx, ctx->dev_attr.max_cqe, NULL, ctx->channel, 0);
    if (!(ctx->cq)) {
        fprintf(stderr, "Fail to create the completion queue\n");
        goto clean_data_mr;
    }

    if (ctx->use_event) {
        if (ibv_req_notify_cq(ctx->cq, 0)) {
            fprintf(stderr, "Cannot request CQ notification\n");
            goto clean_cq;
        }
    }
    // Create a queue pair (QP)
    struct ibv_qp_attr attr;
    struct ibv_qp_init_attr init_attr = {
        .send_cq = ctx->cq,
        .recv_cq = ctx->cq,
        .cap = {
            .max_send_wr = ctx->dev_attr.max_qp_wr,
            .max_recv_wr = ctx->dev_attr.max_qp_wr,
            .max_send_sge = 1,
            .max_recv_sge = 1,
        },
        .qp_type = IBV_QPT_RC,
    };

    ctx->qp = ibv_create_qp(ctx->pd, &init_attr);
    if (!(ctx->qp)) {
        fprintf(stderr, "Fail to create QP\n");
        goto clean_cq;
    }
    
    ctx->send_flags = IBV_SEND_SIGNALED;

    attr.qp_state        = IBV_QPS_INIT;
    attr.pkey_index      = 0;
    attr.port_num        = ctx->dev_port;
    attr.qp_access_flags = IBV_ACCESS_REMOTE_READ;

    if (ibv_modify_qp(ctx->qp, &attr,
				      IBV_QP_STATE          |
				      IBV_QP_PKEY_INDEX     |
				      IBV_QP_PORT           |
				      IBV_QP_ACCESS_FLAGS)) {
		
        fprintf(stderr, "Fail to modify QP to INIT\n");
        goto clean_qp;
    }


    ibv_free_device_list(dev_list);
    return true;

clean_qp:
    ibv_destroy_qp(ctx->qp);

clean_cq:
    ibv_destroy_cq(ctx->cq);

clean_data_mr:
    ibv_dereg_mr(ctx->data_mr);

clean_ctrl_mr:
    ibv_dereg_mr(ctx->ctrl_mr);

clean_data_buf:
    free(ctx->data_buf);

clean_ctrl_buf:
    free(ctx->ctrl_buf);

clean_pd:
	ibv_dealloc_pd(ctx->pd);

clean_comp_channel:
    if (ctx->channel) {
        ibv_destroy_comp_channel(ctx->channel);
    }

clean_device:
	ibv_close_device(ctx->ctx);

clean_dev_list:
    ibv_free_device_list(dev_list);
    
err:
    return false;
}

void destroy_ctx(struct read_lat_context *ctx)
{   
    if (!ctx) {
        return;
    }
    
    // Destroy queue pair
    if (ctx->qp) {
        ibv_destroy_qp(ctx->qp);
    }

    // Destroy completion queue
    if (ctx->cq) {
        ibv_destroy_cq(ctx->cq);
    }

    // Un-register memory region
    if (ctx->data_mr) {
        ibv_dereg_mr(ctx->data_mr);
    }

    if (ctx->ctrl_mr) {
        ibv_dereg_mr(ctx->ctrl_mr);
    }

    // Free memory
    if (ctx->data_buf) {
        free(ctx->data_buf);
    }

    if (ctx->ctrl_buf) {
        free(ctx->ctrl_buf);
    }

    // Destroy protection domain
    if (ctx->pd) {
        ibv_dealloc_pd(ctx->pd);
    }

    // Desotry completion channel
    if (ctx->channel) {
        ibv_destroy_comp_channel(ctx->channel);
    }

    // Close RDMA device context
    if (ctx->ctx) {
        ibv_close_device(ctx->ctx);
    }
}

// Get the index of GID whose type is RoCE V2
// Refer to https://docs.mellanox.com/pages/viewpage.action?pageId=12013422#RDMAoverConvergedEthernet(RoCE)-RoCEv2 for more details
int get_rocev2_gid_index(struct read_lat_context *ctx)
{
    int gid_index = 2;

    while (true) {
        FILE * fp;
        char * line = NULL;
        size_t len = 0;
        ssize_t read;
        char file_name[128];

        snprintf(file_name, sizeof(file_name), 
                "/sys/class/infiniband/%s/ports/%d/gid_attrs/types/%d", 
                ctx->ib_dev_name, ctx->dev_port, gid_index);
        
        fp = fopen(file_name, "r");
        if (!fp) {
            break;
        }

        read = getline(&line, &len, fp);
        if (read <= 0) {
            fclose(fp);
            break;
        }
        
        if (strncmp(line, "RoCE v2", strlen("RoCE v2")) == 0) {
            fclose(fp);
            free(line);
            return gid_index;
        } 
        
        fclose(fp);
        free(line);
        gid_index++;
    }

    return DEFAULT_GID_INDEX;
}

// Write exactly 'count' bytes storing in buffer 'buf' into 
// the file descriptor 'fd'.
// Return the number of bytes sucsessfully written.
size_t write_exact(int fd, char *buf, size_t count)
{
    // current buffer loccation
    char *cur_buf = NULL;
    // # of bytes that have been written   
    size_t bytes_wrt = 0;
    int n;

    if (!buf) {
        return 0;
    }

    cur_buf = buf;

    while (count > 0) {
        n = write(fd, cur_buf, count);

        if (n <= 0) {
            fprintf(stderr, "write error\n");
            break;

        } else {
            bytes_wrt += n;
            count -= n;
            cur_buf += n;
        }
    }

    return bytes_wrt;
}

// Read exactly 'count' bytes from the file descriptor 'fd' 
// and store the bytes into buffer 'buf'.
// Return the number of bytes successfully read.
size_t read_exact(int fd, char *buf, size_t count)
{   
    // current buffer loccation
    char *cur_buf = NULL;
    // # of bytes that have been read   
    size_t bytes_read = 0;
    int n;

    if (!buf) {
        return 0;
    }

    cur_buf = buf;

    while (count > 0) {
        n = read(fd, cur_buf, count);

        if (n <= 0) {
            fprintf(stderr, "read error\n");
            break;
            
        } else {
            bytes_read += n;
            count -= n;
            cur_buf += n;
        }
    }

    return bytes_read;
}

static void wire_gid_to_gid(const char *wgid, union ibv_gid *gid)
{
	char tmp[9];
	uint32_t v32;
	int i;

	for (tmp[8] = 0, i = 0; i < 4; ++i) {
		memcpy(tmp, wgid + i * 8, 8);
		sscanf(tmp, "%x", &v32);
		*(uint32_t*)(&gid->raw[i * 4]) = ntohl(v32);
	}
}

static void gid_to_wire_gid(const union ibv_gid *gid, char wgid[])
{
	int i;

	for (i = 0; i < 4; ++i)
		sprintf(&wgid[i * 8], "%08x", htonl(*(uint32_t *)(gid->raw + i * 4)));
}

// Write my destination information to a remote server
bool write_dest_info(int fd, struct read_lat_dest *my_dest)
{
    if (!fd || !my_dest) {
        return false;
    }

    // Message format: "LID : QPN : PSN : GID"
    char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
    char gid[33];

    gid_to_wire_gid(&my_dest->gid, gid);
    sprintf(msg, "%04x:%06x:%06x:%s", my_dest->lid, my_dest->qpn, my_dest->psn, gid);

    if (write_exact(fd, msg, sizeof(msg)) != sizeof(msg)) {
        fprintf(stderr, "Could not send the local address\n");
        return false;
    }

    return true;
}

// Read destination informaiton
bool read_dest_info(int fd, struct read_lat_dest *rem_dest)
{
    if (!fd || !rem_dest) {
        return false;
    }

    // Message format: "LID : QPN : PSN : GID"
    char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
    char gid[33];

    if (read_exact(fd, msg, sizeof(msg)) != sizeof(msg)) {
        fprintf(stderr, "Could not receive the remote address\n");
        return false;
    }   

    sscanf(msg, "%x:%x:%x:%s", &rem_dest->lid, &rem_dest->qpn, &rem_dest->psn, gid);
    wire_gid_to_gid(gid, &rem_dest->gid);

    return true;
}

// Initialize destination information
bool init_dest(struct read_lat_dest *dest, 
               struct read_lat_context *ctx)
{

    if (!dest || !ctx) {
        return false;
    }

    srand48(getpid() * time(NULL));
    // local identifier
    dest->lid = ctx->port_attr.lid;
    // QP number
    dest->qpn = ctx->qp->qp_num;
    // packet sequence number
    dest->psn = lrand48() & 0xffffff;

    // Get the index of GID whose type is RoCE v2
    ctx->gid_index = get_rocev2_gid_index(ctx);
    printf("GID index = %d\n", ctx->gid_index);

    if (ibv_query_gid(ctx->ctx, ctx->dev_port, ctx->gid_index, &(dest->gid)) != 0) {
        fprintf(stderr, "Cannot read my device's GID (GID index = %d)\n", ctx->gid_index);
        return false;
    }

    return true;
}

void print_dest(struct read_lat_dest *dest)
{
    if (!dest) {
        return;
    }

    char gid[33] = {0};
    inet_ntop(AF_INET6, &(dest->gid), gid, sizeof(gid));
    printf("LID 0x%04x, QPN 0x%06x, PSN 0x%06x, GID %s\n",
	       dest->lid, dest->qpn, dest->psn, gid);
}


// Initialize data plane memory information
bool init_data_mem(struct read_lat_mem *mem, 
                   struct read_lat_context *ctx)
{
    if (!mem || !ctx) {
        return false;
    }

    mem->addr = (uint64_t)(ctx->data_mr->addr);
    mem->key = ctx->data_mr->rkey;
    return true;
}

void print_mem(struct read_lat_mem *mem)
{   
    if (!mem) {
        return;
    }

    printf("Addr %" PRIu64 ", Key %" PRIu32 "\n", mem->addr, mem->key);
}

// Post a receive request to receive a control plane message.
// Return true on success.
bool post_ctrl_recv(struct read_lat_context *ctx)
{   
    struct ibv_sge list = {
		.addr	= (uintptr_t)(ctx->ctrl_buf),
		.length = ctx->ctrl_buf_size,
		.lkey	= ctx->ctrl_mr->lkey
	};

    struct ibv_recv_wr wr = {
		.wr_id      = RECV_WRID,
		.sg_list    = &list,
		.num_sge    = 1,
	};

    struct ibv_recv_wr *bad_wr;
    return ibv_post_recv(ctx->qp, &wr, &bad_wr) == 0;
}

// Post a send request to send a control plane message.
// Return true on success.
bool post_ctrl_send(struct read_lat_context *ctx)
{
    struct ibv_sge list = {
		.addr	= (uintptr_t)(ctx->ctrl_buf),
		.length = ctx->ctrl_buf_size,
		.lkey	= ctx->ctrl_mr->lkey
	};

	struct ibv_send_wr wr = {
		.wr_id	    = SEND_WRID,
		.sg_list    = &list,
		.num_sge    = 1,
		.opcode     = IBV_WR_SEND,
        .send_flags = ctx->send_flags
	};

	struct ibv_send_wr *bad_wr;
	return ibv_post_send(ctx->qp, &wr, &bad_wr) == 0;
}

// Post a send request with immediate data.
// Return true on success.
bool post_ctrl_send_with_imm(struct read_lat_context *ctx,
                             unsigned int imm_data)
{
    struct ibv_sge list = {
		.addr	= (uintptr_t)(ctx->ctrl_buf),
		.length = ctx->ctrl_buf_size,
		.lkey	= ctx->ctrl_mr->lkey
	};

	struct ibv_send_wr wr = {
		.wr_id	    = SEND_WITH_IMM_WRID,
		.sg_list    = &list,
		.num_sge    = 1,
		.opcode     = IBV_WR_SEND_WITH_IMM,
        .send_flags = ctx->send_flags,
        .imm_data   = htonl(imm_data)  
	};

	struct ibv_send_wr *bad_wr;
	return ibv_post_send(ctx->qp, &wr, &bad_wr) == 0;
}

// Post a read request to PULL a data plane message
// Return true on success.
bool post_data_read(struct read_lat_context *ctx,
                    struct read_lat_mem *rem_mem)
{
    struct ibv_sge list = {
		.addr	= (uintptr_t)(ctx->data_buf),
		.length = ctx->data_buf_size,
		.lkey	= ctx->data_mr->lkey
	};

	struct ibv_send_wr wr = {
        .wr_id	            = READ_WRID,
        .sg_list            = &list,
        .num_sge            = 1,
        .opcode             = IBV_WR_RDMA_READ,
        .send_flags         = ctx->send_flags,
        .wr.rdma.remote_addr= rem_mem->addr,
        .wr.rdma.rkey       = rem_mem->key 
	};

    struct ibv_send_wr *bad_wr;
    return ibv_post_send(ctx->qp, &wr, &bad_wr) == 0;
}

bool connect_qp(struct read_lat_context *ctx,
                struct read_lat_dest *my_dest,
                struct read_lat_dest *rem_dest)
{
    struct ibv_qp_attr attr = {
		.qp_state		    = IBV_QPS_RTR,
		.path_mtu		    = IBV_MTU_1024,
        // Remote QP number
		.dest_qp_num		= rem_dest->qpn,
        // Packet Sequence Number of the received packets
		.rq_psn			    = rem_dest->psn,
		.max_dest_rd_atomic	= 1,
		.min_rnr_timer		= 12,
        // Address vector
		.ah_attr		    = {
			.is_global	    = 0,
			.dlid		    = rem_dest->lid,
			.sl		        = 0,
			.src_path_bits	= 0,
			.port_num	    = ctx->dev_port
		}
	};

    if (rem_dest->gid.global.interface_id) {
        attr.ah_attr.is_global = 1;
        // Set attributes of the Global Routing Headers (GRH)
        // When using RoCE, GRH must be configured!
		attr.ah_attr.grh.hop_limit = 1;
		attr.ah_attr.grh.dgid = rem_dest->gid;
		attr.ah_attr.grh.sgid_index = ctx->gid_index;
    }

    if (ibv_modify_qp(ctx->qp, &attr,
			          IBV_QP_STATE              |
			          IBV_QP_AV                 |
			          IBV_QP_PATH_MTU           |
			          IBV_QP_DEST_QPN           |
			          IBV_QP_RQ_PSN             |
			          IBV_QP_MAX_DEST_RD_ATOMIC |
			          IBV_QP_MIN_RNR_TIMER)) {
		fprintf(stderr, "Fail to modify QP to RTR\n");
		return false;
	}

    attr.qp_state	    = IBV_QPS_RTS;
    // The minimum time that a QP waits for ACK/NACK from remote QP
	attr.timeout	    = 14;
	attr.retry_cnt	    = 7;
	attr.rnr_retry	    = 7;
	attr.sq_psn	        = my_dest->psn;
	attr.max_rd_atomic  = 1;

    if (ibv_modify_qp(ctx->qp, &attr,
			          IBV_QP_STATE              |
			          IBV_QP_TIMEOUT            |
			          IBV_QP_RETRY_CNT          |
			          IBV_QP_RNR_RETRY          |
			          IBV_QP_SQ_PSN             |
			          IBV_QP_MAX_QP_RD_ATOMIC)) {
		fprintf(stderr, "Failed to modify QP to RTS\n");
		return false;
	}

    return true;
}

bool parse_recv_wc(struct ibv_wc *wc)
{
    if (wc->status != IBV_WC_SUCCESS) {
        fprintf(stderr, "Work request status is %s\n", ibv_wc_status_str(wc->status));
        return false;
    }

    if (wc->opcode != IBV_WC_RECV) {
        fprintf(stderr, "Work request opcode is not IBV_WC_RECV (%d)\n", wc->opcode);
        return false;
    }

    return true;
}

bool parse_send_wc(struct ibv_wc *wc)
{
    if (wc->status != IBV_WC_SUCCESS) {
        fprintf(stderr, "Work request status is %s\n", ibv_wc_status_str(wc->status));
        return false;
    }

    if (wc->opcode != IBV_WC_SEND) {
        fprintf(stderr, "Work request opcode is not IBV_WC_SEND (%d)\n", wc->opcode);
        return false;
    }

    return true;
}

bool parse_read_wc(struct ibv_wc *wc)
{
    if (wc->status != IBV_WC_SUCCESS) {
        fprintf(stderr, "Work request status is %s\n", ibv_wc_status_str(wc->status));
        return false;
    }

    if (wc->opcode != IBV_WC_RDMA_READ) {
        fprintf(stderr, "Work request opcode is not IBV_WC_RDMA_READ (%d)\n", wc->opcode);
        return false;
    }

    return true;
}

// Wait for a completed work request. 
// Return the completed work request in @wc.
// Return true on success.
bool wait_for_wc(struct read_lat_context *ctx, struct ibv_wc *wc)
{
    while (true) {
        int ne = ibv_poll_cq(ctx->cq, 1, wc);
        if (ne < 0) {
            fprintf(stderr, "Fail to poll CQ (%d)\n", ne);
			return false;

        } else if (ne > 0) {
            return true;

        } else {
            continue;
        }           
    }

    // We should never reach here
    return false;
}

