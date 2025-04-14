#include "str_detector.h"
#include <string.h>

int sd_init(struct str_detector *self, const char *target)
{
	if (target == NULL || *target == '\0')
		return 1;

	self->target = target;
	self->target_size = strlen(target);
	self->cursor = 0;
	return 0;
}

struct range sd_feed(struct str_detector *self, const char *buffer, int size)
{
	struct range r;
	int i = 0, j = 0;

	while (i < size && self->cursor < self->target_size) {
		if (self->target[self->cursor] == buffer[i]) {
			self->cursor++;
			i++;
		} else {
			self->cursor = 0;
			i = ++j;
		}
	}

	if (self->cursor == self->target_size)
		range_set(&r, j, i);
	else
		range_set(&r, -1, -1);

	return r;
}
