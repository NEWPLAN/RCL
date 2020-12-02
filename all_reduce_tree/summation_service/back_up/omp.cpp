#include <iostream>
#include <omp.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include <iostream>

void function_test()
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

void flush_mem()
{
#pragma omp parallel num_threads(56)
	for (int index = 0; index < 3; index++)
	{
		//std::cout<<"Thread ID: "<<omp_get_thread_num()<<", "<<index<<std::endl;
		function_test();
	}
}

void checksum(float *res, int length, float val)
{
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

class Timer
{
public:
	Timer() : initted_(false),
			  running_(false),
			  has_run_at_least_once_(false)
	{
		Init();
	}
	~Timer() {}
	void Start()
	{
		if (!running())
		{
			start_cpu_ = std::chrono::steady_clock::now();
			//gettimeofday(&t_start_, NULL);
			running_ = true;
			has_run_at_least_once_ = true;
		}
	}
	void Stop()
	{
		if (running())
		{
			stop_cpu_ = std::chrono::steady_clock::now();
			//gettimeofday(&t_stop_, NULL);
			running_ = false;
		}
	}
	uint64_t MilliSeconds()
	{
		if (!has_run_at_least_once())
		{
			std::cout << "Timer has never been run before reading time." << std::endl;
			return 0;
		}
		if (running())
		{
			Stop();
		}

		elapsed_milliseconds_ = std::chrono::duration_cast<std::chrono::milliseconds>(stop_cpu_ - start_cpu_).count();
		return elapsed_milliseconds_;
	}
	uint64_t MicroSeconds()
	{
		if (!has_run_at_least_once())
		{
			std::cout << "Timer has never been run before reading time." << std::endl;
			return 0;
		}
		if (running())
		{
			Stop();
		}
		//std::cout << t_stop_.tv_sec - t_start_.tv_sec + t_stop_.tv_usec - t_start_.tv_usec << " us[sys], ";

		elapsed_microseconds_ = std::chrono::duration_cast<std::chrono::microseconds>(stop_cpu_ - start_cpu_).count();
		return elapsed_microseconds_;
	}
	uint64_t NanoSeconds()
	{
		if (!has_run_at_least_once())
		{
			std::cout << "Timer has never been run before reading time." << std::endl;
			return 0;
		}
		if (running())
		{
			Stop();
		}

		elasped_nanoseconds_ = std::chrono::duration_cast<std::chrono::nanoseconds>(stop_cpu_ - start_cpu_).count();
		return elasped_nanoseconds_;
	}
	uint64_t Seconds()
	{
		if (!has_run_at_least_once())
		{
			std::cout << "Timer has never been run before reading time." << std::endl;
			return 0;
		}
		if (running())
		{
			Stop();
		}

		elasped_seconds_ = std::chrono::duration_cast<std::chrono::seconds>(stop_cpu_ - start_cpu_).count();
		return elasped_seconds_;
	}

	inline bool initted() { return initted_; }
	inline bool running() { return running_; }
	inline bool has_run_at_least_once() { return has_run_at_least_once_; }

protected:
	void Init()
	{
		if (!initted())
		{
			initted_ = true;
		}
	}

	bool initted_;
	bool running_;
	bool has_run_at_least_once_;

	std::chrono::steady_clock::time_point start_cpu_;
	std::chrono::steady_clock::time_point stop_cpu_;
	//struct timeval t_start_, t_stop_;
	uint64_t elapsed_milliseconds_;
	uint64_t elapsed_microseconds_;
	uint64_t elasped_nanoseconds_;
	uint64_t elasped_seconds_;
};

inline void reduce_sum(float **src, int tensor_num, int length)
{
#define THREAD_NUM 14
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

	switch (tensor_num)
	{
	case 2:
	{
#pragma omp parallel for simd num_threads(THREAD_NUM)
		for (size_t i = 0; i < length; ++i)
		{
			dst[i] = src0[i] + src1[i];
		}
		break;
	}
	case 3:
	{
#pragma omp parallel for simd num_threads(THREAD_NUM)
		for (size_t i = 0; i < length; ++i)
		{
			dst[i] = src0[i] + src1[i] + src2[i];
		}
		break;
	}
	case 4:
	{
#pragma omp parallel for simd num_threads(THREAD_NUM)
		for (size_t i = 0; i < length; ++i)
		{
			dst[i] = src0[i] + src1[i] + src2[i] + src3[i];
		}
		break;
	}
	case 5:
	{
#pragma omp parallel for simd num_threads(THREAD_NUM)
		for (size_t i = 0; i < length; ++i)
		{
			dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i];
		}
		break;
	}
	case 6:
	{
#pragma omp parallel for simd num_threads(THREAD_NUM)
		for (size_t i = 0; i < length; ++i)
		{
			dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i];
		}
		break;
	}
	case 7:
	{
#pragma omp parallel for simd num_threads(THREAD_NUM)
		for (size_t i = 0; i < length; ++i)
		{
			dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i];
		}
		break;
	}
	case 8:
	{
#pragma omp parallel for simd num_threads(THREAD_NUM)
		for (size_t i = 0; i < length; ++i)
		{
			dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i];
		}
		break;
	}
	case 9:
	{
#pragma omp parallel for simd num_threads(THREAD_NUM)
		for (size_t i = 0; i < length; ++i)
		{
			dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i] + src8[i];
		}
		break;
	}
	case 10:
	{
#pragma omp parallel for simd num_threads(THREAD_NUM)
		for (size_t i = 0; i < length; ++i)
		{
			dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i] + src8[i] + src9[i];
		}
		break;
	}
	case 11:
	{
#pragma omp parallel for simd num_threads(THREAD_NUM)
		for (size_t i = 0; i < length; ++i)
		{
			dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i] + src8[i] + src9[i] + src10[i];
		}
		break;
	}
	case 12:
	{
#pragma omp parallel for simd num_threads(THREAD_NUM)
		for (size_t i = 0; i < length; ++i)
		{
			dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i] + src8[i] + src9[i] + src10[i] + src11[i];
		}
		break;
	}
	case 13:
	{
#pragma omp parallel for simd num_threads(THREAD_NUM)
		for (size_t i = 0; i < length; ++i)
		{
			dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i] + src8[i] + src9[i] + src10[i] + src11[i] + src12[i];
		}
		break;
	}
	case 14:
	{
#pragma omp parallel for simd num_threads(THREAD_NUM)
		for (size_t i = 0; i < length; ++i)
		{
			dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i] + src8[i] + src9[i] + src10[i] + src11[i] + src12[i] + src13[i];
		}
		break;
	}
	case 15:
	{
#pragma omp parallel for simd num_threads(THREAD_NUM)
		for (size_t i = 0; i < length; ++i)
		{
			dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i] + src8[i] + src9[i] + src10[i] + src11[i] + src12[i] + src13[i] + src14[i];
		}
		break;
	}
	case 16:
	{
#pragma omp parallel for simd num_threads(THREAD_NUM)
		for (size_t i = 0; i < length; ++i)
		{
			dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i] + src8[i] + src9[i] + src10[i] + src11[i] + src12[i] + src13[i] + src14[i] + src15[i];
		}
		break;
	}
	case 17:
	{
#pragma omp parallel for simd num_threads(THREAD_NUM)
		for (size_t i = 0; i < length; ++i)
		{
			dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i] + src8[i] + src9[i] + src10[i] + src11[i] + src12[i] + src13[i] + src14[i] + src15[i] + src16[i];
		}
		break;
	}
	case 18:
	{
#pragma omp parallel for simd num_threads(THREAD_NUM)
		for (size_t i = 0; i < length; ++i)
		{
			dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i] + src8[i] + src9[i] + src10[i] + src11[i] + src12[i] + src13[i] + src14[i] + src15[i] + src16[i] + src17[i];
		}
		break;
	}
	case 19:
	{
#pragma omp parallel for simd num_threads(THREAD_NUM)
		for (size_t i = 0; i < length; ++i)
		{
			dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i] + src8[i] + src9[i] + src10[i] + src11[i] + src12[i] + src13[i] + src14[i] + src15[i] + src16[i] + src17[i] + src18[i];
		}
		break;
	}
	case 20:
	{
#pragma omp parallel for simd num_threads(THREAD_NUM)
		for (size_t i = 0; i < length; ++i)
		{
			dst[i] = src0[i] + src1[i] + src2[i] + src3[i] + src4[i] + src5[i] + src6[i] + src7[i] + src8[i] + src9[i] + src10[i] + src11[i] + src12[i] + src13[i] + src14[i] + src15[i] + src16[i] + src17[i] + src18[i] + src19[i];
		}
		break;
	}
	default:
		std::cerr << "Unknown tensor_num: " << tensor_num << std::endl;
		break;
	}
}

