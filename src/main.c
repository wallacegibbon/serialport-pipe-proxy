#include "cmd_argument_parser.h"
#include "serialport.h"
#include "str_matcher.h"
#include "str_fixer.h"
#include "util.h"
#include <errno.h>
#include <libserialport.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SERIALPORT_READ_BUFFER_SIZE 4096
#define STDIN_READ_BUFFER_SIZE 4096

struct application {
	/* const parameters */
	const char *serialport_device;
	int baudrate;
	const char *start_string;
	const char *end_string;
	const char *output_file;
	char pipe_stdin;
	char debug;

	/* running states */
	struct sp_port *serialport;
	int running_flag;
	pthread_mutex_t running_flag_lock;
	FILE *output_file_handle;
};

struct application app;

/* Methods for struct application can call exit_info directly. */

const char *adjust_escaped_string(const char *s)
{
	struct str_fixer fixer;
	int i;

	sf_init(&fixer, s);
	if (sf_convert(&fixer) == 0)
		return (const char *)fixer.output;
	else
		return NULL;
}

void app_init(struct application *self)
{
	int r;

	pthread_mutex_init(&self->running_flag_lock, NULL);
	self->running_flag = 1;

	/*
	 * Open the serial port before starting serial port reading thread
	 * to avoid some error.
	 */
	r = serialport_open(&self->serialport, self->serialport_device,
				self->baudrate);
	if (r == -1)
		exit_info(r, "failed getting port by name \"%s\"\n",
				self->serialport_device);
	if (r == -2)
		exit_info(r, "failed opening port \"%s\"\n",
				self->serialport_device);

	if (self->output_file == NULL)
		self->output_file_handle = stdout;
	else
		self->output_file_handle = fopen(self->output_file, "w");

	if (self->output_file_handle == NULL)
		exit_info(3, "failed opening output file: %d\n",
				self->output_file);

	if (self->start_string)
		self->start_string = adjust_escaped_string(self->start_string);
	if (self->end_string)
		self->end_string = adjust_escaped_string(self->end_string);
}

void app_deinit(struct application *self)
{
	int r;
	pthread_mutex_destroy(&self->running_flag_lock);
	r = sp_close(self->serialport);
	if (r < 0)
		exit_info(r, "failed closing port %s\n",
				self->serialport_device);

	r = fclose(self->output_file_handle);
	if (r)
		exit_info(12, "failed closing output file %s\n",
				self->output_file);

	/*
	 * start_string and end_string points to memories `malloc`ed
	 * by `str_fixer`.
	 */

	free((void *)self->start_string);
	free((void *)self->end_string);
}

void app_describe(struct application *self)
{
	fprintf(stderr, "application params\n\tport:%s, baudrate:%d, "
			"start_string:%s, end_string:%s, "
			"pipe_stdin: %d, debug: %d\n",
			self->serialport_device, self->baudrate,
			self->start_string, self->end_string,
			self->pipe_stdin, self->debug);
}

void app_running_flag_set(struct application *self, int new_value)
{
	pthread_mutex_lock(&self->running_flag_lock);
	self->running_flag = new_value;
	pthread_mutex_unlock(&self->running_flag_lock);
}

int app_running_flag_get(struct application *self)
{
	int is_active;
	pthread_mutex_lock(&self->running_flag_lock);
	is_active = self->running_flag;
	pthread_mutex_unlock(&self->running_flag_lock);
	return is_active;
}

void parse_arguments(int argc, const char **argv, struct application *app)
{
	struct cmd_argument_parser parser;

	if (argc == 1)
		exit_info(1, "Usage: sp-pipe\t--port\t\t/dev/ttyUSB0\n"
				"\t\t--baudrate\t115200\n"
				"\t\t--start-string\t'a\\x62c'\n"
				"\t\t--end-string\t'd\\x65f'\n"
				"\t\t--pipe-stdin\n"
				"\t\t--debug\n");

	cmd_argument_parser_init(&parser, argc - 1, argv + 1);
	/*
	cmd_argument_parser_describe(&parser);
	*/

	app->serialport_device = cmd_argument_parser_get(&parser, "port",
			NULL);

	app->baudrate = atoi(cmd_argument_parser_get(&parser, "baudrate",
			"115200"));

	app->start_string = cmd_argument_parser_get(&parser, "start-string",
			NULL);

	app->end_string = cmd_argument_parser_get(&parser, "end-string",
			NULL);

	app->output_file = cmd_argument_parser_get(&parser, "output-file",
			NULL);

	/*
	cmd_argument_parser_describe(&parser);
	*/

	app->pipe_stdin = cmd_argument_parser_has(&parser, "pipe-stdin");
	app->debug = cmd_argument_parser_has(&parser, "debug");

	cmd_argument_parser_deinit(&parser);

	if (app->serialport_device == NULL)
		exit_info(2, "Serial port is not specified\n");
}

enum serialport_fsm_state {
	FSM_WAITING_FOR_START,
	FSM_NORMAL1,
	FSM_NORMAL2,
	FSM_END,
	FSM_READ_ERROR
};

struct serialport_fsm {
	char buffer[SERIALPORT_READ_BUFFER_SIZE + 1];
	int cursor;
	int data_end;
	enum serialport_fsm_state state;
	struct str_matcher matcher;
	const char *start_string;
	const char *end_string;
};

void s_fsm_init(struct serialport_fsm *self,
		const char *start_string, const char *end_string)
{
	if (start_string != NULL) {
		self->state = FSM_WAITING_FOR_START;
		sm_init(&self->matcher, start_string);
	} else if (end_string != NULL) {
		self->state = FSM_NORMAL1;
		sm_init(&self->matcher, end_string);
	} else {
		self->state = FSM_NORMAL2;
	}
	self->cursor = 0;
	self->data_end = 0;
	self->start_string = start_string;
	self->end_string = end_string;
}

