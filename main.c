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

int main(){
    int fd = open("/dev/adxl345-0", O_RDWR);
    if (fd < 0)
    {
        printf("Something went wrong\n");
        return -1;
    }

    int ret;
    adxl_axis_t testcase[3] = {ADXL345_AXIS_X, ADXL345_AXIS_Y, ADXL345_AXIS_Z};

    for (int i = 0; i < 3; i++)
    {
        ret = ioctl(fd, _IOW(10, testcase[i], uint8_t));

        if (ret < 0) {
            printf("Error encountered ioctl_x: %d\n", ret);
        }

        char buffer[2];
        const int count = 100;

        for (int j = 0; j < count; j++) {
            ret = read(fd, buffer, 2);
            short actual_value = buffer[1] << 8 | buffer[0];
            printf("Read value of %d is : %hi\n", getpid(), actual_value);
        }
    }

    close(fd);
}