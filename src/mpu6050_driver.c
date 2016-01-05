#include <linux/module.h>  // Module Defines and Macros (THIS_MODULE)
#include <linux/kernel.h>  // 
#include <linux/fs.h>	   // Inode and File types
#include <linux/cdev.h>    // Character Device Types and functions.
#include <linux/types.h>
#include <linux/slab.h>	   // Kmalloc/Kfree
#include <linux/delay.h>
#include <asm/uaccess.h>   // Copy to/from user space
#include <linux/string.h>
#include <linux/device.h>  // Device Creation / Destruction functions
#include <linux/i2c.h>     // i2c Kernel Interfaces
#include <linux/i2c-dev.h>
#include <linux/gpio.h>
#include <asm/gpio.h>
#include <linux/init.h>
#include <linux/moduleparam.h> // Passing parameters to modules through insmod
#include <linux/input.h>
#include <linux/sched.h>
#include <linux/kthread.h>

#include "mpu6050_driver.h"
#include "game_schedules.h"

#define DEVICE_NAME "mpu6050"  // device name to be created and registered
#define DEVICE_ADDR 0x68
#define I2CMUX 29 // Clear for I2C function

/*Global kthread*/
struct task_struct *task;

/*Joystick*/
static struct input_dev * js_dev;


/* per device structure */
struct mpu6050_dev {
	struct cdev cdev;               /* The cdev structure */
	// Local variables
	struct i2c_client *client;
	struct i2c_adapter *adapter;

} *mpu6050_dev_ptr;

static dev_t  mpu6050_dev_num;        /* Allotted device number */
struct class  *mpu6050_dev_class;     /* Tie with the device model */
static struct device *mpu6050_device;

unsigned long long time_begin, time_end;

/* Looping function that reads and posts joystick events to input subsystem*/
int post_data(void *data) {

	uint8_t gdata [14];
	int16_t gx, gy, gz, ax, ay, az;
	int ret;
	char buf[2];

	buf[0] = PWR_MNG;
	buf[1] = 0x01;
	ret = i2c_master_send(mpu6050_dev_ptr->client, buf, 2); 
	if(ret < 0)
	{
		printk("Error: Could not send power management address.\n");
		return ret;
	}

	while(!kthread_should_stop()) {
		//time_begin = rdtsc();
		
		buf[0] = MPU_ADDR;

		//reset address
		ret = i2c_master_send(mpu6050_dev_ptr->client, buf, 1);
		if(ret < 0)
		{
			printk("Error: Could not send MPU Address.\n");
			return ret;
		}
	
		// Read MPU Data 14 bytes
		ret = i2c_master_recv(mpu6050_dev_ptr->client, (char *)&gdata, 14);
		if(ret < 0)
		{
			printk("Error: could not read MPU6050 Data.\n");
			return ret;
		}

		//store as 16 bit integers
		ax = (gdata[0] << 8) | gdata[1];
		ay = (gdata[2] << 8) | gdata[3];
		az = (gdata[4] << 8) | gdata[5];

		gx = (gdata[8]  << 8) | gdata[9];
		gy = (gdata[10] << 8) | gdata[11];
		gz = (gdata[12] << 8) | gdata[13];		

		//printk("ay: %d\n", ay);		

		//send stuff to input subsystem
		input_report_abs(js_dev, ABS_THROTTLE, ax);
		input_report_abs(js_dev, ABS_RUDDER  , ay);
		input_report_abs(js_dev, ABS_WHEEL   , az);
		input_report_abs(js_dev, ABS_RX      , gx);
		input_report_abs(js_dev, ABS_RY      , gy);
		input_report_abs(js_dev, ABS_RZ      , gz);

		//sync
		input_sync(js_dev);

		//time_end = rdtsc();
		//printk("Cycles Spent Reading from the Sensor: %llu\n", (time_end - time_begin));
		//sleep for some u-seconds
		udelay(GET_DATA_US);
	}

	return 1;

}


