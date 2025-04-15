#ifndef __STR_MATCHER_H
#define __STR_MATCHER_H

#define SM_FEED_UNMATCH		64

struct str_matcher {
	const char *target;
	int target_size;
	int cursor;
};

int sm_init(struct str_matcher *self, const char *target);

int sm_feed(struct str_matcher *self, const char *str, int size,
		int *start, int *end);

#endif
