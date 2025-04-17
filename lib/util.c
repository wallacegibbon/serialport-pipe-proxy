#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64)
#include "windows.h"
#else
#include "unistd.h"
#include "time.h"
#endif

int uptime_ms(unsigned long long *v)
{
#if defined(_WIN32) || defined(_WIN64)
	*v = GetTickCount64();
#else
	struct timespec t;
	if (clock_gettime(CLOCK_BOOTTIME, &t))
		return -1;

	*v = (unsigned long long)(t.tv_sec * 1000) + t.tv_nsec / 1000000;
#endif
	return 0;
}

int sleep_ms(int milliseconds)
{
#if defined(_WIN32) || defined(_WIN64)
	Sleep(milliseconds);
#else
	if (usleep(milliseconds * 1000))
		return -1;
#endif
	return 0;
}

void exit_info(int code, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	exit(code);
}
