#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/i2c.h>

#

typedef enum {
    ADXL345_DEVID_REG = 0x00,
    ADXL345_BW_RATE_REG = 0x2C,
    ADXL345_DATA_FORMAT_REG = 0x31,
    ADXL345_FIFO_CTL_REG = 0x38,
    ADXL345_POWER_CTL_REG = 0x2D,
    ADXL345_INT_ENABLE_REG = 0x2E
} adxl_reg_t;


typedef struct {
    adxl_reg_t reg;
    uint8_t value;
} adxl_reg_pair_t;