#include "../computation/aggregator.h"
#include "../utils/blockingQueue.h"
#include<vector>
#include<iostream>
#include<cstring>

int main(int argc, char const *argv[])
{   

    if (argc > 2)
    {
        std::cout<<"usage: "<<argv[0]<<" <threads>"<<std::endl;
        exit(-1);
    }
    int threads = 10;
    if (argc == 2) threads = atoi(argv[1]);
    if (threads < 1 || threads > 28)
    {
        std::cout<<"invalid number of threads";
        exit(-1);
    }

    // warm up
    const int tmp_size = 2<<30 / sizeof(float); // 1 GiB float
    float* tmp = new float [tmp_size];
    for (int i = 0; i!= tmp_size; i++) tmp[i] = tmp[i] + tmp[0];
    memset(tmp, 0, tmp_size * sizeof(float));
    //delete[] tmp;

    ///* 测试方式: 共用一个 Aggregator. 
    Aggregator* agg = new Aggregator();
    BlockingQueue<int> *signal_queue = new BlockingQueue<int>();
    BlockingQueue<int> *control_channel_ = new BlockingQueue<int>();
    BlockingQueue<int> *data_channel_ = new BlockingQueue<int>();
    std::vector<BlockingQueue<int> *> aggregator_channels;
    aggregator_channels.push_back(data_channel_);
    aggregator_channels.push_back(control_channel_);

    agg->setup(100 * 1000 * 1000, 16, threads); 
    agg->register_signal_event(signal_queue);
    agg->setup_channels(aggregator_channels);
    agg->run();

//for (int i = 0; i != 12; i++)
{
    for (int num_tensor = 2; num_tensor <= 16; num_tensor++)
    {
        //for (int block_size = 1000; block_size <= 100000000; block_size *= 10) //
        int block_size = 10e6;
        {
            data_channel_->push(num_tensor);
            data_channel_->push(block_size);
            control_channel_->pop();
        }
        //std::this_thread::sleep_for(std::chrono::seconds(2)); //时间间隔
    }
    //std::this_thread::sleep_for(std::chrono::seconds(2));
}
    //*/

    /* 测试方式: 每次构造一个新的 Aggregator
    for (int num_tensor = 2; num_tensor <= 15; num_tensor++)
    {
        //for (int block_size = 1000; block_size <= 100000000; block_size *= 10)
        int block_size = 10e6;
        {
            Aggregator* agg = new Aggregator();
            BlockingQueue<int> *signal_queue = new BlockingQueue<int>();
            BlockingQueue<int> *control_channel_ = new BlockingQueue<int>();
            BlockingQueue<int> *data_channel_ = new BlockingQueue<int>();
            std::vector<BlockingQueue<int> *> aggregator_channels;
            aggregator_channels.push_back(data_channel_);
            aggregator_channels.push_back(control_channel_);


            agg->setup(100 * 1000 * 1000, num_tensor, 10); //why can't use num_tensor-1?
            agg->register_signal_event(signal_queue);
            agg->setup_channels(aggregator_channels);
            agg->run();
            
            
            data_channel_->push(num_tensor);
            data_channel_->push(block_size);
            control_channel_->pop();
            agg->~Aggregator();
        }
    }
    */
}
