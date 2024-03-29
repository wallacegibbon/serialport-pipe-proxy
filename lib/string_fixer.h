#ifndef __STRING_FIXER_H
#define __STRING_FIXER_H

struct string_fixer {
	const char *input;
	char *output;
	const char *input_cursor;
	char *output_cursor;
};

int sf_initialize(struct string_fixer *self, const char *input);
int sf_cleanup(struct string_fixer *self);
int sf_convert(struct string_fixer *self);

#endif
