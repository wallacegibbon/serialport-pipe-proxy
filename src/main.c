#include "cmd_argument_parser.h"
#include "str_matcher.h"
#include "str_fixer.h"
#include "util.h"
#include <libserialport.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>

#define SERIALPORT_BUF_SIZE 4096
#define STDIN_BUF_SIZE 4096

/* Parameters */

const char *serialport_device;		/* Device file name of port */
long baudrate;				/* Baudrate of the port */
const char *start_string;		/* The start sign of data */
const char *end_string;			/* The end sign of data */
const char *output_file;		/* File to write to */
long long timeout = -1;			/* Timeout in milliseconds */
long long timecount;			/* To match with `timeout` */
char pipe_stdin;			/* STDIN -> Serial Port */
char debug;				/* Control debug printing */

/* States */

struct sp_port *serialport;		/* Main object of `libserialport` */
int is_running = 1;			/* Thread control */
pthread_mutex_t is_running_lock;	/* Work with `is_running` */
FILE *output_stream;			/* Can be STDOUT */

static const char *adjust_escaped_string(const char *s)
{
	struct str_fixer fixer;
	int i;

	sf_init(&fixer, s);
	if (sf_convert(&fixer) == 0)
		return (const char *)fixer.output;
	else
		return NULL;
}

void app_usage_and_exit()
{
	exit_info(1, "Usage: sp-pipe\t--port\t\t/dev/ttyUSB0\n"
			"\t\t--baudrate\t115200\n"
			"\t\t--start-string\t'a\\x62c'\n"
			"\t\t--end-string\t'd\\x65f'\n"
			"\t\t--timeout\t10000\n"
			"\t\t--pipe-stdin\n"
			"\t\t--debug\n");
}

void app_get_arguments(int argc, const char **argv)
{
	struct cmd_argument_parser parser;

	cmd_argument_parser_init(&parser, argc - 1, argv + 1);
	/*
	cmd_argument_parser_describe(&parser);
	*/

	serialport_device = cmd_argument_parser_get(&parser, "port", NULL);
	baudrate = atoi(cmd_argument_parser_get(&parser, "baudrate", "115200"));
	start_string = cmd_argument_parser_get(&parser, "start-string", NULL);
	end_string = cmd_argument_parser_get(&parser, "end-string", NULL);
	output_file = cmd_argument_parser_get(&parser, "output-file", NULL);
	timeout = atoi(cmd_argument_parser_get(&parser, "timeout", "-1"));
	pipe_stdin = cmd_argument_parser_has(&parser, "pipe-stdin");
	debug = cmd_argument_parser_has(&parser, "debug");

	cmd_argument_parser_deinit(&parser);

	if (serialport_device == NULL)
		exit_info(2, "Serial port is not specified\n");
}

static void serialport_open()
{
	if (sp_get_port_by_name(serialport_device, &serialport) < 0)
		exit_info(-1, "failed getting port by name \"%s\"\n",
				serialport_device);
	if (sp_open(serialport, SP_MODE_READ_WRITE) < 0)
		exit_info(-2, "failed opening port \"%s\"\n",
				serialport_device);

	sp_set_baudrate(serialport, baudrate);
	sp_set_bits(serialport, 8);
	sp_set_parity(serialport, SP_PARITY_NONE);
	sp_set_stopbits(serialport, 1);
	sp_set_flowcontrol(serialport, SP_FLOWCONTROL_NONE);
}

void app_init()
{
	pthread_mutex_init(&is_running_lock, NULL);

	/*
	 * Open the serial port before starting serial port reading thread
	 * to avoid some error.
	 */
	serialport_open();

	if (output_file == NULL)
		output_stream = stdout;
	else
		output_stream = fopen(output_file, "w");

	if (output_stream == NULL)
		exit_info(3, "failed opening output file: %d\n",
				output_file);

	if (start_string)
		start_string = adjust_escaped_string(start_string);
	if (end_string)
		end_string = adjust_escaped_string(end_string);
}

