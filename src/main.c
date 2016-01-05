#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <math.h>
#include <linux/input.h>

#include "max7219_user.h"
#include "mpu6050_driver.h"
#include "game_patterns.h"
#include "math_functions.h"
#include "game_schedules.h"

/*Timing Clocks*/
unsigned long long disp_begin, disp_end,
                   comp_begin, comp_end;

/*Joystick info*/
#define KEYFILE "/dev/input/event2"
int g_js_fd;
struct input_event js_ie;

/*Game variables*/
#define SECS_TO_WIN   3
#define US_TO_WIN     SECS_TO_WIN*1000000
#define MAX_VEL       6 //for playability


struct ball {
	double px;
	double py;
	int led_px;
	int led_py;
	double vx;
	double vy;
	double ax;
	double ay;
	double tx;
	double ty;
} global_ball;

struct spi_ioc_transfer spi_tx;

//global mutex
pthread_mutex_t lock;

volatile int program_active;
volatile int game_in_progress;
int win_state;
long win_counter;
double max_accel;
double calc_ball_per;

//game functions
void set_difficulty();
void init_ball();
void sig_handler(int sig_num);

//display functions
void display_level();
void display_start_screen();
void display_ball();
void display_win_screen();
void display_lose_screen();

//user threads
void *max7219_display_thread(void* arg);
void *mpu6050_print_thread(void* arg);
void *mpu6050_read_thread(void * arg);
void *compute_thread(void * arg);

int difficulty; //game difficulty 1-10
int g_mpu6050_fd;

/*Global Gyroscope Data*/
struct mpu6050_data global_mpu6050_data;

void main (int argc, char *argv[])
{
	int ret;
	program_active = 1;
	difficulty     = 1; //default
	win_state      = 0;
	calc_ball_per  = 1/((double)GET_DATA); //period is the inverse of the MPU6050 sample rate
	
	signal(SIGINT, sig_handler);

	if(argc > 2) {
		printf("usage: %s starting_difficulty(1-10)\n", argv[0]);
		return;
	}
	else if(argc == 2) {
		difficulty = atoi(argv[1]);
		if(difficulty > 10 || difficulty < 1) {
			printf("Please enter a valid difficulty.\n");
			return;
		}
	}

	set_difficulty();	//set the initial difficulty

	/*Open the MPU6050 file.*/
	g_mpu6050_fd = open("/dev/mpu6050", O_RDWR);
	if(g_mpu6050_fd < 0) {
		printf("MAIN: Error opening MPU-6050 file.\n");
		close(g_mpu6050_fd);
		return;
	}

	/*Open input susbsystem*/
	g_js_fd= open(KEYFILE, O_RDONLY);
	if(g_js_fd < 0) {
		printf("MAIN: Error opening joystick events file.\n");
		close(g_mpu6050_fd);
		return;
	}

	/*Initialize the LED board.*/
	gpio_setup();	//initalize gpio pins (42,43,55)
	ret = init_MAX7219(&spi_tx); //initialize the max7219 module
	if(ret < 0) {
		gpio_unexport();
		close(g_mpu6050_fd);
	}

	init_ball();
	display_start_screen();

	/*initialize the mutex*/
	pthread_mutex_init(&lock, NULL);
	
	/*Create the program threads.*/
	pthread_t thread_id[4];
	pthread_create(&thread_id[0], NULL, max7219_display_thread, NULL);
	//pthread_create(&thread_id[1], NULL, mpu6050_read_thread, (void *)g_mpu6050_fd);
	pthread_create(&thread_id[1], NULL, compute_thread, (void *)g_mpu6050_fd);
	//pthread_create(&thread_id[2], NULL, mpu6050_print_thread, (void *)g_mpu6050_fd);         
	game_in_progress = 1;

	/*Prevent the game from ending until Ctrl-C is entered.*/
	int i;
	for(i=0; i < 2; i++) {
		pthread_join(thread_id[i], NULL);
	}

	game_in_progress = 0;
	program_active = 0;

	gpio_unexport(); //unexport gpios (42,43,55)

	close(g_mpu6050_fd);
}

