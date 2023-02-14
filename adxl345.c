#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/interrupt.h>

#include "adxl345.h"

static int dev_count = 0;

struct adxl_association_s* subscribers = NULL;
DEFINE_MUTEX(i2c_lock);
DEFINE_MUTEX(list_lock);
DEFINE_MUTEX(fifo_read_lock);

static struct adxl_association_s* find_subscriber(pid_t pid)
{
    int res;
    struct adxl_association_s* curr = subscribers;

    res = mutex_lock_killable(&list_lock);

    if (res != 0){
        printk(KERN_WARNING "Process with pid %d killed while taking the lock", current->pid);
        return NULL;
    }

    while (curr != NULL)
    {

        if (curr->pid == pid){
            break;
        }

        curr = curr->next;
    }

    mutex_unlock(&list_lock);

    return curr;
}

static struct adxl_association_s* last_subscriber(void)
{
    int res;
    struct adxl_association_s* curr = subscribers;

    res = mutex_lock_killable(&list_lock);

    if (res != 0){
        printk(KERN_WARNING "Process with pid %d killed while taking the lock", current->pid);
        return NULL;
    }

    if (curr != NULL)
    {
        while (curr->next != NULL)
        {
            curr = curr->next;
        }
    }

    mutex_unlock(&list_lock);

    return curr;
}   

static void add_subscriber(struct adxl_association_s* sub)
{
    int res;
    struct adxl_association_s* curr = subscribers;

    res = mutex_lock_killable(&list_lock);

    if (res != 0){
        printk(KERN_WARNING "Process with pid %d killed while taking the lock", current->pid);
        return;
    }

    if (subscribers == NULL)
    {
        subscribers = sub;
    }
    else 
    {
        while (curr->next != NULL)
        {
            printk(KERN_INFO "Curr pid is %d\n", curr->pid);
            curr = curr->next;
        }

        curr->next = sub;
    }

    mutex_unlock(&list_lock);
}

static void remove_subscriber(struct adxl_association_s* sub)
{
    int res;
    struct adxl_association_s* curr = subscribers;
    struct adxl_association_s* prev = subscribers;

    res = mutex_lock_killable(&list_lock);

    if (res != 0){
        printk(KERN_WARNING "Process with pid %d killed while taking the lock", current->pid);
        return;
    }

    if (sub == subscribers)
    {
        subscribers = sub->next;
    }
    else
    {
        while (curr != NULL)
        {
            if (curr->next == sub)
            {
                if (sub->next == NULL)
                {
                    curr->next = NULL;
                } else {
                    curr->next = sub->next;
                }

                break;
            }

            prev = curr;
            curr = curr->next;
        }
    }
    mutex_unlock(&list_lock);
}

static int ADXL345_read_reg(struct i2c_client *client, adxl_reg_t reg, uint8_t* data)
{
    int res;

    struct i2c_msg msg[2] = {
        {
            .addr = client->addr,
            .len = 1,
            .buf = (uint8_t*) &reg,
        },
        {
            .addr = client->addr,
            .len = 1,
            .buf = data,
            .flags = I2C_M_RD,
        }
    };

    res = mutex_lock_killable(&i2c_lock);

    if (res != 0){
        printk(KERN_WARNING "Process with pid %d killed while taking the lock in i2c", current->pid);
        return res;
    }

    res = i2c_transfer(client->adapter, msg, 2);

    mutex_unlock(&i2c_lock);

    return res;
}

static int ADXL345_write_reg(struct i2c_client *client, adxl_reg_t reg, uint8_t* data)
{
    int res;

    uint8_t transfer[2] = {reg, *data};

    res = mutex_lock_killable(&i2c_lock);

    if (res != 0){
        printk(KERN_WARNING "Process with pid %d killed while taking the lock in i2c", current->pid);
        return res;
    }

    res = i2c_master_send(client, transfer, 2);

    mutex_unlock(&i2c_lock);

    return res;
}

