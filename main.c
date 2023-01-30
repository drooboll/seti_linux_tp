#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>

typedef enum {
    ADXL345_AXIS_X = 0,
    ADXL345_AXIS_Y = 2,
    ADXL345_AXIS_Z = 4
} adxl_axis_t;

int msleep(unsigned int tms) {
  return usleep(tms * 1000);
}

int main(){
    int fd, ret;
    fd = open("/dev/adxl345-0", O_RDWR);
    if (fd < 0)
    {
        printf("Something went wrong\n");
        return -1;
    }

    ret = ioctl(fd, _IOW(10, ADXL345_AXIS_X, uint8_t));

    if (ret < 0)
    {
        printf("Error encountered ioctl_x: %d\n", ret);
    }

    char buffer[2];

    const int count = 2;

    for (int i = 0; i < count; i++) {
        ret = read(fd, buffer, 2);
        short actual_value = buffer[1] << 8 | buffer[0];
        printf("Read value is : %hi\n", actual_value);

        msleep(40);
    }

    ret = ioctl(fd, _IOW(10, ADXL345_AXIS_Y, uint8_t));

    if (ret < 0)
    {
        printf("Error encountered ioctl_y: %d\n", ret);
    }

    for (int i = 0; i < count; i++) {
        ret = read(fd, buffer, 2);
        short actual_value = buffer[1] << 8 | buffer[0];
        printf("Read value is : %hi\n", actual_value);

        msleep(40);
    }

    ret = ioctl(fd, _IOW(10, ADXL345_AXIS_Z, uint8_t));

    if (ret < 0)
    {
        printf("Error encountered ioctl_z: %d\n", ret);
    }

    for (int i = 0; i < count; i++) {
        ret = read(fd, buffer, 2);
        short actual_value = buffer[1] << 8 | buffer[0];
        printf("Read value is : %hi\n", actual_value);

        msleep(40);
    }

    if (ret < 0)
    {
        printf("Error encountered: %d\n", ret);
    }

    return 0;
}