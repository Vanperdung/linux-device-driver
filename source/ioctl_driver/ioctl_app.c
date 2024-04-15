#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#define WR_VALUE _IOW('a', 'a', int32_t*)
#define RD_VALUE _IOR('a', 'b', int32_t*)

int main(void)
{
    int fd, ret;
    int32_t read_val, write_val;

    fd = open("/dev/ioctl_dev", O_RDWR, 0666);
    if (fd < 0)
    {
        perror("Failed to open file");
        return -1;
    }

    write_val = 200;
    ret = ioctl(fd, WR_VALUE, &write_val);
    if (ret < 0)
    {
        perror("Failed to write");
        close(fd);
        return -1;
    }

    ret = ioctl(fd, RD_VALUE, &read_val);
    if (ret < 0)
    {
        perror("Failed to read");
        close(fd);
        return -1;
    }
    printf("Read value = %d\n", read_val);

    close(fd);
    return 0;
}