int s_fsm_wait_for_start(struct serialport_fsm *self)
{
	int start = 0, end = 0, size;
	char *s;

	s = self->buffer + self->cursor;
	size = self->data_end - self->cursor;
	if (sm_feed(&self->matcher, s, size, &start, &end)) {
		self->cursor = self->data_end;
		return 0;
	}

	self->cursor += end;

	fwrite(s + start, 1, end - start, app.output_file_handle);
	fflush(app.output_file_handle);

	if (self->end_string != NULL) {
		self->state = FSM_NORMAL1;
		sm_init(&self->matcher, self->end_string);
	} else {
		self->state = FSM_NORMAL2;
	}
	return 0;
}

int s_fsm_normal1(struct serialport_fsm *self)
{
	int start = 0, end = 0, size;
	char *s;

	s = self->buffer + self->cursor;
	size = self->data_end - self->cursor;

	if (sm_feed(&self->matcher, s, size, &start, &end)) {
		self->cursor = self->data_end;
	} else {
		self->state = FSM_END;
		self->cursor += end;
		size = end;
	}

	fwrite(s, 1, size, app.output_file_handle);
	fflush(app.output_file_handle);
	return 0;
}

int s_fsm_normal2(struct serialport_fsm *self)
{
	char *s;
	int size;

	s = self->buffer + self->cursor;
	size = self->data_end - self->cursor;
	self->cursor = self->data_end;

	fwrite(s, 1, size, app.output_file_handle);
	fflush(app.output_file_handle);
	return 0;
}

static inline int s_fsm_buffer_empty(struct serialport_fsm *self)
{
	return self->cursor == self->data_end;
}

/* This function may call sp_blocking_read, so it can block. */
void s_fsm_fill_more(struct serialport_fsm *self)
{
	if (self->data_end > 0 && !s_fsm_buffer_empty(self))
		return;

	self->cursor = 0;

	/*
	 * `sp_blocking_read` return the number of bytes read on success,
	 * or a negative error code.
	 * If the result is zero, the timeout was reached before any bytes
	 * were available.
	 * If timeout_ms is zero, the function will always return either
	 * at least one byte, or a negative error code.
	 */
	self->data_end = sp_blocking_read(app.serialport, self->buffer,
			SERIALPORT_READ_BUFFER_SIZE, 100);

	if (self->data_end < 0)
		self->state = FSM_READ_ERROR;
	else
		self->buffer[self->data_end] = '\0';
}

int s_fsm_step(struct serialport_fsm *self)
{
	/*
	 * extract FSM_END checking out from the switch to avoid unnecessary
	 * blocking.
	 */
	if (self->state == FSM_END)
		return 1;

	s_fsm_fill_more(self);

	switch (self->state) {
	case FSM_WAITING_FOR_START:
		return s_fsm_wait_for_start(self);
	case FSM_NORMAL1:
		return s_fsm_normal1(self);
	case FSM_NORMAL2:
		return s_fsm_normal2(self);
	case FSM_READ_ERROR:
		fprintf(stderr, "serial port read error\n");
		return 2;
	default:
		return 3;
	}
}

static void *serialport_data_handler(void *data)
{
	struct serialport_fsm s_fsm;

	s_fsm_init(&s_fsm, app.start_string, app.end_string);

	/* `app_running_flag_get` have to be called after `s_fsm_step`. */
	while (!s_fsm_step(&s_fsm) &&
			(!s_fsm_buffer_empty(&s_fsm) ||
			app_running_flag_get(&app)))
		;

	if (app.debug)
		fprintf(stderr, "serial port data read thread finished\n");

	if (s_fsm.state == FSM_READ_ERROR)
		exit_info(11, "failed reading from serial port\n");

	app_running_flag_set(&app, 0);
	return NULL;
}

static void *stdin_data_handler(void *data)
{
	char buffer[STDIN_READ_BUFFER_SIZE + 1];
	int has_more;
	int r;

	/*
	 * If the stdin is not used, this thread should not affect serial port
	 * thread. So do NOT call `app_running_flag_set(&app, 0)` here.
	 */
	if (!app.pipe_stdin)
		return NULL;

	/* New threads defaults to be PTHREAD_CANCEL_ENABLE. */
	/*
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	*/

	has_more = 1;
	while (has_more && app_running_flag_get(&app)) {
		r = fread(buffer, 1, STDIN_READ_BUFFER_SIZE, stdin);
		if (r < STDIN_READ_BUFFER_SIZE) {
			has_more = 0;
			if (!feof(stdin) && ferror(stdin))
				exit_info(3, "read error: %d\n", errno);
		}
		r = sp_blocking_write(app.serialport, buffer, r, 0);
		if (r < 0)
			exit_info(2, "write error\n");
	}

	if (app.debug)
		fprintf(stderr, "stdin data handler thread finished\n");

	app_running_flag_set(&app, 0);
	return NULL;
}

int main(int argc, const char **argv)
{
	pthread_t stdin_thread, serialport_thread;
	void *thread_ret;
	int r;

	parse_arguments(argc, argv, &app);

	app_init(&app);
	if (app.debug)
		app_describe(&app);

	r = pthread_create(&serialport_thread, NULL, serialport_data_handler,
			NULL);
	if (r)
		exit_info(2, "failed creating serialport thread\n");

	r = pthread_create(&stdin_thread, NULL, stdin_data_handler, NULL);
	if (r)
		exit_info(2, "failed creating stdin thread\n");

	pthread_join(serialport_thread, &thread_ret);
	if (thread_ret == 0)
		pthread_cancel(stdin_thread);
	else
		pthread_join(stdin_thread, NULL);

	app_deinit(&app);

	return 0;
}
