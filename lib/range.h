#ifndef __RANGE_H
#define __RANGE_H

struct range {
	int start;
	int end;
};

static inline void range_set(struct range *self, int start, int end) {
	self->start = start;
	self->end = end;
}

#endif