static int ADXL345_setup(struct i2c_client *client)
{
    adxl_reg_pair_t setup[5] = {
        {
            .reg = ADXL345_BW_RATE_REG,
            .value = 0x0A,
        },
        {
            .reg = ADXL345_INT_ENABLE_REG,
            // Disable all ints
            // TODO: make separate enum
            .value = 1 << 1,
        },
        {
            .reg = ADXL345_DATA_FORMAT_REG,
            // Reset value
            .value = 0,
        },
        {
            .reg = ADXL345_FIFO_CTL_REG,
            // Fill untill full
            // 20 samples max
            // TODO: make separate enum
            .value = 1 << 7 | WATERFILL_LIMIT,
        },
        {
            .reg = ADXL345_POWER_CTL_REG,
            // Active measure
            // TODO: separate enum
            .value = 1 << 3,
        },
    };

    int ret = 0;
    int i;

    for (i = 0; i < 5; ++i)
    {
        uint8_t value;
        ret = ADXL345_write_reg(client, setup[i].reg, &(setup[i].value));

        //Debug
        value = setup[i].value;
        ADXL345_read_reg(client, setup[i].reg, &(setup[i].value));

        printk(KERN_INFO "ADX345 setup: reg 0x%X written: 0x%X, read 0x%X\n", setup[i].reg, value, setup[i].value);
        // Debug end

        if (ret < 0)
        {
            break;
        }
    }

    return ret;
}

static int ADXL345_read_axis(struct i2c_client *client, int16_t* buf)
{
    size_t i;
    int res;
    uint8_t reg = ADXL345_DATAX0_REG;
    uint8_t data[6];
    struct i2c_msg msg[2] = {
        {
            .addr = client->addr,
            .len = 1,
            .buf = (uint8_t*) &reg,
        },
        {
            .addr = client->addr,
            .len = 6,
            .buf = data,
            .flags = I2C_M_RD,
        }
    };

    res = mutex_lock_killable(&i2c_lock);

    if (res != 0){
        printk(KERN_WARNING "Process with pid %d killed while taking the lock in i2c", current->pid);
        return res;
    }

    res = i2c_transfer(client->adapter, msg, 2);

    mutex_unlock(&i2c_lock);
    
    for (i = 0; i < 6; i += 2)
    {
        int16_t raw = data[i + 1];
        raw = raw << 8 | data[i];
        buf[i / 2] = raw;
    }

    return res;
}

static bool data_is_available(struct adxl345_device* dev)
{
    return !kfifo_is_empty(&(dev->fifo_samples));
}

struct file_operations fops = {
    .read = adxl345_read,
    .unlocked_ioctl = adxl345_ioctl,
    .open = adxl345_open,
    .release = adxl345_release
};

static int ADXL345_probe(struct i2c_client *client,
const struct i2c_device_id *id)
{
    uint8_t data;
    int ret;
    struct adxl345_device* dev;
    char* name_buf;
    int misc;

    printk(KERN_INFO "ADXL345 connected\n");

    ret = ADXL345_read_reg(client, ADXL345_DEVID_REG, &data);

    if (ret < 0)
    {
        printk(KERN_WARNING "ADXL345 is not responding, error %d\n", ret);
        return ret;
    }

    printk(KERN_INFO "ADXL345 DEVID value is 0x%X\n", data);

    ret = ADXL345_setup(client);

    if (ret < 0)
    {
        printk(KERN_WARNING "ADXL345 setup error %d\n", ret);
        return ret;
    }

    printk(KERN_INFO "ADXL345 setup success\n");

    dev = kzalloc(sizeof(struct adxl345_device), GFP_KERNEL);

    name_buf = kasprintf(GFP_KERNEL, "adxl345-%d", dev_count);

    if (dev == NULL || name_buf == NULL)
    {
        printk(KERN_ERR "ADXL345 error memory allocation\n");
        return -1;
    }

    INIT_KFIFO(dev->fifo_samples);

    printk(KERN_INFO "ADXL345 memory alloc ok\n");

    dev->miscdev.minor = MISC_DYNAMIC_MINOR;
    dev->miscdev.name = name_buf;
    dev->miscdev.parent = &(client->dev);
    dev->miscdev.fops = &fops;
    
    i2c_set_clientdata(client, dev);

    printk(KERN_INFO "ADXL345 misc init ok\n");

    misc = misc_register(&(dev->miscdev));

    printk(KERN_INFO "ADXL345 misc register ok\n");

    dev_count += 1;
    printk(KERN_INFO "ADXL345: init as /dev/%s\n", dev->miscdev.name);

    init_waitqueue_head(&(dev->queue));

    ret = devm_request_threaded_irq(
        &(client->dev),
        client->irq,
        NULL,
        adxl345_irq,
        IRQF_ONESHOT,
        dev->miscdev.name,
        dev
    );

    printk(KERN_INFO "IRQ registered: %d\n", ret);

    return 0;
}

