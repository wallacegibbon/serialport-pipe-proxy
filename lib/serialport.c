#include "serialport.h"
#include "util.h"

int serialport_open(struct sp_port **port, const char *portname, int baudrate) {
	int ret;

	ret = sp_get_port_by_name(portname, port);
	if (ret < 0)
		return -1;

	ret = sp_open(*port, SP_MODE_READ_WRITE);
	if (ret < 0)
		return -2;

	sp_set_baudrate(*port, baudrate);
	sp_set_bits(*port, 8);
	sp_set_parity(*port, SP_PARITY_NONE);
	sp_set_stopbits(*port, 1);
	sp_set_flowcontrol(*port, SP_FLOWCONTROL_NONE);

	return 0;
}
