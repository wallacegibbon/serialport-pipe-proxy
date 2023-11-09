#include "str_detector.h"
#include <string.h>

void sd_initialize(struct str_detector *self, const char *target) {
	self->target = target;
	self->target_size = strlen(target);
	self->cursor = 0;
}

int sd_feed(struct str_detector *self, const char *buffer, int size) {
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
		return i;
	else
		return -1;
}