static int ADXL345_remove(struct i2c_client *client)
{
    uint8_t data;
    int ret;
    struct adxl345_device* dev;
    struct adxl_association_s* last;

    data = 0;
    ret = ADXL345_write_reg(client, ADXL345_POWER_CTL_REG, &data);

    if (ret < 0)
    {
        printk(KERN_WARNING "ADXL345 is not responding, error %d\n", ret);
        return ret;
    }

    dev_count -= 1;
    dev = i2c_get_clientdata(client);

    devm_free_irq(&(client->dev), client->irq, dev);
    misc_deregister(&(dev->miscdev));
    kfree(dev->miscdev.name);
    kfree(dev);

    last = last_subscriber();

    while (last != NULL)
    {
        remove_subscriber(last);
        kfree(last);
        last = last_subscriber();
    }

    printk(KERN_INFO "ADXL345 disconnected\n");
    return 0;
}

/* The following list allows the association between a device and its driver
driver in the case of a static initialization without using
device tree.
Each entry contains a string used to make the association
association and an integer that can be used by the driver to
driver to perform different treatments depending on the physical
the physical device detected (case of a driver that can manage
different device models).*/
static struct i2c_device_id ADXL345_idtable[] = {
    { "ADXL345", 0 },
    { }
};

MODULE_DEVICE_TABLE(i2c, ADXL345_idtable);
#ifdef CONFIG_OF
/* If device tree support is available, the following list
allows to make the association using the device tree.
Each entry contains a structure of type of_device_id. The field
compatible field is a string that is used to make the association
with the compatible fields in the device tree. The data field is
a void* pointer that can be used by the driver to perform different
perform different treatments depending on the physical device detected.
device detected.*/
static const struct of_device_id ADXL345_of_match[] = {
    { .compatible = "qemu,ADXL345",
    .data = NULL },
    {}
};

MODULE_DEVICE_TABLE(of, ADXL345_of_match);
#endif
static struct i2c_driver ADXL345_driver = {
    .driver = {
    /* The name field must correspond to the name of the module and must not contain spaces. */
        .name = "ADXL345",
        .of_match_table = of_match_ptr(ADXL345_of_match),
    },
    .id_table = ADXL345_idtable,
    .probe = ADXL345_probe,
    .remove = ADXL345_remove,
};

ssize_t adxl345_read(struct file * file, char __user * buf, size_t count, loff_t * f_pos)
{
    struct miscdevice* misc;
    struct adxl345_device* dev;
    struct i2c_client* client;
    pid_t pid;
    struct adxl_association_s* sub;
    size_t read_offset;
    struct adxl345_measurement el;

    misc = (struct miscdevice*) (file->private_data);
    dev = container_of(misc, struct adxl345_device, miscdev);
    client = to_i2c_client(dev->miscdev.parent);

    pid = current->pid;
    sub = find_subscriber(pid);

    printk(KERN_INFO "Read %d bytes by pid %d\n", count, current->pid);

    if (sub == NULL)
    {
        printk(KERN_ERR "READ called, but pid %d is not registered!\n", pid);
        return -ESRCH;
    }

    read_offset = 0;

    while (read_offset != count){
        char* data_pointer;
        int res;

        res = mutex_lock_killable(&fifo_read_lock);

        if (res != 0){
            return res; // Normally only -EINTR
        }

        res = kfifo_get(&(dev->fifo_samples), &el);

        printk(KERN_INFO "In FIFO: %d\n", kfifo_avail(&(dev->fifo_samples)));
        mutex_unlock(&fifo_read_lock);

        if (res == 0)
        {
            printk(KERN_INFO "Not enough data, sleeping\n");
            res = wait_event_killable(dev->queue, data_is_available(dev));

            if (res != 0){
                printk(KERN_INFO "Process with pid %d killed while waiting for data\n", current->pid);
                return res;
            }

            printk(KERN_INFO "Data available, wake up\n");
            continue;
        }

        switch(sub->axis)
        {
            case ADXL345_AXIS_X: data_pointer = (char*) &(el.x); break;
            case ADXL345_AXIS_Y: data_pointer = (char*) &(el.y); break;
            case ADXL345_AXIS_Z: data_pointer = (char*) &(el.z); break;
            default: break; // Shouldn't happen
        }

        if (copy_to_user(buf + read_offset, data_pointer, 2))
        {
            return -EFAULT;
        }

        read_offset += 2;
    }

    return count;
}

