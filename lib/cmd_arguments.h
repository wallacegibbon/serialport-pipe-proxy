#ifndef __CMD_ARGUMENTS_H
#define __CMD_ARGUMENTS_H

struct cmd_arguments {
	int argc;
	const char **argv;
	int index;
	const char **keys;
	const char **values;
	int option_count;
};

void cmd_arguments_prepare(struct cmd_arguments *self, int argc, const char **argv);
void cmd_arguments_cleanup(struct cmd_arguments *self);

int cmd_arguments_parse_step(struct cmd_arguments *self);

const char *cmd_arguments_get(struct cmd_arguments *self, const char *key, const char *default_value);
int cmd_arguments_has(struct cmd_arguments *self, const char *key);

void cmd_arguments_describe(struct cmd_arguments *self);

#endif
