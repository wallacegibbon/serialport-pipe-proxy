#ifndef __STR_FIXER_H
#define __STR_FIXER_H

struct str_fixer {
	const char *input;
	char *output;
	const char *input_cursor;
	char *output_cursor;
};

int sf_init(struct str_fixer *self, const char *input);
int sf_deinit(struct str_fixer *self);
int sf_convert(struct str_fixer *self);

#endif
