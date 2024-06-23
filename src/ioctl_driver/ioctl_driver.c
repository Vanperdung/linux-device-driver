#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include<linux/uaccess.h>
#include "../libs/common.h"

dev_t dev_number;
struct cdev ioctl_cdev;
struct class *ioctl_class = NULL;
int32_t value = 0;

#define WR_VALUE _IOW('a', 'a', int32_t*)
#define RD_VALUE _IOR('a', 'b', int32_t*)

ssize_t read_func(struct file *filp, char __user *buff, size_t, loff_t *off)
{
    pr_info("Read\n");
    return 0;
}

ssize_t write_func(struct file *filp, const char __user *buff, size_t, loff_t *off)
{
    pr_info("Wrote\n");
    return 0;
}

int open_func(struct inode *inode, struct file *filp)
{
    pr_info("Opened\n");
    return 0;
}

int release_func(struct inode *inode, struct file *filp)
{
    pr_info("Closed\n");
    return 0;
}

long ioctl_func(struct file *filp, unsigned int cmd, unsigned long arg)
{
    switch (cmd)
    {
        case WR_VALUE:
            copy_from_user(&value, (int32_t*)arg, sizeof(value));
            pr_info("Value = %d\n", value);
            break;
        case RD_VALUE:
            pr_info("Value = %d\n", value);
            copy_to_user((int32_t*)arg, &value, sizeof(value));
            pr_info("Copied\n");
            break;
        default:
            break;
    }
    return 0;
}

static struct file_operations fops = 
{
    .owner = THIS_MODULE,
    .open = open_func,
    .read = read_func,
    .write = write_func,
    .unlocked_ioctl = ioctl_func,
    .release = release_func
};

static int __init ioctl_driver_init(void)
{  
    int ret;

    // allocate major and minor numbers for driver
    ret = alloc_chrdev_region(&dev_number, 0, 1, "ioctl_dev");
    if (ret < 0)
    {
        pr_err(FUNC_FORMAT_INFO "alloc_chrdev_region error \r\n", FUNC_INFO);
    }

    pr_info("Major = %d, minor = %d", MAJOR(dev_number), MINOR(dev_number));

    // create a character device with file oparations that defined
    cdev_init(&ioctl_cdev, &fops);

    // register character device with kernel (including the major number)
    cdev_add(&ioctl_cdev, dev_number, 1);

    // create a directory in sysfs, return a pointer to this struct class
    ioctl_class = class_create("ioctl_class");
    
    // create a device file and register it with sysfs
    device_create(ioctl_class, NULL, dev_number, NULL, "ioctl_dev");

    pr_info("Loaded a kernel module");

    return 0;
}

static void __exit ioctl_driver_exit(void)
{
    // removes a device file that registered with sysfs
    device_destroy(ioctl_class, dev_number);

    // remove a class struct
    class_destroy(ioctl_class);

    // unregister character device from kernel
    cdev_del(&ioctl_cdev);

    // unregister a range of device numbers
    unregister_chrdev_region(dev_number, 1);
}

module_init(ioctl_driver_init);
module_exit(ioctl_driver_exit);

MODULE_LICENSE("GPL");