void app_deinit()
{
	pthread_mutex_destroy(&is_running_lock);

	if (sp_close(serialport) < 0)
		exit_info(-2, "failed closing port %s\n", serialport_device);
	if (fclose(output_stream))
		exit_info(-2, "failed closing output file %s\n", output_file);

	/*
	 * start_string and end_string points to memories `malloc`ed
	 * by `str_fixer`.
	 */

	free((void *)start_string);
	free((void *)end_string);
}

void app_describe()
{
	fprintf(stderr, "application params\n\tport:%s, baudrate:%ld, "
			"start_string:%s, end_string:%s, "
			"timeout: %lld, timecount: %lld, "
			"pipe_stdin: %d, debug: %d\n",
			serialport_device, baudrate,
			start_string, end_string,
			timeout, timecount, pipe_stdin, debug);
}

void app_debug(const char *fmt, ...)
{
	va_list args;

	if (!debug)
		return;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);

	va_end(args);
}

void app_set_running(int v)
{
	pthread_mutex_lock(&is_running_lock);
	app_debug("setting running flag to %d\n", v);
	is_running = v;
	pthread_mutex_unlock(&is_running_lock);
}

int app_get_running()
{
	int is_active;
	pthread_mutex_lock(&is_running_lock);
	is_active = is_running;
	pthread_mutex_unlock(&is_running_lock);
	return is_active;
}

enum serialport_fsm_state {
	FSM_WAITING_FOR_START,
	FSM_NORMAL1,
	FSM_NORMAL2,
	FSM_END,
	FSM_READ_ERROR
};

/* FSM for data analyzing.  (start_string, end_string related) */

enum serialport_fsm_state s_fsm;	/* The state machine handle */
char s_buf[SERIALPORT_BUF_SIZE + 1];	/* Buffer for serial port data */
int s_buf_cursor;			/* Index, work with s_buf */
int s_buf_end;				/* The index to the end of data */
struct str_matcher s_matcher;		/* Matcher holds some inner data */

void s_fsm_init(const char *start_string, const char *end_string)
{
	if (start_string != NULL) {
		s_fsm = FSM_WAITING_FOR_START;
		sm_init(&s_matcher, start_string);
	} else if (end_string != NULL) {
		s_fsm = FSM_NORMAL1;
		sm_init(&s_matcher, end_string);
	} else {
		s_fsm = FSM_NORMAL2;
	}
}

int s_fsm_wait_for_start()
{
	int start = 0, end = 0, size;
	char *s;

	s = s_buf + s_buf_cursor;
	size = s_buf_end - s_buf_cursor;
	if (sm_feed(&s_matcher, s, size, &start, &end)) {
		s_buf_cursor = s_buf_end;
		return 0;
	}

	s_buf_cursor += end;

	fwrite(s + start, 1, end - start, output_stream);
	fflush(output_stream);

	if (end_string != NULL) {
		s_fsm = FSM_NORMAL1;
		sm_init(&s_matcher, end_string);
	} else {
		s_fsm = FSM_NORMAL2;
	}
	return 0;
}

int s_fsm_normal1()
{
	int start = 0, end = 0, size;
	char *s;

	s = s_buf + s_buf_cursor;
	size = s_buf_end - s_buf_cursor;

	if (sm_feed(&s_matcher, s, size, &start, &end)) {
		s_buf_cursor = s_buf_end;
	} else {
		s_fsm = FSM_END;
		s_buf_cursor += end;
		size = end;
	}

	fwrite(s, 1, size, output_stream);
	fflush(output_stream);
	return 0;
}

int s_fsm_normal2()
{
	char *s;
	int size;

	s = s_buf + s_buf_cursor;
	size = s_buf_end - s_buf_cursor;
	s_buf_cursor = s_buf_end;

	fwrite(s, 1, size, output_stream);
	fflush(output_stream);
	return 0;
}

static inline int s_fsm_buf_empty()
{
	return s_buf_cursor == s_buf_end;
}

