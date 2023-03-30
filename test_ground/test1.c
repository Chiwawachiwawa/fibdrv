#include <fcntl.h>
#include <math.h>
#include <stdint.h>
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


static long long fib_fd(long long n)
{
  if (n == 0) {
    return 0; // F(0) = 0.
  } else if (n <= 2) {
    return 1; // F(1) = F(2) = 0.
  }
  long long k = 0;
  if (n % 2) { // By F(n) = F(2k+1) = F(k+1)^2 + F(k)^2
    k = (n - 1) / 2;
    return fib_fd(k) * fib_fd(k) + fib_fd(k + 1) * fib_fd(k + 1);
  } else { // By F(n) = F(2k) = F(k) * [ 2 * F(k+1) – F(k) ]
    k = n / 2;
    return fib_fd(k) * (2 * fib_fd(k + 1) - fib_fd(k));
  }
}

static long long fib_sequence_clz(int k)//double_fib
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
int main()
{
    struct timespec t1, t2;
    char buf[93];
    int fd = open(FIB_DEV, O_RDWR);
    int offset = 93;
    for (int i = 0; i <= offset; i++) {
        clock_gettime(CLOCK_REALTIME, &t1);
        fib_fd(i);
        // write(fd, buf, i);//fdbbn
        clock_gettime(CLOCK_REALTIME, &t2);
        printf("%d ", i);
        printf("%ld ", elapse(t1, t2));
        clock_gettime(CLOCK_REALTIME, &t1);
        fib_sequence_clz(i);
        clock_gettime(CLOCK_REALTIME, &t2);
        printf("%ld\n", elapse(t1, t2));
    }
    return 0;
}
