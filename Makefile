KDIR:=/home/esp/SDK/sysroots/i586-poky-linux/usr/src/kernel
CC = i586-poky-linux-gcc
ARCH = x86
CROSS_COMPILE = i586-poky-linux-
SROOT=/home/esp/SDK/sysroots/i586-poky-linux/

APP1 = run_game

obj-m:= mpu6050_driver.o

all:
	make ARCH=x86 CROSS_COMPILE=i586-poky-linux- -C $(KDIR) M=$(PWD) modules 

	i586-poky-linux-gcc -o $(APP1) main.c max7219_user.h max7219_user.c math_functions.h -lm -lpthread -pthread --sysroot=$(SROOT) 

clean:
	rm -f *.ko
	rm -f *.o
	rm -f Module.symvers
	rm -f modules.order
	rm -f *.mod.c
	rm -rf .tmp_versions
	rm -f *.mod.c
	rm -f *.mod.o
	rm -f \.*.cmd
	rm -f Module.markers
	rm -f $(APP)
	rm -f run_1

	
