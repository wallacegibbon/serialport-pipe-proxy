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

struct application {
	/// parameters
	const char *serialport_device;
	int baudrate;
	const char *start_string;
	const char *end_string;

	/// running states
	struct sp_port *serialport;
	int running_flag;
	pthread_mutex_t running_flag_lock;
};

struct application app;

void app_initialize(struct application *self) {
	int r;
	pthread_mutex_init(&self->running_flag_lock, NULL);
	self->running_flag = 1;
	/// Open the serial port before starting serial port reading thread to avoid some error.
	r = serialport_open(&self->serialport, self->serialport_device, self->baudrate);
	if (r == -1)
		exit_info(r, "failed getting port by name \"%s\"\n", self->serialport_device);
	if (r == -2)
		exit_info(r, "failed opening port \"%s\"\n", self->serialport_device);
}

void app_cleanup(struct application *self) {
	int r;
	pthread_mutex_destroy(&self->running_flag_lock);
	r = sp_close(self->serialport);
	if (r < 0)
		exit_info(r, "failed closing port %s\n", self->serialport_device);
}

void app_describe(struct application *self) {
	fprintf(stderr, "application params\n\tport:%s, baudrate:%d, startstring:%s, endstring:%s\n",
		self->serialport_device, self->baudrate, self->start_string, self->end_string);
}

void app_running_flag_set(struct application *self, int new_value) {
	pthread_mutex_lock(&self->running_flag_lock);
	self->running_flag = new_value;
	pthread_mutex_unlock(&self->running_flag_lock);
}

int app_running_flag_get(struct application *self) {
	int is_active;
	pthread_mutex_lock(&self->running_flag_lock);
	is_active = self->running_flag;
	pthread_mutex_unlock(&self->running_flag_lock);
	return is_active;
}

void parse_arguments(int argc, const char **argv, struct application *app) {
	if (argc < 3)
		exit_info(1, "Usage sample: sp-pipe /dev/ttyUSB0 115200 ['start string'] ['end string']\n");

	app->serialport_device = argv[1];
	app->baudrate = atoi(argv[2]);
	app->start_string = NULL;
	app->end_string = NULL;

	if (argc >= 4)
		app->start_string = argv[3];
	if (argc >= 5)
		app->end_string = argv[4];
}

// clang-format off
enum serialport_fsm_state {FSM_WAITING_FOR_START, FSM_NORMAL1, FSM_NORMAL2, FSM_END};
// clang-format on

struct serialport_fsm {
	unsigned char buffer[SERIALPORT_READ_BUFFER_SIZE + 1];
	int cursor;
	int buffer_end;
	enum serialport_fsm_state state;
	struct str_detector detector;
	const char *start_string;
	const char *end_string;
};

void s_fsm_initialize(struct serialport_fsm *self, const char *start_string, const char *end_string) {
	if (start_string != NULL) {
		self->state = FSM_WAITING_FOR_START;
		sd_initialize(&self->detector, start_string);
	} else if (end_string != NULL) {
		self->state = FSM_NORMAL1;
		sd_initialize(&self->detector, end_string);
	} else {
		self->state = FSM_NORMAL2;
	}
	self->cursor = 0;
	self->buffer_end = 0;
	self->start_string = start_string;
	self->end_string = end_string;
}

int s_fsm_wait_for_start(struct serialport_fsm *self) {
	struct feed_result r;
	unsigned char *s;

	s = self->buffer + self->cursor;
	r = sd_feed(&self->detector, s, self->buffer_end);
	if (r.start < 0) {
		self->cursor = self->buffer_end;
		return 1;
	}
	fwrite(s + r.start, 1, r.end - r.start, stdout);
	self->cursor += r.end;
	if (self->end_string != NULL) {
		self->state = FSM_NORMAL1;
		sd_initialize(&self->detector, self->end_string);
	} else {
		self->state = FSM_NORMAL2;
	}
	return 1;
}

