#include "str_fixer.h"
#include <stdlib.h>
#include <string.h>

int sf_init(struct str_fixer *self, const char *input)
{
	if (input == NULL || *input == '\0')
		return 1;

	self->input = input;
	self->output = malloc(strlen(input) + 1);
	if (self->output == NULL)
		return 2;

	self->input_cursor = input;
	self->output_cursor = self->output;

	return 0;
}

/*
 * This function may not be called since the output is usually managed
 * by other data.
 */
int sf_deinit(struct str_fixer *self)
{
	free(self->output);
	return 0;
}

/*
 * Return 0 when the iteration is not finished yet.
 * Return 1 on finish.
 * Return minus number on error.
 */
int sf_step(struct str_fixer *self)
{
	char tmpbuf[3];
	char ch;

	ch = *self->input_cursor;
	if (ch == '\0')
		return 1;
	if (ch != '\\') {
		*self->output_cursor++ = *self->input_cursor++;
		return 0;
	}

	if (*(self->input_cursor + 1) != 'x')
		return -1;

	tmpbuf[0] = *(self->input_cursor + 2);
	if (tmpbuf[0] == '\0')
		return -2;

	tmpbuf[1] = *(self->input_cursor + 3);
	if (tmpbuf[1] == '\0')
		return -3;

	tmpbuf[2] = '\0';
	*self->output_cursor++ = strtol(tmpbuf, NULL, 16);
	self->input_cursor += 4;
	return 0;
}

int sf_convert(struct str_fixer *self)
{
	int t;
	while ((t = sf_step(self)) == 0);

	/* not necessary since self->output is allocated by `calloc`. */
	*self->output_cursor = '\0';

	if (t == 1)
		return 0;
	else
		return t;
}
