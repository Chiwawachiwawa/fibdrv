#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

static inline long elapse(struct timespec start, struct timespec end)
{
    return ((long) 1.0e+9 * end.tv_sec + end.tv_nsec) -
           ((long) 1.0e+9 * start.tv_sec + start.tv_nsec);
}

int main()
{
    char buf[817];
    int offset = 817;

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }
    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        long fib_v1_time = read(fd, buf, 817);
        long fib_v0_time = write(fd, buf, 817);
        printf("%d ", i);
        printf("%ld ", fib_v1_time);
        printf("%ld\n", fib_v0_time);
    }

    close(fd);
    return 0;
}
