README.txt

CSE4380 Project 4

Written by Daniel Pikul

Pin-Out:
MAX7219:
SPI_CS_PIN   10 
SPI_MOSI_PIN 11
SPI_CLK_PIN  13

MPU-6050

SCL SCL
SDA SDA

Ensure 3.3V Jumper Pin is set for proper operation of sensor.

Instructions:

1. In this directory, enter the command 'make'.
   This will generate the necessary binary for running
   the program as well as .ko file for the MPU6050 module.

2. Use SCP to transfer the entire folder contents to the Galileo.

3. Call 'insmod mpu6050_driver.ko' to load the module. If this is unsuccessful, 
   please contact me as it worked in over 10 tests.

4. Run the program by entering command './run_game optional_difficulty'.
   optional_difficulty is an optional integer that will modify the 
   starting difficulty, from level 1-10.

5. The program is designed to run indefinitely until Ctrl+C is entered.
   Entering Ctrl+C will cause the program to free all GPIO and IRQ's,
   as well as close all opened files cleanly. 

6. Losing a level will blink an 'X' on ths screen, and the level will restart.

7. Winning a level will cause the game to advance to the next level. Beating
   all 10 levels will end the game.

Orientation:

Please see the file "orientation.JPG".
Both the MPU and the screen should be held horizontally such that both pin outs face the same direction.

Implementation:

This program met ALL non-bonus criteria in testing. Please contact me at dpikul@asu.edu 
if it does not work.

In kernel space, when the mpu6050 driver is opened, a kthread is spawned which will read
the mpu6050 data at a rate of 'GET_DATA' hertz (GET_DATA is defined in game_schedules.h).
This kthread will continue to read and post data as joystick events until the program ends.

Main spawns two threads, one which updates the LED screen at a rate of 100hz, and another
which reads data as joystick events and performs physics calculations on the data to determine 
parameters such as the ball's acceleration, velocity, and location, which can then be displayed on the LED board.

NOTE: This program reads from:
#define KEYFILE "/dev/input/event2"

Please modify this if a different event file is used on the testing system!


