#include "zynq.h"

#include <cstring>
#include <sys/time.h>

#include <cstdio>	//for perror
#include <sys/mman.h>	//mmap

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#define BRAM_BASE 0x40000000

#define MATRIX_ADDR BRAM_BASE
#define IPT_VECTOR_ADDR (MATRIX_ADDR + (MATRIX_SIZE * MATRIX_SIZE * sizeof(uint32_t)))
#define OPT_VECTOR_ADDR (IPT_VECTOR_ADDR + MATRIX_SIZE * sizeof(uint32_t))

#define INSTRUCTION_ADDR 0x43C00000
#define MAGIC_CODE 0x5555

static inline float f16_to_f32(const uint32_t *input);

double fpga_calculate(uint32_t *ipt_matrix_f16, uint32_t *ipt_vector_f16, float *your_vector_f32)
{
	//Map BRAM to virtual memory space and copy data.
	
	struct timeval start, end;
	gettimeofday(&start, NULL);

	//Run IP and copy value to DRAM space
	for (int i = 0; i != 64; i++)
		your_vector_f32[i] = 0;

	int foo;
	foo = open("/dev/mem", O_RDWR | O_NONBLOCK);
	uint32_t *fpga_bram = (uint32_t *)mmap(NULL, 128 * sizeof(uint32_t), PROT_WRITE, MAP_SHARED, foo, BRAM_BASE);
	for (int i = 0; i != 64; i++) {
		*(fpga_bram + i) = ipt_vector_f16[i];
	}
	for (int i = 64; i != 128; i++) {
		*(fpga_bram + i) = ipt_matrix_f16[(i - 64)];
	}

	unsigned int *fpga_ip = (unsigned int *)mmap(NULL, sizeof(int), PROT_WRITE, MAP_SHARED, foo, INSTRUCTION_ADDR);
	*fpga_ip = MAGIC_CODE;
	std::cout << "hello" << std::endl;
	while (*fpga_ip == MAGIC_CODE);

	float result;
	memcpy(&result, fpga_bram, sizeof(float));
	std::cout << result << std::endl;
	
	gettimeofday(&end, NULL);
	
	//Cleanup allocated resources (optional).

	return (double)(end.tv_sec - start.tv_sec) + ((double)(end.tv_usec - start.tv_usec)) / 1000000.0;
}


double arm_calculate(uint32_t *ipt_matrix_f16, uint32_t *ipt_vector_f16, float *arm_vector_f32)
{
	struct timeval start, end;
	gettimeofday(&start, NULL);

	for(size_t i = 0 ; i < matrix_size ; i++)
	{
		arm_vector_f32[i] = 0;
		float vector_element = f16_to_f32(ipt_vector_f16 + i);
		for(size_t j = 0 ; !(j & (1 << SIZE_SHIFTER)) ; arm_vector_f32[i]
			+= vector_element * f16_to_f32(ipt_matrix_f16 + (i << SIZE_SHIFTER) + (j++)));
	}

	gettimeofday(&end, NULL);
	return (double)(end.tv_sec - start.tv_sec) + ((double)(end.tv_usec - start.tv_usec)) / 1000000.0;
}

static inline float f16_to_f32(const uint32_t *input)
{
	char *iptc = (char*)input + 2;

	uint32_t half_precision = 0;
	memcpy(&half_precision, iptc + 1, 1);
	memcpy(((void*)&half_precision) + 1, iptc, 1);

	/*uint32_t half_precision = 0;
	memcpy(&half_precision, input, sizeof(float));*/
	
	uint32_t opt 	= ((half_precision & 0x8000) << 16)
			| ((((half_precision & 0x7C00) >> 10) + 112) << 23)
			| ((half_precision & 0x3FF) << 13);

	float casted_opt;
	memcpy(&casted_opt, &opt, sizeof(float));
	return casted_opt;
}
