#ifndef __STR_DETECTOR_H
#define __STR_DETECTOR_H

#include "range.h"

struct str_detector {
	const char *target;
	int target_size;
	int cursor;
};

int sd_init(struct str_detector *self, const char *target);
struct range sd_feed(struct str_detector *self, const char *buffer, int size);

#endif
