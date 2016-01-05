
#ifndef MATH_FUNCTIONS
#define MATH_FUNCTIONS

#include <stdio.h>
#include <linux/types.h>
#include <sys/types.h>
#include <math.h>

#define PI               3.14159265

#define ACCEL_SENS       16384
#define GYRO_SENS        131
#define BOARD_WIDTH_MM   32
#define LED_COUNT        8
#define LED_WIDTH        BOARD_WIDTH_MM / LED_COUNT

#define ACCEL_GRAV_PLUTO   610
#define ACCEL_GRAV_MOON    1625
#define ACCEL_GRAV_MARS    3700
#define ACCEL_GRAV_URANUS  8870
#define ACCEL_GRAV_VENUS   8872
#define ACCEL_GRAV_EARTH   9810
#define ACCEL_GRAV_SATURN  10445
#define ACCEL_GRAV_NEPTUNE 11150
#define ACCEL_GRAV_JUPITER 24790
#define ACCEL_GRAV_SUN     274000

void comp_filter(int16_t ax, int16_t ay, int16_t az, 
				 int16_t gx, int16_t gy, int16_t gz,
				 double *roll, double *pitch, double period) { 

	//double period = 1/freq;
	double pitch_accel, roll_accel;
	
	//calculate pitch
	*roll  += ((double)gx / GYRO_SENS) * period; 
	*pitch += ((double)gy / GYRO_SENS) * period; 

	//factor in drift
	int approx_force_mag = abs(ax) + abs(ay) + abs(az);
	if(approx_force_mag > ACCEL_SENS && approx_force_mag < 32768) {
		pitch_accel = atan2((double)ax, (double)az) * (180/PI);
		*pitch      = *pitch * 0.98 + pitch_accel * .02;
		roll_accel  = atan2((double)ay, (double)az) * (180/PI);
		*roll      = *roll * 0.98 + roll_accel * .02;
	}

}

/*Returns acceleration in mm/s2 from theta*/
double calc_accel_mms2(double theta, double accel) {

	return (accel*sin(theta));

}

double calc_vel_mms(double accel, double prev_vel, double period) {
	/*calculate new vel based on accel, prev_vel, and freq*/
	double new_vel;
	new_vel = (period * accel + prev_vel);
	return new_vel;
}

double calc_pos(double prev_pos, double * vel, double period) {

	double new_pos;
	
	new_pos = (period * (*vel)) + prev_pos;

	//slow down near board edge for playability 
	if(new_pos < 0.00)
		*vel = 0;    
	else if(new_pos > 31.00)
		*vel = 0; //IMPACT
	/*
	if(new_pos < 0.00)
		new_pos = 0;
	else if(new_pos > 31)
		new_pos = 31.00;
	*/
	return new_pos;
}


#endif
