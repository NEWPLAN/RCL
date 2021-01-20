#include "summation_service.h"

#define MAX_DATA 600 * 1000 * 1000
#define MAX_TENSOR_NUM 16

int main(int argc, char **argv)
{
    if (argc < 2)
        printf("Usage:%s uses default settings\n", argv[0]);
    //new_api();

    summation::SummationService *s_sum = new summation::SummationService();

    float **tensor_group = new float *[MAX_TENSOR_NUM];
    for (int index = 0; index < MAX_TENSOR_NUM; index++)
    {
        void *tmp_data = s_sum->register_memory(MAX_DATA * sizeof(float));
        tensor_group[index] = (float *)tmp_data;
    }

    int current_data = 64e6;
    int current_block = 2;
    do
    {
        if (current_data > MAX_DATA)
            current_data = 1000;
        if (current_block > MAX_TENSOR_NUM)
            current_block = 2;

        for (int index = 0; index < MAX_TENSOR_NUM; index++)
        {
            std::fill_n(tensor_group[index], MAX_DATA, 1);
        }

        s_sum->flush_mem();
        s_sum->test_case(tensor_group, current_block, current_data);
        ++current_block;
        //current_data *= 2;
    } while (1);
    return 0;

    delete s_sum;
}
