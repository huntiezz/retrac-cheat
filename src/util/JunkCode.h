#pragma once
#include <cmath>

#define JUNK_CODE_ONE \
    { \
        volatile int a = 42; \
        volatile int b = 1337; \
        volatile int c = a + b; \
        volatile float d = std::sin(c); \
        (void)d; \
    }

#define JUNK_CODE_TWO \
    { \
        volatile double x = 3.14159; \
        volatile double y = 2.71828; \
        volatile double z = x * y; \
        volatile double w = std::cos(z); \
        (void)w; \
    }

#define JUNK_CODE_THREE \
    { \
        volatile long long i = 0xDEADBEEF; \
        volatile long long j = 0xCAFEBABE; \
        volatile long long k = i ^ j; \
        (void)k; \
    }

#define JUNK_CODE_FOUR \
    { \
        volatile int arr[5] = {1, 2, 3, 4, 5}; \
        volatile int sum = 0; \
        for(int i=0; i<5; ++i) sum += arr[i]; \
        (void)sum; \
    }


#define JUNK_CODE_HEAVY \
    JUNK_CODE_ONE \
    JUNK_CODE_TWO \
    JUNK_CODE_THREE \
    JUNK_CODE_FOUR