/*
* Open driver
*/
int mpu6050_driver_open(struct inode *inode, struct file *file)
{
	struct mpu6050_dev *mpu6050_dev_ptr;

	/* Get the per-device structure that contains this cdev */
	mpu6050_dev_ptr = container_of(inode->i_cdev, struct mpu6050_dev, cdev);

	/* Easy access to tmp_devp from rest of the entry points */
	file->private_data = mpu6050_dev_ptr;

	printk("Device Opened\n");

	//spawn kthread and call post_data()
	task = kthread_run(&post_data, NULL, "thread");	

	return 0;
}

/*
 * Release driver
 */
int mpu6050_driver_release(struct inode *inode, struct file *file) {

	//struct tmp_dev *tmp_devp = file->private_data;
	//gpio_free(I2CMUX);
	//printk("Released pin 29.\n");
	kthread_stop(task);
	printk("Closed MPU6050 task.\n");
	return 0;
}

/*
 * Write to driver
 */
ssize_t mpu6050_driver_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	struct mpu6050_dev *mpu6050_dev_ptr = file->private_data;
	int ret;	
	char buff[2];

	//write power management
	buff[0] = PWR_MNG;
	buff[1] = 0x01;
	ret = i2c_master_send(mpu6050_dev_ptr->client, buff, 2); 
	if(ret < 0)
	{
		printk("Error: Could not send power management address.\n");
		return ret;
	}
	
	return 0;
}
/*
 * Read from driver
 */
ssize_t mpu6050_driver_read(struct file *file, char *user_buf, size_t count, loff_t *ppos)
{
	struct mpu6050_dev *mpu6050_dev_ptr = file->private_data;
	int ret;
	uint8_t gdata [14];
	int16_t gx, gy, gz, ax, ay, az;

	char buf[1];
	
	printk("entered read function\n");

	buf[0] = MPU_ADDR;
	ret = i2c_master_send(mpu6050_dev_ptr->client, buf, 1);
	if(ret < 0)
	{
		printk("Error: Could not send MPU Address.\n");
		return ret;
	}
	

	// Read MPU Data 14 bytes
	ret = i2c_master_recv(mpu6050_dev_ptr->client, (char *)&gdata, 14);
	if(ret < 0)
	{
		printk("Error: could not read Gyro Data.\n");
		return ret;
	}

	ax = (gdata[0] << 8) | gdata[1];
	ay = (gdata[2] << 8) | gdata[3];
	az = (gdata[4] << 8) | gdata[5];

	gx = (gdata[8]  << 8) | gdata[9];
	gy = (gdata[10] << 8) | gdata[11];
	gz = (gdata[12] << 8) | gdata[13];

	//debug
	//printk("\nACCEL X: %d\n", ax);
	//printk("ACCEL Y: %d\n",   ay);
	//printk("ACCEL Z: %d\n",   az);

	//printk("\nGYRO X: %d\n", gx);
	//printk("GYRO Y: %d\n",   gy);
	//printk("GYRO Z: %d\n",   gz);

	// Copy to the user
	ret = copy_to_user(user_buf, &gdata, 14);

	if(ret < 0)
		printk("Failed to copy to user.\n");

	return ret;

}

/* File operations structure. Defined in linux/fs.h */
static struct file_operations mpu6050_fops = {
    .owner		= THIS_MODULE,            /* Owner */
    .open		= mpu6050_driver_open,    /* Open method */
    .release	= mpu6050_driver_release, /* Release method */
    .write		= mpu6050_driver_write,   /* Write method */
    .read		= mpu6050_driver_read,    /* Read method */
};

/*
 * Driver Initialization
 */
