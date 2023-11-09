#ifndef __SERIALPORT_H
#define __SERIALPORT_H

#include <libserialport.h>

int serialport_open(struct sp_port **port, const char *portname, int baudrate);

#endif
