
#ifndef GPIO_SETUP_H
#define GPIO_SETUP_H

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

/*Max Clock Rate*/
#define MAX7219_MAX_CLK 500000

/*MAX7219 LED bar addresses*/
#define col1_addr	0x01;
#define col2_addr	0x02;
#define col3_addr	0x03;
#define col4_addr	0x04;
#define col5_addr	0x05;
#define col6_addr	0x06;
#define col7_addr	0x07;
#define col8_addr	0x08;

/*Mux*/
#define SPI1_CS      42
#define SPI1_MOSI    43
#define SPI1_CLK     55

/*Pins*/
#define SPI_CS_PIN   10 
#define SPI_MOSI_PIN 11
#define SPI_CLK_PIN  13

void gpio_setup();
void gpio_unexport();
void display_pattern(uint8_t * pattern, struct spi_ioc_transfer *spi_tx);
int init_MAX7219 (struct spi_ioc_transfer *spi_tx);

#endif
