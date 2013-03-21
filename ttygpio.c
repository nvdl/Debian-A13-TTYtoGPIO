#include <linux/module.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/errno.h>			// For "EINVAL"

#include <linux/termios.h>			// "TIOCM_DTR" and other definitions
#include <asm-generic/ioctls.h>		// "TIOCMBIS" and other "ioctl()" commands' numbers
#include <asm/io.h>					// "ioremap()"

#define DEVICE_MAJOR_NUMBER 90
#define DEVICE_NAME "TTYtoGPIO"

//#define DEBUG_MODULE

//#define MODULE
//#define __KERNEL__

#define LINUX

#define SUNXI_SW_PORTC_IO_BASE  (0x01c20800)

#define MMAP_OFFSET				SUNXI_SW_PORTC_IO_BASE
#define MMAP_SIZE				200
//=================================================================================================

MODULE_LICENSE("GPL"); // this avoids kernel warning
MODULE_DESCRIPTION("Creates a virtual TTY Port to control GPIO lines");
MODULE_AUTHOR("nvd");
//=================================================================================================

int init_module(void);
void cleanup_module(void);

static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static long device_ioctl(struct file *, unsigned int, unsigned long);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

static void *baseAddr;

static int signals = 0;
//=================================================================================================

static char msg[100] = {0};
static short readPos=0;
static int times = 0;
//=================================================================================================

static struct file_operations fops = {
	.read = device_read,
	.open = device_open,
	.write = device_write,
	.release = device_release,
	.unlocked_ioctl = device_ioctl,
};
//=================================================================================================

static long device_ioctl(struct file *filp, unsigned int command, unsigned long argument) {
	int *arg = (int *) argument;
	
	//printk(KERN_DEBUG "TTYtoGPIO: device_ioctl(): command: %u, argument: %lu\n", command, argument);
	
	switch (command) {
	
	case TIOCMBIS: // Set bits
		if (*arg & TIOCM_DTR) {
			#ifdef DEBUG_MODULE
			printk(KERN_DEBUG "TTYtoGPIO: device_ioctl(): DTR=1\n");
			#endif
			signals |= TIOCM_DTR;
		}
		
		if (*arg & TIOCM_RTS) {
			#ifdef DEBUG_MODULE
			printk(KERN_DEBUG "TTYtoGPIO: device_ioctl(): RTS=1\n");
			#endif
			signals |= TIOCM_RTS;
		}

		break;
	
	case TIOCMBIC: // Clear bits
		if (*arg & TIOCM_DTR) {
			#ifdef DEBUG_MODULE
			printk(KERN_DEBUG "TTYtoGPIO: device_ioctl(): DTR=0\n");
			#endif
			signals &= ~TIOCM_DTR;
		}
		
		if (*arg & TIOCM_RTS) {
			#ifdef DEBUG_MODULE
			printk(KERN_DEBUG "TTYtoGPIO: device_ioctl(): RTS=0\n");
			#endif
			signals &= ~TIOCM_RTS;
		}
	
		break;
	
	case TIOCMGET: // Get bits
		#ifdef DEBUG_MODULE
		printk(KERN_DEBUG "TTYtoGPIO: device_ioctl(): Get signals levels\n");
		#endif
		
		*arg = signals; // |= (TIOCM_DSR | TIOCM_CTS | TIOCM_RI | TIOCM_CD);

		break;
	
	case TCGETS: // Get settings
		#ifdef DEBUG_MODULE
		printk(KERN_DEBUG "TTYtoGPIO: device_ioctl(): Get settings\n");
		#endif
		
		break;
	
	case TCSETS: // Set settings
		#ifdef DEBUG_MODULE
		printk(KERN_DEBUG "TTYtoGPIO: device_ioctl(): Set settings\n");
		#endif
		
		break;
	
	default:
		return -EINVAL;
		break;
	}

	return 0;
}
//=================================================================================================

int init_module(void) {

	int t = register_chrdev(DEVICE_MAJOR_NUMBER, DEVICE_NAME, &fops);

	if (t < 0) {
		printk(KERN_ERR "TTYtoGPIO: device registration failed\n");
	} else {
		printk(KERN_DEBUG "TTYtoGPIO: module loaded\n");
	}

#if 0
	int i;
	
	if (t < 0) {
		printk(KERN_ERR "TTYtoGPIO: device registration failed\n");
	} else {
		printk(KERN_DEBUG "TTYtoGPIO: module loaded\n");
		baseAddr = ioremap(MMAP_OFFSET, MMAP_SIZE);
		
		if (baseAddr == NULL) {
			printk(KERN_DEBUG "TTYtoGPIO: init_module(): Got NULL baseAddr");
		}
		
		printk(KERN_DEBUG "TTYtoGPIO: ioread: ");
		
		for (i = 0; i < MMAP_SIZE; i ++) {
			printk( KERN_DEBUG "%u-", ioread8(baseAddr + i) );
		}
	}
#endif

	return t;
}
//=================================================================================================

void cleanup_module(void) {

	unregister_chrdev(DEVICE_MAJOR_NUMBER, DEVICE_NAME);
	iounmap(baseAddr);
	
	printk(KERN_DEBUG "TTYtoGPIO: module unloaded\n");
}
//=================================================================================================

static int device_open(struct inode *inod, struct file *filp) {

	times ++;
	
	#ifdef DEBUG_MODULE
	printk(KERN_DEBUG "TTYtoGPIO: device opened %d times\n", times);
	#endif
	
	return 0;
}
//=================================================================================================

static ssize_t device_read(struct file *filp, char *buff, size_t len, loff_t *off) {

	short count = 0;

	while (len && (msg[readPos] != 0)) {
		put_user(msg[readPos], buff ++); //copy byte from kernel space to user spare
		count ++;
		len --;
		readPos ++;
	}

	return count;
}
//=================================================================================================

static ssize_t device_write(struct file *filp, const char *buff, size_t len, loff_t *off) {

	short ind = len - 1;
	short count = 0;
	short i;
	char buffw[100];
	
	memset(msg, 0, 100);
	readPos = 0;

	for (i = 0; i < len; i ++) {
		buffw[i] = buff[i];
	}

	buffw[i] = '\0';	

	#ifdef DEBUG_MODULE
	printk(KERN_DEBUG "TTYtoGPIO:dev_write():%s\n", buffw);
	#endif

	while (len > 0) {
		//printk(KERN_DEBUG "TTYtoGPIO: device opened %d times\n", times);
		//printk(KERN_DEBUG "TTYtoGPIO:dev_write():%s\n", buff);
		msg[count ++] = buff[ind --]; //copy the given string to the driver but in reverse
		len --;
	}

	return count;
}
//=================================================================================================

static int device_release(struct inode *inod, struct file *filp) {

	#ifdef DEBUG_MODULE
	printk(KERN_DEBUG "TTYtoGPIO: device closed\n");
	#endif
	
	return 0;
}
//=================================================================================================
