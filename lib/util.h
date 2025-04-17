#ifndef __UTIL_H
#define __UTIL_H

int uptime_ms(unsigned long long *v);
void sleep_ms(int milliseconds);
void exit_info(int code, const char *format, ...);

#endif
