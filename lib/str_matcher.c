#include "str_matcher.h"
#include <string.h>

int sm_init(struct str_matcher *self, const char *target)
{
	if (target == NULL || *target == '\0')
		return 1;

	self->target = target;
	self->target_size = strlen(target);
	self->cursor = 0;
	return 0;
}

/* Return 0 on match */
int sm_feed(struct str_matcher *self, const char *str, int size,
		int *start, int *end)
{
	int i = 0, j = 0;

	if (str == NULL || size == 0)
		return 1;
	if (start == NULL || end == NULL)
		return 2;

	while (self->cursor < self->target_size && i < size) {
		if (self->target[self->cursor++] != str[i++]) {
			self->cursor = 0;
			i = ++j;
		}
	}

	if (self->cursor < self->target_size)
		return SM_FEED_UNMATCH;

	*start = j;
	*end = i;
	return 0;
}