long adxl345_ioctl(struct file* file, unsigned int cmd, unsigned long arg)
{
    pid_t pid;
    struct adxl_association_s* sub;

    printk(KERN_INFO "Command data: %x\n", cmd);

    pid = current->pid;
    sub = find_subscriber(pid);

    if (sub == NULL)
    {
        printk(KERN_ERR "IOCTL called, but pid %d is not registered!\n", pid);
        return -ESRCH;
    }

    switch (cmd)
    {
        case _IOW(MISC_MAJOR, ADXL345_AXIS_X, uint8_t):
            sub->axis = (adxl_axis_t) ADXL345_AXIS_X;
            return 0;
        case _IOW(MISC_MAJOR, ADXL345_AXIS_Y, uint8_t):
            sub->axis = (adxl_axis_t) ADXL345_AXIS_Y;
            return 0;
        case _IOW(MISC_MAJOR, ADXL345_AXIS_Z, uint8_t):
            sub->axis = (adxl_axis_t) ADXL345_AXIS_Z;
            return 0;
        default:
            printk(KERN_WARNING "ADXL345 illegal ioctl command: %d\n", cmd);
            return -EINVAL;
    }
}

int adxl345_open(struct inode * inode, struct file * file)
{
    // If the process opens our file 2 times,
    // it won't be different
    pid_t pid;
    struct adxl_association_s* assoc;

    pid = current->pid;

    printk(KERN_INFO "Open by %d", pid);
    
    assoc = kmalloc(sizeof(struct adxl_association_s), GFP_KERNEL);
    assoc->pid = pid;
    assoc->axis = ADXL345_AXIS_X;
    assoc->next = NULL;

    printk(KERN_INFO "Adding in %d", pid);
    add_subscriber(assoc);

    return 0;
}

int adxl345_release(struct inode * inode, struct file * file)
{
    // If the process opens our file 2 times,
    // it won't be different
    pid_t pid;
    struct adxl_association_s* sub;

    pid = current->pid;
    sub = find_subscriber(pid);

    printk(KERN_INFO "Release by %d", pid);

    if (sub == NULL)
    {
        printk(KERN_ERR "CLOSE called, but pid %d is not registered!\n", pid);
        return -ESRCH;
    }

    remove_subscriber(sub);
    kfree(sub);

    return 0;
}

irqreturn_t adxl345_irq(int irq, void* dev_id)
{
    struct adxl345_device* dev = (struct adxl345_device*) dev_id;
    struct i2c_client* client = to_i2c_client(dev->miscdev.parent);

    size_t i;

    for (i = 0; i < WATERFILL_LIMIT; ++i)
    {
        int16_t buf[3];
        struct adxl345_measurement meas;

        ADXL345_read_axis(client, buf);

        meas.x = buf[0];
        meas.y = buf[1];
        meas.z = buf[2];

        kfifo_put(&(dev->fifo_samples), meas);
    }

    wake_up(&(dev->queue));

    return IRQ_HANDLED;
}

module_i2c_driver(ADXL345_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ADXL345 driver");
MODULE_AUTHOR("Nobody");
