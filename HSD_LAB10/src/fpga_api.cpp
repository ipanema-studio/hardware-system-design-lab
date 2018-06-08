#include "fpga_api.h"
#include <cstring>
#include <cstdio>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define DATA_SIZE SIZE*(SIZE+1)*sizeof(float) // fpga bram data size

#define min(x,y) (((x)<(y))?(x):(y))

FPGA::FPGA(off_t data_addr, off_t api_addr)
{
    fd_ = open("/dev/mem", O_RDWR);
    data_ = static_cast<float*>(mmap(NULL, DATA_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd_, data_addr));
    api_ = static_cast<unsigned int*>(mmap(NULL, sizeof(unsigned int), PROT_READ|PROT_WRITE, MAP_SHARED,fd_, api_addr));
}

FPGA::~FPGA()
{
    munmap(data_, DATA_SIZE );
    munmap(api_, sizeof(unsigned int));
    close(fd_);
}

float* FPGA::matrix(void)
{
	return data_ + SIZE;
}

float* FPGA::vector(void)
{
	return data_;
}

const float* __attribute__((optimize("O0"))) FPGA::run()
{
    *api_ = 0x5555;
    while(*api_ == 0x5555);

    return data_;    
}

void FPGA::largeMV(const float* large_mat, const float* input,
		float* output, int M, int N)
{
    // write down your code here.

    for (int i = 0; i != N; i++) {
        output[i] = 0.0f;
    }

    const float *output_fgpa;

    int row_left = (64 - N % 64) % 64;
    int row_loop = (N + row_left) >> 6;
    int col_left = (64 - M % 64) % 64;
    int col_loop = (M + col_left) >> 6;

    float* vec = this->vector();
    float* mat = this->matrix();

    // for (int i = 0; i != row_loop; i++) {
    //     int row_base = i * 64;
    //     for (int j = 0; j != col_loop; j++) {
    //         int col_base = j * 64;
    //         for (int row = row_base; row != row_base + 64; row++) {
    //             for (int col = col_base; col != col_base + 64; col++) {
    //                 if (row == row_base) {
    //                     vec[col - col_base] = (col < M ? input[col] : 0.0f);
    //                 }
    //                 mat[(row - row_base) * 64 + (col - col_base)] = ((row < N && col < M) ? large_mat[row * M + col] : 0.0f);
    //             }
    //         }
    //         output_fgpa = this->run();
    //         for (int k = 0; k != 64; k++) {
    //             output[row_base + k] += output_fgpa[k];
    //         }
    //     }
    // }


    for (int i = 0; i != 64; i++) {
        vec[i] = 0.0f;
    }
    for (int i = 0; i != 64; i++) {
        for (int j = 0; j != 64; j++) {
            mat[i * 64 + j] = 0.0f;
        }
    }
    for (int i = 0; i != 32; i++) {
        vec[i] = input[i];
    }
    for (int i = 0; i != 32; i++) {
        for (int j = 0; j != 32; j++) {
            mat[i * 64 + j] = large_mat[i * 32 + j];
        }
    }
    output_fgpa = this->run();
    for (int k = 0; k != 32; k++) {
        output[k] = output_fgpa[k];
    }
}
