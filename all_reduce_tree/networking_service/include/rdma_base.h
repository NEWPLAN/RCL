#ifndef __NEWPLAN_RDMA_BASE_H__
#define __NEWPLAN_RDMA_BASE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include <endian.h>
#include <byteswap.h>
#include <getopt.h>

#include <sys/time.h>
#include <arpa/inet.h>
#include <infiniband/verbs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint64_t htonll(uint64_t x) { return bswap_64(x); }
static inline uint64_t ntohll(uint64_t x) { return bswap_64(x); }
#elif __BYTE_ORDER == __BIG_ENDIAN
static inline uint64_t htonll(uint64_t x) { return x; }
static inline uint64_t ntohll(uint64_t x) { return x; }
#else
#error __BYTE_ORDER is neither __LITTLE_ENDIAN nor __BIG_ENDIAN
#endif

/* structure to exchange data which is needed to connect the QPs */
struct cm_con_data_t
{
    uint64_t addr;   /* Buffer address */
    uint32_t rkey;   /* Remote key */
    uint32_t qp_num; /* QP number */
    uint16_t lid;    /* LID of the IB port */
    uint8_t gid[16]; /* gid */
} __attribute__((packed));

/* structure of system resources */
struct resources
{
    struct ibv_device_attr device_attr; /* Device attributes */
    struct ibv_port_attr port_attr;     /* IB port attributes */
    struct cm_con_data_t remote_props;  /* values to connect to remote side */
    struct ibv_context *ib_ctx;         /* device handle */
    struct ibv_comp_channel *channel;   /*event channel*/
    struct ibv_pd *pd;                  /* PD handle */
    struct ibv_cq *cq;                  /* CQ handle */
    struct ibv_qp *qp;                  /* QP handle */
    struct ibv_mr *mr;                  /* MR handle for buf */
    struct ibv_mr *ctrl_mr;             /* MR handle for buf */
    char *buf;                          /* memory buffer pointer, used for RDMA and send ops */
    char *ctrl_buf;                     /*control buf*/
    int sock;                           /* TCP socket file descriptor */
    int current_block;                  /*cur block info*/
    int signal_all;                     /*signal all =false*/
};

#endif