void* mpu6050_read_thread(void * arg) {
	int mpu6050_fd;
	int ret;

	uint8_t buf[14];

	mpu6050_fd = (int)arg;

	ret = write(mpu6050_fd,
				NULL,
			 	6);
	if(ret < 0)
		printf("Write Failed.\n");

	while(program_active) {
		//printf("read waiting\n");
		pthread_mutex_lock(&lock);

		ret = read(mpu6050_fd, 
			       &buf,
			       14);

		global_mpu6050_data.ax = (buf[0] << 8)  | buf[1];
		global_mpu6050_data.ay = (buf[2] << 8)  | buf[3];
		global_mpu6050_data.az = (buf[4] << 8)  | buf[5];

		global_mpu6050_data.gx = (buf[8] << 8)  | buf[9];
		global_mpu6050_data.gy = (buf[10] << 8) | buf[11];
		global_mpu6050_data.gz = (buf[12] << 8) | buf[13];

		if(ret < 0)
			printf("MAIN: MPU-6050 Read Failed.\n");
		
		pthread_mutex_unlock(&lock);
		usleep(GET_DATA_US);
	}
	
	pthread_exit(NULL);
}


//thread function that handles ultrasonic sensor
void *mpu6050_print_thread(void* arg){

	while(program_active) {
		while(game_in_progress) {
			pthread_mutex_lock(&lock);
			printf("\nX Degrees: %f\n Y Degrees: %f\n", global_ball.tx, global_ball.ty);	
			printf("\nX Accel: %f\n Y Accel: %f\n", global_ball.ax, global_ball.ay);
			printf("X Vel: %f\n Y Vel: %f\n", global_ball.vx, global_ball.vy);
			printf("X POS: %f\n Y POS: %f\n", global_ball.px, global_ball.py);
			pthread_mutex_unlock(&lock);
			usleep(PRINT_DATA_US);
		}
	}
	pthread_exit(NULL);
}

void sig_handler(int sig_num) {
	if(sig_num == SIGINT) {
		printf("\nCtrl-C Entered. Releasing pins and exiting game.\n");
		program_active = 0;
		display_pattern(pattern_clear, &spi_tx);
		gpio_unexport();
		close(g_mpu6050_fd);	
		exit(0);
	}
}


/*************************DISPLAY FUNCTIONS*************************/
//thread function that handles display
void *max7219_display_thread(void* arg)
{
	while(program_active) {
		pthread_mutex_lock(&lock);
		display_level();
		pthread_mutex_unlock(&lock);
		while(game_in_progress) {
			pthread_mutex_lock(&lock);
			//disp_begin = rdtsc();
			display_ball();
			//disp_end = rdtsc();
			//printf("Time Spent Updating the display: %f\n", (double)(disp_end - disp_begin) / 400000000);
			pthread_mutex_unlock(&lock);
			usleep(DISP_BALL_US);
		}
		pthread_mutex_lock(&lock);
		if(win_state < 0) {
			display_lose_screen();
			game_in_progress = 1;
		}
		else if(win_state == 1) {
			display_win_screen();
			game_in_progress = 1;
		}
		pthread_mutex_unlock(&lock);
	}
	display_pattern(pattern_clear, &spi_tx);
	pthread_exit(NULL);
} 

void display_level() {
	int i;
	for (i = 0; i < 3; ++i)
	{
		display_pattern(numerals[difficulty-1], &spi_tx);
		sleep(1);
		display_pattern(pattern_clear, &spi_tx);
		sleep(1);
	}
}

void display_win_screen() {
	int i;
	for(i = 0; i < 4; i++) {
		display_pattern(win_screen[i], &spi_tx);
		sleep(1);
	}
}

void display_lose_screen() {
	int secs;
	for(secs = 0; secs < 3; secs++) {
		display_pattern(pattern_X, &spi_tx);
		sleep(1);
		display_pattern(pattern_clear, &spi_tx);
		sleep(1);
	}
}

void display_start_screen(){
	int blinks;
	for(blinks = 0; blinks < 4; blinks++) {
		display_pattern(pattern_start, &spi_tx);
		sleep(1);
		display_pattern(pattern_clear, &spi_tx);
		sleep(1);		
	}
}

