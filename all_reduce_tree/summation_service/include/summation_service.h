#ifndef __NEWPLAN_SUMMATION_SERVICE_H__
#define __NEWPLAN_SUMMATION_SERVICE_H__

#include <cstring>
#include <stdlib.h>
#include <iostream>
#include "util/spsc_queue.h"
#include <vector>
#include <thread>
#include <glog/logging.h>

#define MAX_NUM_ELEMENT 1000 * 1000 * 100
#define MAX_NUM_BLOCK 20

#define SHOWN_LOG_EVERN_N 991

namespace summation
{

    enum DataType
    {
        FLOAT32 = 28,
        FLOAT64 = 29,
        DOUBLE = 30,
        INT32 = 31,
        INT64 = 32,
        UINT32 = 33,
        UINT64 = 34,
    };

    struct TaskInfo
    {
        int agg_width;      /*汇聚的宽度*/
        int length_in_byte; /*单个数据块长度*/
        void **data;        /*存放数据和结果， [0,1,...,n]-->[0]*/
    };

    using TaskQueue = NEWPLAN::SPSCQueue<struct TaskInfo>;

    class SummationService
    {
    public:
        SummationService();
        virtual ~SummationService()
        {
            if (bg_thread)
                bg_thread->join();
        }

    public:
        void *register_memory(uint64_t mem_size, enum DataType dtype = FLOAT32);
        void deregister_memory(void *, enum DataType dtype = FLOAT32);
        void reduce_sum(float **, int, size_t, enum DataType dtype = FLOAT32);
        void *get_mem_buffer(int index = 0);

    public:
        void flush_cache(void *ptr, size_t size);
        void flush_mem(void);
        void checksum(float *, int, float,
                      enum DataType dtype = FLOAT32);
        void test_case(float **, int, int);
        void test_examples()
        {
            summation::SummationService *s_sum = new summation::SummationService();

            float **tensor_group = new float *[MAX_NUM_BLOCK];
            for (int index = 0; index < MAX_NUM_BLOCK; index++)
            {
                void *tmp_data = s_sum->register_memory(MAX_NUM_ELEMENT * sizeof(float));
                tensor_group[index] = (float *)tmp_data;
            }

            s_sum->start_service_async();
            do
            {
                for (int block_num = 2; block_num < 16; block_num++)
                {
                    for (int data_length = 1000; data_length <= MAX_NUM_ELEMENT; data_length *= 10)
                    {
                        s_sum->submited_compute_task(block_num, data_length * sizeof(float),
                                                     (void **)tensor_group);
                        s_sum->sync_compute_task(0);
                    }
                }
            } while (true);
        }

    public: //interface
            // 通过submited_compute_task提交任务
            // 通过sync_compute_task同步任务
            // 通过peek_compute_task查询任务
        inline int submited_compute_task(int agg_width,
                                         int length_in_byte,
                                         void **data_)
        {
            LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "submitting task:"
                                                 << agg_width << " * "
                                                 << length_in_byte / sizeof(float);
            int token = 0;
            struct TaskInfo task_info = {.agg_width = agg_width,
                                         .length_in_byte = length_in_byte,
                                         .data = data_};

            comm_channel[0]->push(std::move(task_info));
            return token;
        }
        inline void sync_compute_task(int token)
        {
            LOG_EVERY_N(INFO, SHOWN_LOG_EVERN_N) << "synchronizing task";
            token++;
            comm_channel[1]->blocking_pop();
            return;
        }
        inline bool peek_compute_task(int token)
        {
            token++;
            if (comm_channel[1]->front() == nullptr)
                return false;
            comm_channel[1]->pop();
            return true;
        }

        void start_service_async();

    private:
        void random_rw_mem();

    private:
        //int PARALLEL_THREAD = 10;
        void *mem_group[1024] = {0};
        bool available[1024] = {false};
        int num_allocated = 0;

        std::vector<TaskQueue *> comm_channel;
        std::thread *bg_thread = nullptr;
    };
} // namespace summation
#endif /* __NEWPLAN_SUMMATION_SERVICE_H__*/