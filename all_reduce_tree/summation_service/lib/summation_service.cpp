#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "summation_service.h"
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <unordered_map>
#include "util/timer_record.h"
#include <omp.h>

namespace summation
{

    SummationService::SummationService()
    {
        memset(mem_group, 0, 1024 * sizeof(void *));
        for (int index = 0; index < 1024; index++)
            available[index] = false;

        TaskQueue *in_queue = new TaskQueue(1024);
        TaskQueue *out_queue = new TaskQueue(1024);
        comm_channel.push_back(in_queue);
        comm_channel.push_back(out_queue);
    }

    void *SummationService::register_memory(uint64_t mem_size, enum DataType dtype)
    {
        if (dtype != FLOAT32)
            printf("[Warning]: current can use FLOAT32 for data type\n");

        if (mem_size <= 0 || mem_size > 0xFFFFFFFF)
        {
            printf("[Error]: invalid memory size: %lu\n", mem_size);
            exit(EXIT_FAILURE);
        }

        void *buf = 0;
        int ret = 0;
        if (0 != (ret = posix_memalign(&buf, sysconf(_SC_PAGESIZE), mem_size)))
        {
            printf("[error] posix_memalign: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        mem_group[num_allocated] = buf;
        available[num_allocated++] = true;

        memset(buf, 0, mem_size); //clean all the mem buf
        printf("Having allocated %lu bytes : %d\n", mem_size, num_allocated);

        return buf;
    }
    void SummationService::deregister_memory(void *dataptr, enum DataType dtype)
    {
        if (dtype != FLOAT32)
            printf("[Warning]: current can use FLOAT32 for data type\n");
        for (int index = 0; index < num_allocated; index++)
        {
            if (dataptr == mem_group[index])
            {
                if (available[index] == false)
                {
                    printf("[Warning]: mem index: %d has already been freed\n", index);
                }
                else
                {
                    free(dataptr);
                }
                available[index] = false;
                break;
            }
        }
    }
    void SummationService::reduce_sum(float **src, int num_blocks, size_t num_elements, enum DataType dtype)
    {
        if (dtype != FLOAT32)
            printf("[Warning]: current can use FLOAT32 for data type\n");
#define PARALLEL_THREAD 14
        float *dst = src[0];
        float *src0 = src[0];
        float *src1 = src[1];
        float *src2 = src[2];
        float *src3 = src[3];
        float *src4 = src[4];
        float *src5 = src[5];
        float *src6 = src[6];
        float *src7 = src[7];
        float *src8 = src[8];
        float *src9 = src[9];
        float *src10 = src[10];
        float *src11 = src[11];
        float *src12 = src[12];
        float *src13 = src[13];
        float *src14 = src[14];
        float *src15 = src[15];
        float *src16 = src[16];
        float *src17 = src[17];
        float *src18 = src[18];
        float *src19 = src[19];

        switch (num_blocks)
        {
        case 2:
        {
#pragma omp parallel for simd num_threads(PARALLEL_THREAD)
            for (size_t i = 0; i < num_elements; ++i)
            {
                dst[i] = src0[i] + src1[i];
            }
            break;
        }
        case 3:
        {
#pragma omp parallel for simd num_threads(PARALLEL_THREAD)
            for (size_t i = 0; i < num_elements; ++i)
            {
                dst[i] = src0[i] + src1[i] + src2[i];
            }
            break;
        }
        case 4:
        {
#pragma omp parallel for simd num_threads(PARALLEL_THREAD)
            for (size_t i = 0; i < num_elements; ++i)
            {
                dst[i] = src0[i] + src1[i] + src2[i] + src3[i];
            }
            break;
        }
        case 5:
        {
#pragma omp parallel for simd num_threads(PARALLEL_THREAD)
            for (size_t i = 0; i < num_elements; ++i)
            {
                dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i];
            }
            break;
        }
        case 6:
        {
#pragma omp parallel for simd num_threads(PARALLEL_THREAD)
            for (size_t i = 0; i < num_elements; ++i)
            {
                dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i];
            }
            break;
        }
        case 7:
        {
#pragma omp parallel for simd num_threads(PARALLEL_THREAD)
            for (size_t i = 0; i < num_elements; ++i)
            {
                dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i];
            }
            break;
        }
        case 8:
        {
#pragma omp parallel for simd num_threads(PARALLEL_THREAD)
            for (size_t i = 0; i < num_elements; ++i)
            {
                dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i];
            }
            break;
        }
        case 9:
        {
#pragma omp parallel for simd num_threads(PARALLEL_THREAD)
            for (size_t i = 0; i < num_elements; ++i)
            {
                dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i] + src8[i];
            }
            break;
        }
        case 10:
        {
#pragma omp parallel for simd num_threads(PARALLEL_THREAD)
            for (size_t i = 0; i < num_elements; ++i)
            {
                dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i] + src8[i] + src9[i];
            }
            break;
        }
        case 11:
        {
#pragma omp parallel for simd num_threads(PARALLEL_THREAD)
            for (size_t i = 0; i < num_elements; ++i)
            {
                dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i] + src8[i] + src9[i] + src10[i];
            }
            break;
        }
        case 12:
        {
#pragma omp parallel for simd num_threads(PARALLEL_THREAD)
            for (size_t i = 0; i < num_elements; ++i)
            {
                dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i] + src8[i] + src9[i] + src10[i] + src11[i];
            }
            break;
        }
        case 13:
        {
#pragma omp parallel for simd num_threads(PARALLEL_THREAD)
            for (size_t i = 0; i < num_elements; ++i)
            {
                dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i] + src8[i] + src9[i] + src10[i] + src11[i] + src12[i];
            }
            break;
        }
        case 14:
        {
#pragma omp parallel for simd num_threads(PARALLEL_THREAD)
            for (size_t i = 0; i < num_elements; ++i)
            {
                dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i] + src8[i] + src9[i] + src10[i] + src11[i] + src12[i] + src13[i];
            }
            break;
        }
        case 15:
        {
#pragma omp parallel for simd num_threads(PARALLEL_THREAD)
            for (size_t i = 0; i < num_elements; ++i)
            {
                dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i] + src8[i] + src9[i] + src10[i] + src11[i] + src12[i] + src13[i] + src14[i];
            }
            break;
        }
        case 16:
        {
#pragma omp parallel for simd num_threads(PARALLEL_THREAD)
            for (size_t i = 0; i < num_elements; ++i)
            {
                dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i] + src8[i] + src9[i] + src10[i] + src11[i] + src12[i] + src13[i] + src14[i] + src15[i];
            }
            break;
        }
        case 17:
        {
#pragma omp parallel for simd num_threads(PARALLEL_THREAD)
            for (size_t i = 0; i < num_elements; ++i)
            {
                dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i] + src8[i] + src9[i] + src10[i] + src11[i] + src12[i] + src13[i] + src14[i] + src15[i] + src16[i];
            }
            break;
        }
        case 18:
        {
#pragma omp parallel for simd num_threads(PARALLEL_THREAD)
            for (size_t i = 0; i < num_elements; ++i)
            {
                dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i] + src8[i] + src9[i] + src10[i] + src11[i] + src12[i] + src13[i] + src14[i] + src15[i] + src16[i] + src17[i];
            }
            break;
        }
        case 19:
        {
#pragma omp parallel for simd num_threads(PARALLEL_THREAD)
            for (size_t i = 0; i < num_elements; ++i)
            {
                dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i] + src8[i] + src9[i] + src10[i] + src11[i] + src12[i] + src13[i] + src14[i] + src15[i] + src16[i] + src17[i] + src18[i];
            }
            break;
        }
        case 20:
        {
#pragma omp parallel for simd num_threads(PARALLEL_THREAD)
            for (size_t i = 0; i < num_elements; ++i)
            {
                dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i] + src8[i] + src9[i] + src10[i] + src11[i] + src12[i] + src13[i] + src14[i] + src15[i] + src16[i] + src17[i] + src18[i] + src19[i];
            }
            break;
        }
        default:
            std::cerr << "Unknown num_blocks: " << num_blocks << std::endl;
            break;
        }
    }

    void *SummationService::get_mem_buffer(int index)
    {
        if (index < 0 || index >= num_allocated)
        {
            printf("[Error]: cannot get memory buffer out of range, with index: %d\n", index);
            return nullptr;
        }
        if (available[index] == false)
            printf("[Error]: cannot get freed memory buffer, with index: %d\n", index);

        return mem_group[index];
    }

    void SummationService::random_rw_mem()
    {

#define BYTE_MEM (1024 * 1024 * 100)
        float *res = new float[BYTE_MEM];
        float *src = new float[BYTE_MEM];
        float *dst = new float[BYTE_MEM];

        if (res == nullptr || src == nullptr || dst == nullptr)
        {
            printf("Error of malloc memory\n");
            return;
        }

        for (int index = 0; index < BYTE_MEM; index++)
        {
            res[index] = src[index] + dst[index];
        }

        delete[] res;
        delete[] src;
        delete[] dst;
    }

    void SummationService::flush_cache(void *ptr, size_t allocation_size)
    {
        const size_t cache_line = 64;
        const char *cp = (const char *)ptr;
        size_t i = 0;

        if (ptr == NULL || allocation_size <= 0)
            return;

        for (i = 0; i < allocation_size; i += cache_line)
        {
            asm volatile("clflush (%0)\n\t"
                         :
                         : "r"(&cp[i])
                         : "memory");
        }

        asm volatile("sfence\n\t"
                     :
                     :
                     : "memory");
        // if (!cacheflush(ptr, allocation_size, BCACHE))
        // {
        //     printf("error to flush cache\n");
        // }
    }
