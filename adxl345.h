#include <linux/miscdevice.h>
#include <linux/kfifo.h>

#define MISC_MAJOR 10
#define WATERFILL_LIMIT 20

typedef enum {
    ADXL345_DEVID_REG = 0x00,
    ADXL345_BW_RATE_REG = 0x2C,
    ADXL345_DATA_FORMAT_REG = 0x31,
    ADXL345_FIFO_CTL_REG = 0x38,
    ADXL345_POWER_CTL_REG = 0x2D,
    ADXL345_INT_ENABLE_REG = 0x2E,
    ADXL345_INT_MAP_REG = 0x2F,
    ADXL345_DATAX0_REG = 0x32,
    ADXL345_DATAX1_REG = 0x33,
    ADXL345_DATAY0_REG = 0x34,
    ADXL345_DATAY1_REG = 0x35,
    ADXL345_DATAZ0_REG = 0x36,
    ADXL345_DATAZ1_REG = 0x37,
} adxl_reg_t;

typedef enum {
    ADXL345_AXIS_X = 0,
    ADXL345_AXIS_Y = 2,
    ADXL345_AXIS_Z = 4
} adxl_axis_t;

typedef struct {
    adxl_reg_t reg;
    uint8_t value;
} adxl_reg_pair_t;

struct adxl_association_s {
    pid_t pid;
    adxl_axis_t axis;
    struct adxl_association_s* next;
};

struct adxl345_measurement {
    uint16_t x;
    uint16_t y;
    uint16_t z;
};

struct adxl345_device {
    struct miscdevice miscdev;
    struct wait_queue_head queue;
    DECLARE_KFIFO(fifo_samples, struct adxl345_measurement, 64);
};

ssize_t adxl345_read(struct file * file, char __user * buf, size_t count, loff_t * f_pos);
long adxl345_ioctl(struct file* file, unsigned int cmd, unsigned long arg);
int adxl345_open(struct inode * inode, struct file * file);
int adxl345_release(struct inode * inode, struct file * file);
irqreturn_t adxl345_irq(int irq, void* dev_id);