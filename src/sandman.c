#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <scsi/scsi_device.h>
#include <linux/err.h>
#include <asm/errno.h>

#include "sandman.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Adrien RAULOT <adrien.raulot@gmail.com>");
MODULE_DESCRIPTION("Sandman driver module");

static unsigned int major = 0;
static struct class *class = NULL;
static dev_t dev;
static struct cdev cdev;
static struct device *device = NULL;
static struct device *op_dev = NULL;

extern struct bus_type scsi_bus_type;
int pm_runtime_force_suspend(struct device *dev);
int pm_runtime_force_resume(struct device *dev);

static int sandman_suspend_disk(char *name);
static int sandman_resume_disk(char *name);

static int sandman_open(struct inode *inode, struct file *filp)
{
    unsigned int mj = imajor(inode);
    unsigned int mn = iminor(inode);

    if (mj != major || mn != NDEVICES) {
        printk(KERN_WARNING "[Sandman] "
                            "No device found with major=%d and minor=%d.\n",
                            mj, mn);
        return -ENODEV;
    }

    if (inode->i_cdev != &cdev) {
        printk(KERN_WARNING "[Sandman] open: internal error.\n");
        return -ENODEV;
    }

    return 0;
}

static int sandman_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t sandman_read(struct file *filp, char __user *buf, size_t count,
                            loff_t *f_pos)
{
    return 0;
}

static ssize_t sandman_write(struct file *filp, const char __user *buf,
                             size_t count, loff_t *f_pos)
{
    return 0;
}

static loff_t sandman_llseek(struct file *filp, loff_t off, int whence)
{
    return 0;
}

static long sandman_ioctl(struct file *filp, unsigned int cmd,
                          unsigned long arg)
{
    int ret = 0;
    char *str = (char *)arg;

    if (!str)
        return -1;

    switch(cmd) {
        case IOCTL_SANDMAN_SUSPEND:
            ret = sandman_suspend_disk(str);
            break;
        case IOCTL_SANDMAN_RESUME:
            ret = sandman_resume_disk(str);
            break;
        default:
            break;
    }
    return ret;
}

struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = sandman_read,
    .write = sandman_write,
    .open = sandman_open,
    .release = sandman_release,
    .llseek = sandman_llseek,
    .unlocked_ioctl = sandman_ioctl,
};

static int match_name(struct device *dev, void *data)
{
    const char *name = data;
    return sysfs_streq(name, dev_name(dev));
}

static int dev_check(struct device *dev, void *data)
{
    op_dev = device_find_child(dev, data, match_name);
    return op_dev ? 1 : 0;
}

static struct device *sandman_find_dev(char *name)
{
    int err = 0;
    struct device *device = NULL;

    bus_for_each_dev(&scsi_bus_type, NULL, name, dev_check);
    if (!op_dev) {
        err = -EINVAL;
        goto fail;
    }

    device = op_dev;
    op_dev = NULL;
    return device;

fail:
    return ERR_PTR(err);
}

static int sandman_suspend_disk(char *name)
{
    int err = 0;
    bool manage_start_stop = false;
    struct device *device = NULL;
    struct device *parent = NULL;
    struct scsi_device *sdev = NULL;

    device = sandman_find_dev(name);
    if (IS_ERR(device)) {
        printk(KERN_WARNING "[Sandman] %s: no disk found.\n", name);
        err = PTR_ERR(device);
        goto fail;
    }

    /*
     * Ensure that manage_start_stop is enabled
     */
    parent = device->parent;
    sdev = to_scsi_device(parent);
    if (!sdev)
        return 1;
    if (sdev->manage_start_stop)
        manage_start_stop = true;
    else
        sdev->manage_start_stop = 1;

    err = pm_runtime_force_suspend(parent);
    sdev->manage_start_stop = manage_start_stop ? 1 : 0;
    if (err)
        goto fail;

    if (parent->power.runtime_status != RPM_SUSPENDED) {
        err = 2;
        goto fail;
    }
    return 0;

fail:
    return err;
}

static int sandman_resume_disk(char *name)
{
    int err = 0;
    bool ignore_children = false;
    bool manage_start_stop = false;
    struct device *device = NULL;
    struct device *parent = NULL;
    struct scsi_device *sdev = NULL;

    device = sandman_find_dev(name);
    if (IS_ERR(device)) {
        printk(KERN_WARNING "[Sandman] %s: no disk found.\n", name);
        err = PTR_ERR(device);
        goto fail;
    }

    /*
     * Ignore child_count value (but still updated).
     * Necessary to successfully set runtime_status to RPM_ACTIVE.
     */
    parent = device->parent;
    if (parent->parent->power.ignore_children)
        ignore_children = true;
    else
        parent->parent->power.ignore_children = 1;

    /*
     * Ensure that manage_start_stop is enabled
     */
    sdev = to_scsi_device(parent);
    if (!sdev)
        return 1;
    if (sdev->manage_start_stop)
        manage_start_stop = true;
    else
        sdev->manage_start_stop = 1;

    err = pm_runtime_force_resume(parent);
    parent->parent->power.ignore_children = ignore_children ? 1 : 0;
    sdev->manage_start_stop = manage_start_stop ? 1 : 0;
    if (err)
        goto fail;

    if (parent->power.runtime_status != RPM_ACTIVE) {
        err = 2;
        goto fail;
    }
    return 0;

fail:
    return err;
}

static int __init init_sandman(void)
{
    dev_t devno;
    int minor = NDEVICES;
    int err = 0;

    err = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
    if (err < 0) {
        printk(KERN_WARNING "[Sandman] alloc_chrdev_region() failed\n");
        return err;
    }

    major = MAJOR(dev);

    class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(class)) {
        err = PTR_ERR(class);
        goto fail;
    }

    devno = MKDEV(major, minor);
    cdev_init(&cdev, &fops);
    cdev.owner = THIS_MODULE;

    err = cdev_add(&cdev, devno, 1);
    if (err) {
        printk(KERN_WARNING "[Sandman] Error %d while trying to add %s%d",
               err, DEVICE_NAME, minor);
        goto fail;
    }

    device = device_create(class, NULL, devno, NULL, DEVICE_NAME "%d", minor);
    if (IS_ERR(device)) {
        err = PTR_ERR(device);
        printk(KERN_WARNING "[Sandman] Error %d while trying to create %s%d",
               err, DEVICE_NAME, minor);
        cdev_del(&cdev);
        goto fail;
    }
    printk(KERN_INFO "[Sandman] Driver module initialized.\n");
    return 0;

fail:
    return err;
}

static void __exit cleanup_sandman(void)
{
    device_destroy(class, MKDEV(major, NDEVICES));
    class_destroy(class);

    unregister_chrdev_region(MKDEV(major, 0), NDEVICES);
    printk(KERN_INFO "[Sandman] Driver module unloaded.\n");
}

module_init(init_sandman);
module_exit(cleanup_sandman);
