
#include "max7219_user.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>


void gpio_setup()
{
	int fd;

	char buffer [128];

	fd = open("/sys/class/gpio/export", O_WRONLY);
	
	//get gpio 42, 43, 55 for display (mux selects)

	sprintf(buffer, "%d", 42);
	write(fd, buffer, strlen(buffer));

	sprintf(buffer, "%d", 43);
	write(fd, buffer, strlen(buffer));

	sprintf(buffer, "%d", 55);
	write(fd, buffer, strlen(buffer));

	close(fd);
 
	fd = open("/sys/class/gpio/gpio42/direction", O_WRONLY);
	sprintf(buffer, "out");
	write(fd, buffer, strlen(buffer));
	close(fd);

	fd = open("/sys/class/gpio/gpio43/direction", O_WRONLY);
	sprintf(buffer, "out");
	write(fd, buffer, strlen(buffer));
	close(fd);

	fd = open("/sys/class/gpio/gpio55/direction", O_WRONLY);
	sprintf(buffer, "out");
	write(fd, buffer, strlen(buffer));
	close(fd);


	/*set mux selects (42, 43, 55) to 0*/

	fd = open("/sys/class/gpio/gpio42/value", O_WRONLY);
	sprintf(buffer, "%d", 0);
	write(fd, buffer, strlen(buffer));
	close(fd);

	fd = open("/sys/class/gpio/gpio43/value", O_WRONLY);
	sprintf(buffer, "%d", 0);
	write(fd, buffer, strlen(buffer));
	close(fd);

	fd = open("/sys/class/gpio/gpio55/value", O_WRONLY);
	sprintf(buffer, "%d", 0);
	write(fd, buffer, strlen(buffer));
	close(fd);

}


void gpio_unexport()
{
	int fd;
	char buffer [128];

	//unexport all gpios
	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	
	sprintf(buffer, "%d", 42);
	write(fd, buffer, strlen(buffer));

	sprintf(buffer, "%d", 43);
	write(fd, buffer, strlen(buffer));

	sprintf(buffer, "%d", 55);
	write(fd, buffer, strlen(buffer));

	close(fd);

}

void display_pattern(uint8_t * pattern, struct spi_ioc_transfer *spi_tx)
{
	unsigned short i;

	uint8_t data_to_xfer [2];
	int fd;

	//open spi device
	fd = open("/dev/spidev1.0", O_WRONLY);

	if (fd < 0)
		printf("can't open device");
	
	for(i = 0; i < 8; i++) {
		data_to_xfer[0] = i+1;
		data_to_xfer[1] = pattern[i];
		(*spi_tx).tx_buf = (unsigned long) data_to_xfer;
		ioctl(fd, SPI_IOC_MESSAGE(1), spi_tx);
	}

	close(fd);

}

//initialize led matrix
int init_MAX7219 (struct spi_ioc_transfer *spi_tx)
{
	uint8_t data_to_spi_tx [2];
	int fd;

	(*spi_tx).speed_hz = MAX7219_MAX_CLK;
	(*spi_tx).bits_per_word = 8;
	(*spi_tx).delay_usecs = 100;
	(*spi_tx).rx_buf = (unsigned long) data_to_spi_tx;
	(*spi_tx).len = 2;

	//open spi device
	fd = open("/dev/spidev1.0", O_WRONLY);

	if (fd < 0) {
		printf("Failed to open device.\n");
		return -1;
	}	

	data_to_spi_tx[0] = 0x0C;	//shutdown register
	data_to_spi_tx[1] = 0x01;	
	(*spi_tx).tx_buf = (unsigned long) data_to_spi_tx;
	
	ioctl(fd, SPI_IOC_MESSAGE(1), spi_tx);

	data_to_spi_tx[0] = 0x09;	//decode mode register
	data_to_spi_tx[1] = 0x00;	
	(*spi_tx).tx_buf = (unsigned long) data_to_spi_tx;
	
	ioctl(fd, SPI_IOC_MESSAGE(1), spi_tx);

	data_to_spi_tx[0] = 0x0A;	//intensity register
	data_to_spi_tx[1] = 0x0A;	
	(*spi_tx).tx_buf = (unsigned long) data_to_spi_tx;
	
	ioctl(fd, SPI_IOC_MESSAGE(1), spi_tx);

	data_to_spi_tx[0] = 0x0B;	//scanlimit register
	data_to_spi_tx[1] = 0x07;	
	(*spi_tx).tx_buf = (unsigned long) data_to_spi_tx;
	
	ioctl(fd, SPI_IOC_MESSAGE(1), spi_tx);

	data_to_spi_tx[0] = 0x0F;	//display test register
	data_to_spi_tx[1] = 0x00;	
	(*spi_tx).tx_buf = (unsigned long) data_to_spi_tx;
	
	ioctl(fd, SPI_IOC_MESSAGE(1), spi_tx);
	
	close(fd);
	return 0; //success
}
