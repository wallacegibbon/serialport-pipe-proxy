#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64)
#include "windows.h"
#else
#include "unistd.h"
#endif

void sleep_ms(int milliseconds)
{
#if defined(_WIN32) || defined(_WIN64)
	Sleep(milliseconds);
#else
	usleep(milliseconds * 1000);
#endif
}

void exit_info(int code, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	exit(code);
}
