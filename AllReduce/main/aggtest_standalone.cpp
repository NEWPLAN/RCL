#include <iostream>
#include <thread>
#include<cmath>
#include<ctime>
#include<cstring>
#include <vector>
#include <chrono>

class Timer
{
public:
    Timer(void) {}
    ~Timer(void) {}

public:
    void start() { time_start = std::chrono::steady_clock::now(); }
    void stop()
    {
        time_stop = std::chrono::steady_clock::now();
        time_used = std::chrono::duration_cast<std::chrono::duration<double>>(time_stop - time_start);
    }
    double milliseconds()
    {
        return time_used.count() * 1000;
    }
    double microseconds() { return time_used.count() * 1000 * 1000; }
    double seconds() { return time_used.count(); }

private:
    std::chrono::steady_clock::time_point time_start, time_stop;
    std::chrono::duration<double> time_used;
};

class Aggregator
{
public:
    Aggregator();
    ~Aggregator();
    void setup(int block_size, int tensor_num);
    void run(int tensor_num_, int tensor_size_);
private:
    int block_size;
    int tensor_num;
    std::vector<std::vector<float *>> data_groups;
};

Aggregator::Aggregator()
{
    std::cout << "Aggregator has been ctreated" << std::endl;
}
Aggregator::~Aggregator()
{
    for (int index = 0; index < data_groups.size(); index++)
    {
        for (auto &data_ptr : data_groups[index])
        {
            delete data_ptr;
        }
    }
    std::cout << "Destroying Aggregator engine" << std::endl;
}

void Aggregator::setup(int block_size, int tensor_num)
{
    this->block_size = block_size;
    this->tensor_num = tensor_num;

    //dx: warmup
    for (int i = 0; i < 12; i++)
    {
        const int tmp_size = 2<<32 / sizeof(float); // 4 GiB float
        float* tmp = new float [tmp_size];
        tmp[0] = 3.1415926535;
        for (int i = 0; i!= tmp_size; i++) tmp[i] = tmp[i] + tmp[0];
        memset(tmp, 0, tmp_size * sizeof(float));
        delete[] tmp;
    }
    std::cout<< "Threads warmup complete.\n";
    //dx: end warm up

    for (int data_round = 0; data_round < 3; data_round++)
    {
        std::vector<float *> data_unit;
        for (int tensor_index = 0; tensor_index < tensor_num + 1; tensor_index++)
        {
            float *tmp_data = new float[block_size + 10];
            if (tmp_data == nullptr)
            {
                std::cerr << "Error in malloc data" << std::endl;
                exit(-1);
            }
            data_unit.push_back(tmp_data);
        }
        this->data_groups.push_back(std::move(data_unit));
    }

    std::cout << "Malloc CPU memory size: " << block_size << " B * " << tensor_num + 1 << ", with backups: " << this->data_groups.size() << std::endl;
}