int __init mpu6050_driver_init(void)
{
	int ret;
	
	/* Request dynamic allocation of a device major number */
	if (alloc_chrdev_region(&mpu6050_dev_num, 0, 1, DEVICE_NAME) < 0) {
		printk(KERN_DEBUG "Can't register device\n"); 
		return -1;
	}

	/* Populate sysfs entries */
	mpu6050_dev_class = class_create(THIS_MODULE, DEVICE_NAME);

	/* Allocate memory for the per-device structure */
	mpu6050_dev_ptr = kmalloc(sizeof(struct mpu6050_dev), GFP_KERNEL);
		
	if (!mpu6050_dev_ptr) {
		printk("Bad Kmalloc\n"); 
		return -ENOMEM;
	}

	/* Request I/O region */

	/* Connect the file operations with the cdev */
	cdev_init(&mpu6050_dev_ptr->cdev, &mpu6050_fops);
	mpu6050_dev_ptr->cdev.owner = THIS_MODULE;

	/* Connect the major/minor number to the cdev */
	ret = cdev_add(&mpu6050_dev_ptr->cdev, (mpu6050_dev_num), 1);

	if (ret) {
		printk("Bad cdev\n");
		return ret;
	}

	/* Send uevents to udev, so it'll create /dev nodes */
	mpu6050_device = device_create(mpu6050_dev_class, NULL, MKDEV(MAJOR(mpu6050_dev_num), 0), NULL, DEVICE_NAME);	

	ret = gpio_request(I2CMUX, "I2CMUX");

	if(ret)
	{
		printk("GPIO %d is not requested.\n", I2CMUX);
	}

	ret = gpio_direction_output(I2CMUX, 0);

	if(ret)
	{
		printk("GPIO %d is not set as output.\n", I2CMUX);
	}

	gpio_set_value_cansleep(I2CMUX, 0); // Direction output didn't seem to init correctly.	

	// Create Adapter using:
	mpu6050_dev_ptr->adapter = i2c_get_adapter(0); // /dev/i2c-0
	if(mpu6050_dev_ptr->adapter == NULL)
	{
		printk("Could not acquire i2c adapter.\n");
		return -1;
	}

	// Create Client Structure
	mpu6050_dev_ptr->client = (struct i2c_client*) kmalloc(sizeof(struct i2c_client), GFP_KERNEL);

	mpu6050_dev_ptr->client->addr = DEVICE_ADDR; // Device Address (set by hardware)

	snprintf(mpu6050_dev_ptr->client->name, I2C_NAME_SIZE, "i2c_tmp102");

	mpu6050_dev_ptr->client->adapter = mpu6050_dev_ptr->adapter;

	printk("MPU 6050 Device initialized\n");

	js_dev = input_allocate_device();
	printk("Allocated Joystick device\n");

	js_dev->name = "Joystick Device";

	set_bit(EV_ABS, js_dev->evbit);

	set_bit(ABS_THROTTLE, js_dev->absbit);
	set_bit(ABS_RUDDER, js_dev->absbit);
	set_bit(ABS_WHEEL, js_dev->absbit);
	set_bit(ABS_RX, js_dev->absbit);
	set_bit(ABS_RY, js_dev->absbit);
	set_bit(ABS_RZ, js_dev->absbit);

	ret = input_register_device(js_dev);
	printk("Joystick device registered\n");

	input_set_abs_params(js_dev, ABS_THROTTLE, -32768, 32767, 0, 0);
	input_set_abs_params(js_dev, ABS_RUDDER, -32768, 32767, 0, 0);
	input_set_abs_params(js_dev, ABS_WHEEL, -32768, 32767, 0, 0);
	input_set_abs_params(js_dev, ABS_RX, -32768, 32767, 0, 0);
	input_set_abs_params(js_dev, ABS_RY, -32768, 32767, 0, 0);
	input_set_abs_params(js_dev, ABS_RZ, -32768, 32767, 0, 0);

	return 0;
}
/* Driver Exit */
void __exit mpu6050_driver_exit(void)
{

	// Close and cleanup
	i2c_put_adapter(mpu6050_dev_ptr->adapter);
	kfree(mpu6050_dev_ptr->client);

	/* Release the major number */
	unregister_chrdev_region((mpu6050_dev_num), 1);

	/* Destroy device */
	device_destroy (mpu6050_dev_class, MKDEV(MAJOR(mpu6050_dev_num), 0));
	cdev_del(&mpu6050_dev_ptr->cdev);
	kfree(mpu6050_dev_ptr);
	
	/* Destroy driver_class */
	class_destroy(mpu6050_dev_class);

	input_unregister_device(js_dev);
	gpio_free(I2CMUX);
}

module_init(mpu6050_driver_init);
module_exit(mpu6050_driver_exit);
MODULE_LICENSE("GPL v2");
