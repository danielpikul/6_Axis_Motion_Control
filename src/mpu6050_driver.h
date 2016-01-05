#ifndef MPU6050_DRIVER
#define MPU6050_DRIVER

//#include <sys/types.h>

#define PWR_MNG   0x6B

#define MPU_ADDR 0x3B


struct mpu6050_data{

	int16_t gx;
	int16_t gy;
	int16_t gz;

	int16_t ax;
	int16_t ay;
	int16_t az;

}; 

static __inline__ unsigned long long rdtsc(void)
{
	unsigned long long x;
	__asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
	return x;	
}

#endif





