#include "serialport.h"
#include "str_detector.h"
#include "util.h"
#include <errno.h>
#include <libserialport.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define SERIALPORT_READ_BUFFER_SIZE 4096
#define STDIN_READ_BUFFER_SIZE 4096

/// flag protected by a mutex for checking whether this program has finished.
struct {
	pthread_mutex_t mutex;
	int flag;
} port_active;

/// the global variable shared by 2 threads.
struct sp_port *port;

struct application_params {
	const char *serialport_device;
	int baudrate;
	const char *start_string;
	const char *end_string;
};

void application_params_print(struct application_params *params) {
	fprintf(stderr, "application params\n\tport:%s, baudrate:%d, startstring:%s, endstring:%s\n",
		params->serialport_device, params->baudrate, params->start_string, params->end_string);
}

void parse_arguments(int argc, const char **argv, struct application_params *params) {
	if (argc < 3)
		exit_info(1, "Usage sample: sp-pipe /dev/ttyUSB0 115200 ['start string'] ['end string']\n");

	params->serialport_device = argv[1];
	params->baudrate = atoi(argv[2]);
	params->start_string = NULL;
	params->end_string = NULL;

	if (argc >= 4)
		params->start_string = argv[3];
	if (argc >= 5)
		params->end_string = argv[4];
}

void port_active_set(int new_value) {
	pthread_mutex_lock(&port_active.mutex);
	port_active.flag = new_value;
	pthread_mutex_unlock(&port_active.mutex);
}

int port_active_get() {
	int is_active;
	pthread_mutex_lock(&port_active.mutex);
	is_active = port_active.flag;
	pthread_mutex_unlock(&port_active.mutex);
	return is_active;
}

// clang-format off
enum serialport_fsm_state {FSM_WAITING_FOR_START, FSM_NORMAL, FSM_END};
// clang-format on

struct serialport_fsm {
	unsigned char buffer[SERIALPORT_READ_BUFFER_SIZE + 1];
	int cursor;
	int buffer_end;
	enum serialport_fsm_state state;
	int use_detector;
	struct str_detector detector;
	const char *start_string;
	const char *end_string;
};

void s_fsm_initialize(struct serialport_fsm *self, const char *start_string, const char *end_string) {
	self->use_detector = 1;
	if (start_string != NULL) {
		self->state = FSM_WAITING_FOR_START;
		sd_initialize(&self->detector, start_string);
	} else if (end_string != NULL) {
		self->state = FSM_NORMAL;
		sd_initialize(&self->detector, end_string);
	} else {
		self->state = FSM_NORMAL;
		self->use_detector = 0;
	}
	self->cursor = 0;
	self->start_string = start_string;
	self->end_string = end_string;
}

int s_fsm_wait_for_start(struct serialport_fsm *self) {
	unsigned char *s = self->buffer + self->cursor;
	struct feed_result r;

	r = sd_feed(&self->detector, s, self->buffer_end);
	if (r.start < 0) {
		self->cursor = self->buffer_end;
		return 0;
	}

	fwrite(s + r.start, 1, r.end - r.start, stdout);

	self->cursor += r.end;
	self->state = FSM_NORMAL;
	sd_initialize(&self->detector, self->end_string);
	return 0;
}

int s_fsm_normal(struct serialport_fsm *self) {
	unsigned char *s = self->buffer + self->cursor;
	struct feed_result r;
	int size = self->buffer_end - self->cursor;

	if (self->use_detector) {
		r = sd_feed(&self->detector, s, self->buffer_end);
		if (r.start > 0) {
			size = r.end;
			self->state = FSM_END;
		} else {
			self->cursor = self->buffer_end;
		}
	}
	fwrite(s, 1, size, stdout);
	return 0;
}

int s_fsm_step(struct serialport_fsm *self) {
	if (self->state == FSM_END)
		return 1;

	if (self->buffer_end == 0 || self->cursor == self->buffer_end) {
		self->cursor = 0;
		self->buffer_end = sp_blocking_read(port, self->buffer, SERIALPORT_READ_BUFFER_SIZE, 100);
		if (self->buffer_end < 0)
			exit_info(11, "failed reading from serial port\n");

		self->buffer[self->buffer_end] = '\0';
	}

	switch (self->state) {
	case FSM_WAITING_FOR_START:
		return s_fsm_wait_for_start(self);
	case FSM_NORMAL:
		return s_fsm_normal(self);
	}
}

void *serialport_data_handler(struct application_params *app_params) {
	struct serialport_fsm s_fsm;

	s_fsm_initialize(&s_fsm, app_params->start_string, app_params->end_string);

	while (port_active_get() || (s_fsm.state != FSM_END && s_fsm.cursor != s_fsm.buffer_end)) {
		if (s_fsm_step(&s_fsm))
			break;
	}

	port_active_set(0);
}

void *stdin_data_handler(void *data) {
	unsigned char buffer[STDIN_READ_BUFFER_SIZE + 1];
	int r;

	while (port_active_get()) {
		r = fread(buffer, 1, STDIN_READ_BUFFER_SIZE, stdin);
		if (r < STDIN_READ_BUFFER_SIZE) {
			port_active_set(0);
			if (!feof(stdin) && ferror(stdin))
				exit_info(3, "writting to serialport error: %d\n", errno);
		}
		r = sp_blocking_write(port, buffer, r, 0);
		if (r < 0)
			exit_info(2, "failed writting to serialport\n");
	}

	/*
	while (port_active_get()) {
		if (!feof(stdin)) {
			r = fgetc(stdin);
			r = sp_blocking_write(port, &r, 1, 0);
			if (r < 0)
				exit_info(2, "failed writting to serialport\n");
		} else {
			port_active_set(0);
		}
	}
	*/
}

void initialize_port(struct application_params *app_params) {
	int r;
	r = serialport_open(&port, app_params->serialport_device, app_params->baudrate);
	if (r == -1)
		exit_info(r, "failed getting port by name \"%s\"\n", app_params->serialport_device);
	if (r == -2)
		exit_info(r, "failed opening port \"%s\"\n", app_params->serialport_device);
}

int main(int argc, const char **argv) {
	pthread_t stdin_thread, serialport_thread;
	int r;
	struct application_params params;

	parse_arguments(argc, argv, &params);
	//application_params_print(&params);

	/// Open the serial port before starting serial port reading thread to avoid some error.
	initialize_port(&params);

	pthread_mutex_init(&port_active.mutex, NULL);
	port_active.flag = 1;

	r = pthread_create(&serialport_thread, NULL, (void *(*)(void *))serialport_data_handler, &params);
	if (r)
		exit_info(2, "failed creating serialport thread\n");

	r = pthread_create(&stdin_thread, NULL, stdin_data_handler, NULL);
	if (r)
		exit_info(2, "failed creating stdin thread\n");

	pthread_join(stdin_thread, NULL);
	pthread_join(serialport_thread, NULL);

	pthread_mutex_destroy(&port_active.mutex);

	r = sp_close(port);
	if (r < 0)
		exit_info(r, "failed closing port %s\n", params.serialport_device);

	return 0;
}
