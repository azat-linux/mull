
/**
 * This module create /dev/mull (null + mem)
 * It didn't save anything to memory, just test speed.
 *
 * It use circular buffer.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <asm/uaccess.h> /* for copy_from_user */
#include <linux/time.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Azat Khuzhin <a3at.mail@gmail.com>");

// Move to header
static int mullInit(void) __init;
static void mullExit(void) __exit;
static int deviceOpen(struct inode *inode, struct file *file);
static int deviceRelease(struct inode *inode, struct file *file);
static ssize_t deviceRead(struct file *filePtr, char *buffer,
                          size_t length, loff_t *offset);
static ssize_t deviceWrite(struct file *filePtr, const char *buffer,
                           size_t length, loff_t *offset);

static struct file_operations deviceOperations = {
    .read = deviceRead,
    .write = deviceWrite,
    .open = deviceOpen,
    .release = deviceRelease
};

module_init(mullInit);
module_exit(mullExit);

#define SUCCESS 0
#define DEVICE_NAME "mull"

static int majorNumber;
static int deviceOpened = 0; /* prevent multiple access to device */
static struct timeval start;
static ulong bytesRecieved;

static ulong memoryBufferSize = 1 << 10;
module_param(memoryBufferSize, ulong, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(myshort, "Installs in-memory buffer size (default 1024)");

static void *circularBuffer;

/**
 * Add MKDEV()
 */
static int __init mullInit(void)
{
    circularBuffer = kmalloc(memoryBufferSize, GFP_ATOMIC);

    majorNumber = register_chrdev(0, DEVICE_NAME, &deviceOperations);

    if (majorNumber < 0) {
      printk(KERN_ALERT "Registering char device failed with %d\n", majorNumber);
      return majorNumber;
    }

    printk(KERN_INFO "mull: loaded (buffer size: %lu, major number: %d)",
                     memoryBufferSize, majorNumber);
    return SUCCESS;
}

static void __exit mullExit(void)
{
    kfree(circularBuffer);

    printk(KERN_INFO "mull: exit\n");
}

static int deviceOpen(struct inode *inode, struct file *file)
{
    if (deviceOpened) {
        return -EBUSY;
    }

    deviceOpened++;
    try_module_get(THIS_MODULE);

    do_gettimeofday(&start);
    bytesRecieved = 0;

    printk(KERN_INFO "mull: device is opened\n");
    return SUCCESS;
}

static int deviceRelease(struct inode *inode, struct file *file)
{
    struct timeval end;
    int secondsRunning;

    do_gettimeofday(&end);

    deviceOpened--;
    module_put(THIS_MODULE);

    /**
     * TODO: write this in normal way
     */
    secondsRunning = end.tv_sec - start.tv_sec;
    if (!secondsRunning) {
        secondsRunning = 1;
    }

    bytesRecieved = (bytesRecieved / 1024 /*K*/ / 1024 /*M*/);
    if (!bytesRecieved) {
        bytesRecieved = 1;
    }

    printk(KERN_INFO "mull: device is released (speed: %ld MBs)\n",
                     (ulong)((double)bytesRecieved / secondsRunning));
    return SUCCESS;
}

static ssize_t deviceRead(struct file *filePtr, char *buffer,
                          size_t length, loff_t *offset)
{
    printk(KERN_ALERT "Sorry, this operation isn't supported.\n");
    return -EINVAL;
}

/**
 * TODO: circular buffer
 */
static ssize_t deviceWrite(struct file *filePtr, const char *buffer,
                           size_t length, loff_t *offset)
{
    if (length > memoryBufferSize) {
        length = memoryBufferSize;
    }

    if (copy_from_user(circularBuffer, buffer, length)) {
        return -EFAULT;
    }

    bytesRecieved += length;

    return length;
}