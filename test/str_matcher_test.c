#include "str_matcher.h"
#include <stddef.h>
#include <assert.h>

int main(void)
{
	struct str_matcher sm;
	int start, end;

	/* `_init` that should fail */
	assert(sm_init(&sm, NULL) == 1);
	assert(sm_init(&sm, "") == 1);

	/* `_feed` that should fail */
	assert(sm_feed(&sm, NULL, 2, NULL, NULL) == 1);
	assert(sm_feed(&sm, "a", 0, NULL, NULL) == 1);

	assert(sm_feed(&sm, "a", 1, &start, NULL) == 2);
	assert(sm_feed(&sm, "a", 1, NULL, &end) == 2);

	/* single character pattern */
	assert(sm_init(&sm, "h") == 0);

	assert(sm_feed(&sm, "a", 1, &start, &end) == SM_FEED_UNMATCH);
	assert(sm_feed(&sm, "b", 1, &start, &end) == SM_FEED_UNMATCH);

	assert(sm_feed(&sm, "h", 1, &start, &end) == 0);
	assert(start == 0);
	assert(end == 1);

	/* multiple character pattern */
	assert(sm_init(&sm, "hello") == 0);

	assert(sm_feed(&sm, "abcdef", 6, &start, &end) == SM_FEED_UNMATCH);
	assert(sm_feed(&sm, "hel", 3, &start, &end) == SM_FEED_UNMATCH);
	assert(sm_feed(&sm, "llo", 3, &start, &end) == SM_FEED_UNMATCH);
	assert(sm_feed(&sm, "hel", 3, &start, &end) == SM_FEED_UNMATCH);
	assert(sm_feed(&sm, "looooo", 7, &start, &end) == 0);
	assert(start == 0);
	assert(end == 2);

	assert(sm_init(&sm, "a$!") == 0);
	assert(sm_feed(&sm, "abcdef", 6, &start, &end) == SM_FEED_UNMATCH);
	assert(sm_feed(&sm, "a", 1, &start, &end) == SM_FEED_UNMATCH);
	assert(sm_feed(&sm, "b", 1, &start, &end) == SM_FEED_UNMATCH);
	assert(sm_feed(&sm, "a", 1, &start, &end) == SM_FEED_UNMATCH);
	assert(sm_feed(&sm, "$", 1, &start, &end) == SM_FEED_UNMATCH);
	assert(sm_feed(&sm, "!", 1, &start, &end) == 0);
	assert(start == 0);
	assert(end == 1);

	return 0;
}
