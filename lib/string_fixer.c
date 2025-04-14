#include "string_fixer.h"
#include <stdlib.h>
#include <string.h>

int sf_init(struct string_fixer *self, const char *input)
{
	self->input = input;
	self->output = calloc(1, strlen(input) + 1);
	if (self->output == NULL)
		return 1;

	self->input_cursor = input;
	self->output_cursor = self->output;

	return 0;
}

/*
 * This function may not be called since the output is usually managed
 * by other data.
 */
int sf_deinit(struct string_fixer *self)
{
	free(self->output);
	return 0;
}

int sf_step(struct string_fixer *self, int *error)
{
	char tmpbuf[3];
	char ch;

	*error = 0;
	ch = *self->input_cursor;
	if (ch == '\0')
		return 0;
	if (ch != '\\') {
		*self->output_cursor++ = *self->input_cursor++;
		return 1;
	}

	if (*(self->input_cursor + 1) != 'x') {
		*error = 1;
		return 0;
	}

	tmpbuf[0] = *(self->input_cursor + 2);
	if (tmpbuf[0] == '\0') {
		*error = 2;
		return 0;
	}

	tmpbuf[1] = *(self->input_cursor + 3);
	if (tmpbuf[1] == '\0') {
		*error = 3;
		return 0;
	}

	tmpbuf[2] = '\0';
	*self->output_cursor++ = strtol(tmpbuf, NULL, 16);
	self->input_cursor += 4;
	return 1;
}

int sf_convert(struct string_fixer *self)
{
	int error;
	while (sf_step(self, &error)) ;

	/* not necessary since self->output is allocated by `calloc`. */
	*self->output_cursor = '\0';

	return error;
}