static void reduce(std::vector<float *> &data_, int num_tensor, int start, int stop)
{
    float *result = data_[num_tensor];
    float *data[160];
    for (int index = 0; index < data_.size(); index++)
        data[index] = data_[index];
    switch (num_tensor)
    {
    case 1:
        for (int i = start; i < stop; i++)
            result[i] = data[0][i];
        break;
    case 2:
        for (int i = start; i < stop; i++)
            result[i] = data[0][i] + data[1][i];
        break;
    case 3:
        for (int i = start; i < stop; i++)
            result[i] = data[0][i] + data[1][i] + data[2][i];
        break;
    case 4:
        for (int i = start; i < stop; i++)
            result[i] = data[0][i] + data[1][i] + data[2][i] + data[3][i];
        break;
    case 5:
        for (int i = start; i < stop; i++)
            result[i] = data[0][i] + data[1][i] + data[2][i] + data[3][i] + data[4][i];
        break;
    case 6:
        for (int i = start; i < stop; i++)
            result[i] = data[0][i] + data[1][i] + data[2][i] + data[3][i] + data[4][i] + data[5][i];
        break;
    case 7:
        for (int i = start; i < stop; i++)
            result[i] = data[0][i] + data[1][i] + data[2][i] + data[3][i] + data[4][i] + data[5][i] + data[6][i];
        break;
    case 8:
        for (int i = start; i < stop; i++)
            result[i] = data[0][i] + data[1][i] + data[2][i] + data[3][i] + data[4][i] + data[5][i] + data[6][i] + data[7][i];
        break;
    case 9:
        for (int i = start; i < stop; i++)
            result[i] = data[0][i] + data[1][i] + data[2][i] + data[3][i] + data[4][i] + data[5][i] + data[6][i] + data[7][i] + data[8][i];
        break;
    case 10:
        for (int i = start; i < stop; i++)
            result[i] = data[0][i] + data[1][i] + data[2][i] + data[3][i] + data[4][i] + data[5][i] + data[6][i] + data[7][i] + data[8][i] + data[9][i];
        break;
    case 11:
        for (int i = start; i < stop; i++)
            result[i] = data[0][i] + data[1][i] + data[2][i] + data[3][i] + data[4][i] + data[5][i] + data[6][i] + data[7][i] + data[8][i] + data[9][i] + data[10][i];
        break;
    case 12:
        for (int i = start; i < stop; i++)
            result[i] = data[0][i] + data[1][i] + data[2][i] + data[3][i] + data[4][i] + data[5][i] + data[6][i] + data[7][i] + data[8][i] + data[9][i] + data[10][i] + data[11][i];
        break;
    case 13:
        for (int i = start; i < stop; i++)
            result[i] = data[0][i] + data[1][i] + data[2][i] + data[3][i] + data[4][i] + data[5][i] + data[6][i] + data[7][i] + data[8][i] + data[9][i] + data[10][i] + data[11][i] + data[12][i];
        break;
    case 14:
        for (int i = start; i < stop; i++)
            result[i] = data[0][i] + data[1][i] + data[2][i] + data[3][i] + data[4][i] + data[5][i] + data[6][i] + data[7][i] + data[8][i] + data[9][i] + data[10][i] + data[11][i] + data[12][i] + data[13][i];
        break;
    case 15:
        for (int i = start; i < stop; i++)
            result[i] = data[0][i] + data[1][i] + data[2][i] + data[3][i] + data[4][i] + data[5][i] + data[6][i] + data[7][i] + data[8][i] + data[9][i] + data[10][i] + data[11][i] + data[12][i] + data[13][i] + data[14][i];
        break;
    case 16:
        for (int i = start; i < stop; i++)
            result[i] = data[0][i] + data[1][i] + data[2][i] + data[3][i] + data[4][i] + data[5][i] + data[6][i] + data[7][i] + data[8][i] + data[9][i] + data[10][i] + data[11][i] + data[12][i] + data[13][i] + data[14][i] + data[15][i];
        break;
    default:
        std::cout << "error in unknown tensors: " << num_tensor << std::endl;
        break;
    }
}

void Aggregator::run(int tensor_num_, int tensor_size_)
{
    int data_round = 0;
    if (tensor_size_ > 100000000)
    {
        std::cout << "too much tensor size: " << tensor_size_ << std::endl;
        exit(-1);
    }
    if (tensor_num_ > 16)
    {
        std::cout << "too many tensor num: " << tensor_num_ << std::endl;
        exit(-1);
    }

    Timer t1;
    t1.start();
    {
        reduce(this->data_groups[data_round], tensor_num_, 0, tensor_size_);
    }
    t1.stop();
    //std::cout << "num tensor: " << tensor_num_ << ", size: " << tensor_size_ / 1000.0 << " KB, Time cost: " << t1.milliseconds() << " ms" << std::endl;
    std::cout <<tensor_num_ << " " << tensor_size_ / 1000.0 << " " << t1.milliseconds() <<std::endl;
    data_round = (data_round + 1) % 3;
}

int main()
{
    // warm up
    const int tmp_size = 2<<30 / sizeof(float); // 1 GiB float
    float* tmp = new float [tmp_size];
    for (int i = 0; i!= tmp_size; i++) tmp[i] = tmp[i] + tmp[0];
    memset(tmp, 0, tmp_size * sizeof(float));
    //delete[] tmp;

    Aggregator* agg = new Aggregator();
    agg->setup(100 * 1000 * 1000, 16); 
    for (int num_tensor = 2; num_tensor <= 16; num_tensor++)
    {
        //for (int block_size = 1000; block_size <= 100000000; block_size *= 10) //
        int block_size = 10e6;
        {
            agg->run(num_tensor, block_size);
        }
        //std::this_thread::sleep_for(std::chrono::seconds(2)); //时间间隔
    }
}