void display_ball() {
	//use global ball position to draw a frame to be passed to the screen
	uint8_t bp [8];
	int i;
	
	for(i=0;i<8;i++) {
		if(i == global_ball.led_px) {
			bp[i] = 1 << global_ball.led_py;			
		}
		else
			bp[i] = 0x00;
	}
	display_pattern(bp, &spi_tx);
}

int mm_to_led(double pos_mm) {
	int led_pos;
	if(pos_mm < 4)
		return 0;
	else if(pos_mm < 8)
		return 1;
	else if(pos_mm < 12)
		return 2;
	else if(pos_mm < 16)
		return 3;
	else if(pos_mm < 20)
		return 4;
	else if(pos_mm < 24)
		return 5;
	else if(pos_mm < 28)
		return 6;
	else
		return 7;
}


/*************************GAME FUNCTIONS*************************/
void init_ball() {
	global_ball.vx     = 0;
	global_ball.vy     = 0;
	global_ball.ax     = 0;
	global_ball.ay     = 0;

	//randomize start position
	do {
		global_ball.px = rand() % 32;
	}while(global_ball.px < 4 | global_ball.px > 27 | (global_ball.px < 20 & global_ball.px > 11));
	do {	
		global_ball.py     = rand() % 32;
	}while(global_ball.px < 4 | global_ball.px > 27 | (global_ball.px < 20 & global_ball.px > 11));

	global_ball.led_px = mm_to_led((double)global_ball.px);
	global_ball.led_py = mm_to_led((double)global_ball.py);
}

void set_difficulty() {

	switch(difficulty) {
		case 1:
			max_accel = ACCEL_GRAV_PLUTO;
			printf("\nDifficulty 1. Balance on pluto.\nAcceleration due to gravity is %fmm/s^2.\n", max_accel);
		break;
		case 2:
			max_accel = ACCEL_GRAV_MOON;
			printf("\nDifficulty 2. Balance on moon.\nAcceleration due to gravity is %fmm/s^2.\n", max_accel);
		break;
		case 3:
			max_accel = ACCEL_GRAV_MARS;
			printf("\nDifficulty 3. Balance on Mars.\nAcceleration due to gravity is %fmm/s^2.\n", max_accel);
		break;
		case 4:
			max_accel = ACCEL_GRAV_URANUS;
			printf("\nDifficulty 4. Balance on Uranus.\nAcceleration due to gravity is %fmm/s^2.\n", max_accel);
		break;
		case 5:
			max_accel = ACCEL_GRAV_VENUS;
			printf("\nDifficulty 5. Balance on Venus.\nAcceleration due to gravity is %fmm/s^2.\n", max_accel);
		break;
		case 6:
			max_accel = ACCEL_GRAV_EARTH;
			printf("\nDifficulty 6. Balance on Earth.\nAcceleration due to gravity is %fmm/s^2.\n", max_accel);
		break;
		case 7:
			max_accel = ACCEL_GRAV_SATURN;
			printf("\nDifficulty 7. Balance on Saturn.\nAcceleration due to gravity is %fmm/s^2.\n", max_accel);
		break;
		case 8:
			max_accel = ACCEL_GRAV_NEPTUNE;
			printf("\nDifficulty 8. Balance on Neptune.\nAcceleration due to gravity is %fmm/s^2.\n", max_accel);
		break;
		case 9:
			max_accel = ACCEL_GRAV_JUPITER;
			printf("\nDifficulty 9. Balance on Jupiter.\nAcceleration due to gravity is %fmm/s^2.\n", max_accel);
		break;
		case 10:
			max_accel = ACCEL_GRAV_SUN;
			printf("\nDifficulty 10. Balance on Sun.\nAcceleration due to gravity is %fmm/s^2.\n", max_accel);
		break;
		default:
			max_accel = ACCEL_GRAV_EARTH;
			printf("Defaulting back to earth.\n");
		break;
	}

}

