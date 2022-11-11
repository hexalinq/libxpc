#pragma once
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <libusb-1.0/libusb.h>

typedef unsigned char uchar;

#define crash(format, ...) do { \
	fprintf(stderr, "%s:%u:%s | %s (%s) | " format "\n", __FILE__, __LINE__, __FUNCTION__, strerrorname_np(errno), strerrordesc_np(errno), ##__VA_ARGS__); \
	_Exit(1); \
} while(0);

#define FALSE 0
#define TRUE 1

#define INLINE inline

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define FOR_RANGE(variable, limit) for(size_t variable = 0; variable < limit; ++variable)

static inline double timef(void) {
	struct timespec tTime;
	if(clock_gettime(CLOCK_MONOTONIC_RAW, &tTime)) return NAN;
	return (double)tTime.tv_sec + (double)tTime.tv_nsec * 1.0e-9;
}
