//scheduling

#ifndef GAME_SCHEDULES
#define GAME_SCHEDULES

/*Scheduler Frequencies (Hz)*/
#define PRINT_DATA    2   //so your eyes dont burn out
#define GET_DATA      200 
#define CALC_BALL     200 
#define DISP_BALL     100 //as specified

#define PRINT_DATA_US 1000000/PRINT_DATA
#define GET_DATA_US   1000000/GET_DATA 
#define CALC_BALL_US  1000000/CALC_BALL 
#define DISP_BALL_US  1000000/DISP_BALL

#endif
