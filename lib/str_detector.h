#ifndef __STR_DETECTOR_H
#define __STR_DETECTOR_H

struct str_detector {
	const char *target;
	int target_size;
	int cursor;
};

void sd_initialize(struct str_detector *self, const char *target);
int sd_feed(struct str_detector *self, const char *buffer, int size);

#endif
