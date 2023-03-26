#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/ktime.h>

#include "bn.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 817

#define NUM_TYPE uint32_t
#define TMP_TYPE uint64_t
#define MAX_VAL (0xFFFFFFFF)
#define NUM_SZ (sizeof(NUM_TYPE))

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);

static long long fib_sequence_dp(long long k)
{
    long long f[k + 2];

    f[0] = 0;
    f[1] = 1;

    for (int i = 2; i <= k; i++) {
        f[i] = f[i - 1] + f[i - 2];
    }

    return f[k];
}

static long long fib_sequence(int k)
{
    if (k < 2)
    return k;

    long long first_element = 0;
    long long second_element = 1;
    long long cover_bits;
    for(int i = 31 - __builtin_clz(k); i>=0; --i){
        long long temp_1 = first_element * (2 * second_element - first_element);
        long long temp_2 = first_element * first_element + second_element * second_element;
        long long process_bits = 1UL<<i;
        cover_bits = -!!(k&(process_bits));
        first_element = (temp_1 & ~cover_bits) + (temp_2 & cover_bits);
        second_element = (temp_1 & cover_bits) + temp_2;
    }
    return first_element;
}

void fib_og_bn(bn *target, unsigned int n){
    bn_resize(target, 1);
    if (n < 2){
        target->digits[0] = n;
        return;
    }

    bn *fst_ele = bn_alloc(1);
    bn *sec_ele = bn_alloc(1);
    target->digits[0] = 1;

    for (unsigned int i = 1; i < n; i++){
        bn_swap(sec_ele, target);
        bn_add(fst_ele, sec_ele, target);
        bn_swap(fst_ele, sec_ele);
    }
    bn_free(fst_ele);
    bn_free(sec_ele);
}


void fib_fdbbn(bn *target, unsigned int n){
    bn_resize(target, 1);
    if (n < 2){
        target->digits[0] = n;
        return;
    }
    bn *f_k = target;
    bn *f_k_next = bn_alloc(1);
    f_k->digits[0] = 0;
    f_k_next->digits[0] = 1;
    bn *k = bn_alloc(1);
    bn *k_next = bn_alloc(1);

    for (unsigned int i = 1U << 31; i; i >>= 1){
        bn_copy(k, k_next);
        bn_lshift(k, 1, k);
        bn_sub(k, f_k, k);
        bn_mult(k, f_k, k);
        bn_mult(f_k, f_k, f_k);
        bn_mult(f_k_next, f_k_next, f_k_next);
        /* f(k)*f(k) + f(k+1)*f(k+1) */
        bn_copy(k_next,f_k);
        bn_add(k_next, f_k_next, k_next);

        if(n & i){
            bn_copy(f_k, k_next);
            bn_copy(f_k_next, k);
            bn_add(f_k_next, k_next, f_k_next);
        }
        else{
            bn_copy(f_k, k);
            bn_copy(f_k_next, k_next);
        }
    }
    bn_free(f_k_next);
    bn_free(k);
    bn_free(k_next);
}



static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    bn *fib = bn_alloc(1);
    ktime_t k1 = ktime_get();
    fib_og_bn(fib, *offset);
    ktime_t k2 = ktime_sub(ktime_get(), k1);
    char *p = bn_to_string(*fib);
    size_t len = strlen(p) + 1;
    // copy_to_user(buf, fib->number, sizeof(unsigned int)*size);
    copy_to_user(buf, p, len);
    bn_free(fib);
    return ktime_to_ns(k2);
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    bn *fib = bn_alloc(1);
    ktime_t k1 = ktime_get();
    fib_fdbbn(fib, *offset);
    ktime_t k2 = ktime_sub(ktime_get(), k1);
    char *p = bn_to_string(*fib);
    size_t len = strlen(p) + 1;
    copy_to_user(buf, p, len);
    bn_free(fib);
    kfree(p);
    return ktime_to_ns(k2);
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);