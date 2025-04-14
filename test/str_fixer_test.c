#include "str_fixer.h"
#include <string.h>
#include <stddef.h>
#include <assert.h>

int main(void)
{
	struct str_fixer sf;

	assert(sf_init(&sf, NULL) == 1);
	assert(sf_init(&sf, "") == 1);
	assert(sf_init(&sf, "a") == 0);

	assert(sf_convert(&sf) == 0);
	assert(strcmp(sf.output, "a") == 0);

	sf_deinit(&sf);

	assert(sf_init(&sf, "a\\x62c") == 0);
	assert(sf_convert(&sf) == 0);
	assert(strcmp(sf.output, "abc") == 0);

	sf_deinit(&sf);

	return 0;
}