int s_fsm_normal1(struct serialport_fsm *self) {
	struct feed_result r;
	unsigned char *s;
	int size;

	s = self->buffer + self->cursor;
	size = self->buffer_end - self->cursor;
	r = sd_feed(&self->detector, s, self->buffer_end);
	if (r.start > 0) {
		self->cursor += r.end;
		size = r.end;
		self->state = FSM_END;
	} else {
		self->cursor = self->buffer_end;
	}
	fwrite(s, 1, size, stdout);

	return 1;
}

int s_fsm_normal2(struct serialport_fsm *self) {
	unsigned char *s;
	int size;

	s = self->buffer + self->cursor;
	size = self->buffer_end - self->cursor;
	self->cursor = self->buffer_end;
	fwrite(s, 1, size, stdout);

	return 1;
}

/// This function may call sp_blocking_read, so it can block.
void s_fsm_fill_more(struct serialport_fsm *self) {
	if (self->buffer_end == 0 || self->cursor == self->buffer_end) {
		self->cursor = 0;
		self->buffer_end = sp_blocking_read(app.serialport, self->buffer, SERIALPORT_READ_BUFFER_SIZE, 100);
		if (self->buffer_end < 0)
			exit_info(11, "failed reading from serial port\n");

		self->buffer[self->buffer_end] = '\0';
	}
}

int s_fsm_step(struct serialport_fsm *self) {
	/// extract FSM_END checking out from the switch to avoid unnecessary blocking.
	if (self->state == FSM_END)
		return 0;

	s_fsm_fill_more(self);

	switch (self->state) {
	case FSM_WAITING_FOR_START:
		return s_fsm_wait_for_start(self);
	case FSM_NORMAL1:
		return s_fsm_normal1(self);
	case FSM_NORMAL2:
		return s_fsm_normal2(self);
	}
}

static inline int s_fsm_buffer_empty(struct serialport_fsm *self) {
	return self->cursor == self->buffer_end;
}

void *serialport_data_handler(void *data) {
	struct serialport_fsm s_fsm;

	s_fsm_initialize(&s_fsm, app.start_string, app.end_string);

	/// `app_running_flag_get` have to be called after `s_fsm_step`.
	while (s_fsm_step(&s_fsm) && (!s_fsm_buffer_empty(&s_fsm) || app_running_flag_get(&app)))
		;

	app_running_flag_set(&app, 0);
}

void *stdin_data_handler(void *data) {
	unsigned char buffer[STDIN_READ_BUFFER_SIZE + 1];
	int has_more;
	int r;

	has_more = 1;
	while (has_more && app_running_flag_get(&app)) {
		r = fread(buffer, 1, STDIN_READ_BUFFER_SIZE, stdin);
		if (r < STDIN_READ_BUFFER_SIZE) {
			has_more = 0;
			if (!feof(stdin) && ferror(stdin))
				exit_info(3, "writting to serialport error: %d\n", errno);
		}
		r = sp_blocking_write(app.serialport, buffer, r, 0);
		if (r < 0)
			exit_info(2, "failed writting to serialport\n");
	}

	app_running_flag_set(&app, 0);
}

typedef void *(*pthread_start_routine)(void *);

int main(int argc, const char **argv) {
	pthread_t stdin_thread, serialport_thread;
	int r;

	parse_arguments(argc, argv, &app);
	app_initialize(&app);
	// app_describe(&app);

	r = pthread_create(&serialport_thread, NULL, (pthread_start_routine)serialport_data_handler, NULL);
	if (r)
		exit_info(2, "failed creating serialport thread\n");

	r = pthread_create(&stdin_thread, NULL, stdin_data_handler, NULL);
	if (r)
		exit_info(2, "failed creating stdin thread\n");

	pthread_join(stdin_thread, NULL);
	pthread_join(serialport_thread, NULL);

	app_cleanup(&app);

	return 0;
}