#include <unordered_map>
//#include<iostream>
inline void test_case(float **tensor_group, int num_data, int MAX_DATA)
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
	/*printf("iter:%lu,round: %d ,index: %d, origin: %.2f, new val: %.2f, latest: %.2f\n",
	       iter, round, num_data, history_tm_cost[num_data][current_index], time_cost * 1.0,
	       (history_tm_cost[num_data][current_index] * (round ) + time_cost) / (round  + 1));
	*/
	history_tm_cost[num_data][current_index] = (history_tm_cost[num_data][current_index] * (round) + time_cost * 1.0) / (round + 1);
	//history_tm_cost[num_data] = (history_tm_cost[num_data] * (iter ) + time_cost * 1.0) / (iter  + 1);
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

inline void test_case_old(float **tensor_group, int num_data, int MAX_DATA)
{
	Timer tm_record;
	flush_mem();
	static uint64_t iter = 0;
	static float history_tm_cost[1024];
	if (iter == 0 && num_data == 2)
	{
		for (int index = 0; index < 1000; index++)
			history_tm_cost[index] = 0;
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
	//printf("iter:%lu, index: %d, origin: %.2f, new val: %.2f, latest: %.2f\n", iter, num_data, history_tm_cost[num_data], time_cost * 1.0, (history_tm_cost[num_data] * (iter ) + time_cost) / (iter  + 1));
	history_tm_cost[num_data] = (history_tm_cost[num_data] * (iter) + time_cost * 1.0) / (iter + 1);
	printf("Total time cost [%.2d M floats *%2.d] %8.lu us, %8.2f, averaged %8.lu us, %8.2f, [%4lu: %8.2f ms]\n",
		   MAX_DATA / 1000 / 1000, num_data - 1,
		   time_cost, time_cost / 1000.0,
		   time_cost / (num_data - 1), time_cost / 1000.0 / (num_data - 1),
		   iter, history_tm_cost[num_data] / 1000.0 / (num_data - 1));

	checksum(tensor_group[0], MAX_DATA, num_data * 1.0f);

	if (num_data == 20)
	{
		iter++;
		printf("\n\n");
	}
}

inline int myfunction()
{
#pragma omp parallel num_threads(6)
	{
		printf("index %d\n", omp_get_thread_num());
		printf("world %d\n", omp_get_thread_num());
	}

	return 1;
}

int main(int argc, char **argv)
{
#define MAX_DATA 1000 * 2000 * 1000
#define MAX_TENSOR_NUM 3

	return myfunction();

	float **tensor_group = new float *[MAX_TENSOR_NUM];
	for (int index = 0; index < MAX_TENSOR_NUM; index++)
	{
		tensor_group[index] = new float[MAX_DATA];
	}
	int round = 0;
	int current_data = 1000;
	do
	{
		if (current_data > MAX_DATA)
			current_data = 1000;

		for (int index = 0; index < MAX_TENSOR_NUM; index++)
		{
			std::fill_n(tensor_group[index], MAX_DATA, 1);
		}

		flush_mem();
		//test_case(tensor_group, 2 + ((round++) % 19), MAX_DATA);
		test_case(tensor_group, 2, current_data);
		current_data *= 2;
	} while (1);
	return 0;
}
