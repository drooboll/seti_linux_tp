#include "adxl345.h"


static int ADXL345_read_reg(struct i2c_client *client, adxl_reg_t reg, uint8_t* data)
{
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

    return i2c_transfer(client->adapter, msg, 2);
}

static int ADXL345_write_reg(struct i2c_client *client, adxl_reg_t reg, uint8_t* data)
{
    uint8_t transfer[2] = {reg, *data};

    return i2c_master_send(client, transfer, 2);
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
            .value = 0,
        },
        {
            .reg = ADXL345_DATA_FORMAT_REG,
            // Reset value
            .value = 0,
        },
        {
            .reg = ADXL345_FIFO_CTL_REG,
            // Bypass
            // TODO: make separate enum
            .value = 0,
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
        ret = ADXL345_write_reg(client, setup[i].reg, &(setup[i].value));

        //Debug
        uint8_t value = setup[i].value;
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

static int ADXL345_probe(struct i2c_client *client,
const struct i2c_device_id *id)
{
    printk(KERN_INFO "ADXL345 connected\n");

    uint8_t data;

    int ret = ADXL345_read_reg(client, ADXL345_DEVID_REG, &data);

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

    printk(KERN_WARNING "ADXL345 setup success\n");

    return 0;
}

static int ADXL345_remove(struct i2c_client *client)
{
    uint8_t data = 0;

    int ret = ADXL345_write_reg(client, ADXL345_POWER_CTL_REG, &data);

    if (ret < 0)
    {
        printk(KERN_WARNING "ADXL345 is not responding, error %d\n", ret);
        return ret;
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

module_i2c_driver(ADXL345_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ADXL345 driver");
MODULE_AUTHOR("Nobody");