/* This function may call sp_blocking_read, so it can block. */
void s_fsm_fill_more()
{
	if (s_buf_end > 0 && !s_fsm_buf_empty())
		return;

	s_buf_cursor = 0;

	/*
	 * `sp_blocking_read` return the number of bytes read on success,
	 * or a negative error code.
	 * If the result is zero, the timeout was reached before any bytes
	 * were available.
	 * If timeout_ms is zero, the function will always return either
	 * at least one byte, or a negative error code.
	 */
	s_buf_end = sp_blocking_read(serialport, s_buf,
			SERIALPORT_BUF_SIZE, 100);

	if (s_buf_end < 0)
		s_fsm = FSM_READ_ERROR;
	else
		s_buf[s_buf_end] = '\0';
}

int s_fsm_step()
{
	/*
	 * extract FSM_END checking out from the switch to avoid unnecessary
	 * blocking.
	 */
	if (s_fsm == FSM_END)
		return 1;

	s_fsm_fill_more();

	switch (s_fsm) {
	case FSM_WAITING_FOR_START:
		return s_fsm_wait_for_start();
	case FSM_NORMAL1:
		return s_fsm_normal1();
	case FSM_NORMAL2:
		return s_fsm_normal2();
	case FSM_READ_ERROR:
		fprintf(stderr, "serial port read error\n");
		return 2;
	default:
		return 3;
	}
}

static void *serialport_data_handler(void *data)
{
	s_fsm_init(start_string, end_string);

	/* `app_get_running` have to be called after `s_fsm_step`. */
	while (!s_fsm_step() && (!s_fsm_buf_empty() ||
					app_get_running()))
		;

	app_debug("serial port thread finished\n");

	if (s_fsm == FSM_READ_ERROR)
		exit_info(11, "failed reading from serial port\n");

	app_set_running(0);
	return NULL;
}

static void *stdin_data_handler(void *data)
{
	char buffer[STDIN_BUF_SIZE + 1];
	int has_more = 1, r;

	/*
	 * If the stdin is not used, this thread should not affect serial port
	 * thread. So do NOT call `app_set_running(0)` here.
	 */
	if (!pipe_stdin)
		exit_info(3, "invalid option %lld for stdin piping\n");

	/* New threads defaults to be PTHREAD_CANCEL_ENABLE. */
	/*
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	*/

	while (has_more && app_get_running()) {
		r = fread(buffer, 1, STDIN_BUF_SIZE, stdin);
		if (r < STDIN_BUF_SIZE) {
			has_more = 0;
			if (!feof(stdin) && ferror(stdin))
				exit_info(3, "read error: %d\n", errno);
		}
		r = sp_blocking_write(serialport, buffer, r, 0);
		if (r < 0)
			exit_info(2, "write error\n");
	}

	app_debug("stdin thread finished\n");

	app_set_running(0);
	return NULL;
}

static void *timeout_handler(void *data)
{
	if (timeout < 0)
		exit_info(3, "invalid timeout value %lld\n", timeout);

	while (timecount < timeout && app_get_running()) {
		sleep_ms(100);
		timecount += 100;
	}

	app_debug("timeout thread finished\n");

	app_set_running(0);
	return NULL;
}

int main(int argc, const char **argv)
{
	pthread_t serialport_thread, stdin_thread, timeout_thread;

	if (argc == 1)
		app_usage_and_exit();

	app_get_arguments(argc, argv);
	app_init();

	if (debug)
		app_describe();

	if (pthread_create(&serialport_thread, NULL, serialport_data_handler,
			NULL))
		exit_info(2, "failed creating serialport thread\n");

	if (pipe_stdin) {
		if (pthread_create(&stdin_thread, NULL, stdin_data_handler,
				NULL))
			exit_info(2, "failed creating stdin thread\n");
	}

	if (timeout > 0) {
		if (pthread_create(&timeout_thread, NULL, timeout_handler,
				NULL))
			exit_info(2, "failed creating timeout thread\n");
	}

	if (pthread_join(serialport_thread, NULL))
		exit_info(2, "failed joining serial port thread");

	if (timeout > 0) {
		if (pthread_join(timeout_thread, NULL))
			exit_info(2, "failed joining timeout thread");
	}

	/* stdin is working in block mode, thus need to be canceled */
	if (pipe_stdin) {
		if (pthread_cancel(stdin_thread))
			exit_info(2, "failed canceling stdin thread");
	}

	app_deinit();

	return 0;
}
