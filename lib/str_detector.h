#ifndef __STR_DETECTOR_H
#define __STR_DETECTOR_H

struct str_detector {
	const char *target;
	int target_size;
	int cursor;
};

struct feed_result {
	int start;
	int end;
};

void sd_initialize(struct str_detector *self, const char *target);
struct feed_result sd_feed(struct str_detector *self, const char *buffer, int size);

#endif