#include <glog/logging.h>
    void SummationService::flush_mem(void)
    {
#pragma omp parallel num_threads(56)
        for (int index = 0; index < 3; index++)
        {
            //LOG(INFO) << "Thread ID: " << omp_get_thread_num() << ", " << index;
            //std::cout << "Thread ID: " << omp_get_thread_num() << ", " << index << std::endl;
            random_rw_mem();
        }
    }

    void SummationService::checksum(float *res, int length, float val, enum DataType dtype)
    {
        if (dtype != FLOAT32)
            printf("[Warning]: current can use FLOAT32 for data type\n");

        for (int index = 0; index < length; index++)
        {
            if (res[index] != val)
            {
                std::cerr << "Error of check sum: " << res[index]
                          << ", index: " << index << std::endl;
                exit(0);
            }
        }
    }

    void SummationService::test_case(float **tensor_group, int num_data, int MAX_DATA)
    {
        Timer tm_record;
        flush_mem();
        static uint64_t iter = 0;
        int round = iter / 21;
        //printf("Round: %d, iter: %lu\n", round, iter);
        static float history_tm_cost[1000][100];
        //if (iter == 0 && num_data == 2)
        if (iter == 0)
        {
            for (int index = 0; index < 1000; index++)
            {
                for (int y_index = 0; y_index < 100; y_index++)
                    history_tm_cost[index][y_index] = 0;
            }
        }

        static std::unordered_map<int, int> size_index_map;
        static int global_index = 0;
        static int current_index = 0;

        {
            auto index_map = size_index_map.find(MAX_DATA);
            if (index_map == size_index_map.end())
            {
                size_index_map.insert(std::make_pair(MAX_DATA, global_index));
                current_index = global_index++;
            }
            else
            {
                current_index = index_map->second;
            }
            //printf("%d --> %d\n", MAX_DATA, current_index);
        }

        if (num_data < 2 || num_data > 20)
        {
            std::cerr << "Error of num data [2,20]: " << num_data << std::endl;
            exit(0);
        }
        tm_record.Start();
        reduce_sum(tensor_group, num_data, MAX_DATA);
        tm_record.Stop();

        uint64_t time_cost = tm_record.MicroSeconds();

        history_tm_cost[num_data][current_index] = (history_tm_cost[num_data][current_index] * (round) + time_cost * 1.0) / (round + 1);

        printf("Total time cost [%10.2d K floats *%2.d] %8.lu us, %8.2f, averaged %8.lu us, %8.2f, [%4lu: %8.2f ms]\n",
               MAX_DATA / 1000, num_data - 1,
               time_cost, time_cost / 1000.0,
               time_cost / (num_data - 1), time_cost / 1000.0 / (num_data - 1),
               iter, history_tm_cost[num_data][current_index] / 1000.0 / (num_data - 1));

        checksum(tensor_group[0], MAX_DATA, num_data * 1.0f);

        //if (num_data == 20)
        {
            iter++;
            //printf("\n\n");
        }
    }

    void SummationService::start_service_async()
    {
        if (bg_thread != nullptr)
        {
            std::cout << "[Warning] launching service failed;" << std::endl;
            return;
        }

        bg_thread = new std::thread([=]() {
            TaskQueue *task_queue = comm_channel[0];
            TaskQueue *fin_queue = comm_channel[1];
            do
            {
                struct TaskInfo task_info;
                task_info = task_queue->blocking_pop();
                float **tensor_group = (float **)task_info.data;
                reduce_sum(tensor_group, task_info.agg_width,
                           task_info.length_in_byte / sizeof(float));
                fin_queue->push(std::move(task_info));
            } while (true);
        });
    }
} // namespace summation