void *compute_thread(void * arg) {
	int ret;
	int i;
	double rads_x, rads_y;
	while(program_active) {
		while(game_in_progress) {
			pthread_mutex_lock(&lock);
			//read latest data
			//comp_begin = rdtsc();
			do {
				ret = read(g_js_fd, &js_ie, sizeof(struct input_event));
				if(ret < 0) {
					printf("MAIN: Read failure from event file.\n");
				}
				switch(js_ie.code) {

					case ABS_THROTTLE :
						global_mpu6050_data.ax = (int16_t)js_ie.value;
					break;
					case ABS_RUDDER :	
						global_mpu6050_data.ay = (int16_t)js_ie.value;		
					break;
					case ABS_WHEEL :
						global_mpu6050_data.az = (int16_t)js_ie.value;
					break;
					case ABS_RX :
						global_mpu6050_data.gx = (int16_t)js_ie.value;
					break;
					case ABS_RY :
						global_mpu6050_data.gy = (int16_t)js_ie.value;			
					break;
					case ABS_RZ :
						global_mpu6050_data.gz = (int16_t)js_ie.value;
					break;
					default:
					break;
				}
				
			}while(js_ie.code != 0);

			//calc new degrees
			comp_filter(global_mpu6050_data.ax, global_mpu6050_data.ay, global_mpu6050_data.az, 
						global_mpu6050_data.gx, global_mpu6050_data.gy, global_mpu6050_data.gz,
						&global_ball.tx, &global_ball.ty, calc_ball_per);		

			rads_x = (global_ball.tx * PI) / 180.0;
			rads_y = (global_ball.ty * PI) / 180.0;

			//calc new acceleration

			global_ball.ax = calc_accel_mms2(rads_x, max_accel);
			global_ball.ay = calc_accel_mms2(rads_y, max_accel);
			global_ball.ax = global_ball.ax/15;
			global_ball.ay = global_ball.ay/15;

			//calc new velocity
			global_ball.vx = calc_vel_mms(global_ball.ax, global_ball.vx, calc_ball_per);	
			global_ball.vy = calc_vel_mms(global_ball.ay, global_ball.vy, calc_ball_per);

			if(global_ball.vx < 0) {
				if(global_ball.vx < -MAX_VEL)
					global_ball.vx = -MAX_VEL;
			}
			else if(global_ball.vx > MAX_VEL) {
				global_ball.vx = MAX_VEL;
			}

			if(global_ball.vy < 0) {
				if(global_ball.vy < -MAX_VEL)
					global_ball.vy = -MAX_VEL;
			}
			else if(global_ball.vy > MAX_VEL) {
				global_ball.vy = MAX_VEL;
			}

			//calc new position (and velocity incase of impact)
			global_ball.px     = calc_pos(global_ball.px, &global_ball.vx, calc_ball_per);
			global_ball.py     = calc_pos(global_ball.py, &global_ball.vy, calc_ball_per);
			global_ball.led_px = mm_to_led(global_ball.px);
			global_ball.led_py = mm_to_led(global_ball.py);

			/*Calculate win state based on refresh rate of this function, or ball out of bounds*/
			if(global_ball.px > 31 | global_ball.px < 0 | global_ball.py > 31 | global_ball.py < 0) {
				win_state = -1;       //signal that game is lost
				game_in_progress = 0; //end current game
				init_ball();
				printf("You lose! Restarting Level.\n");
				printf("Press Ctrl-C at any time to exit the game.\n");
			}		
			else if(global_ball.px < 20 & global_ball.px > 11 & global_ball.py < 20 & global_ball.py > 11)
				win_counter += CALC_BALL_US;
			else
				win_counter = 0;

			//check if the win counter has reached enough time
			if(win_counter >= US_TO_WIN) {
				win_state = 1;
				game_in_progress = 0;
				win_counter = 0;
				difficulty++;
				if(difficulty > 10) {
					printf("Congrats! You beat all 10 levels!\n");
					program_active = 0;
				}
				else {
					printf("You won! Going to next level.\n");
					printf("Press Ctrl-C at any time to exit the game.\n");
					set_difficulty();
				}
				init_ball(); //re-initialize the ball
			}
			//comp_end = rdtsc();
			//printf("Time Spent Computing Ball Physics: %f\n", (double)(comp_end - comp_begin) / 400000000);
			pthread_mutex_unlock(&lock);
			usleep(CALC_BALL_US);
		}
	}
	pthread_exit(NULL);
}


















