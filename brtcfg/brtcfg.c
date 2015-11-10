#include <stdlib.h>
#include <stdio.h>
#include <math.h>

struct bclass {
	char *id;
	char *name;
	char *brtfile;
	char *maxfile;
};
struct bclass btrcfg_classes[] = {
	{ "lcd", "Display Backlight",
		"/sys/class/backlight/apple_backlight/brightness",
		"/sys/class/backlight/apple_backlight/max_brightness" },
	{ "gmux", "Display Backlight",
		"/sys/class/backlight/gmux_backlight/brightness",
		"/sys/class/backlight/gmux_backlight/max_brightness" },
	{ "key", "Keyboard Backlight",
		"/sys/class/leds/smc::kbd_backlight/brightness",
		"/sys/class/leds/smc::kbd_backlight/max_brightness" },
	{ NULL, NULL, NULL, NULL }
};
struct bclass *get_class(char *id)
{
	struct bclass *curr = btrcfg_classes;
	while (curr->id != NULL) {
		if (strcmp(curr->id, id) == 0)
			break;
		curr++;
	}
	if (curr->id == NULL)
		return NULL;
	return curr;
}

struct bstate {
	int current;
	int maximum;
};
int load_state(struct bstate *dest, struct bclass *class)
{
	FILE *f;
	char buffer[33];

	buffer[32] = '\0';

	f = fopen(class->brtfile, "r");
	if (f == NULL)
		return -1;
	fgets(buffer, 32, f);
	dest->current = atoi(buffer);
	fclose(f);

	f = fopen(class->maxfile, "r");
	if (f == NULL)
		return -1;
	fgets(buffer, 32, f);
	dest->maximum = atoi(buffer);
	fclose(f);

	return 0;
}
int save_state(struct bstate *src, struct bclass *class)
{
	FILE *f;

	f = fopen(class->brtfile, "w");
	if (f == NULL)
		return -1;
	fprintf(f, "%d", src->current);

	return 0;
}

void cmd_list_individual(struct bclass *class)
{
	struct bstate state;
	printf("%s '%s': %s\n", class->id, class->name, class->brtfile);
	if (load_state(&state, class) < 0) {
		printf ("\terror loading class state\n");
		return;
	}
	printf("\tBrightness: %d/%d\n", state.current, state.maximum);
}
int cmd_list(int argc, char *argv[])
{
	struct bclass *curr;
	
	curr = btrcfg_classes;
	if (argc > 1)
		curr = get_class(argv[1]);

	if (curr == NULL || curr->id == NULL) {
		printf("list: unknown class '%s'\n", argv[1]);
		return 1;
	}

	if (argc > 1) {
		cmd_list_individual(curr);
	} else {
		while (curr->id != NULL) {
			cmd_list_individual(curr);
			curr++;
		}
	}

	return 0;
}

int adjust(char *classid, int set, int delta, double fac)
{
	struct bclass *class;
	struct bstate state;
	int len;

	class = get_class(classid);
	if (class == NULL || class->id == NULL) {
		printf("adjust: unknown class '%s'\n", classid);
		return 1;
	}

	if (load_state(&state, class) < 0) {
		printf("adjust: error loading class state\n");
		return 1;
	}

	if (set >= 0)
		state.current = set;
	state.current *= fac;
	if (state.current == 0)
		state.current = 1;
	state.current += delta;

	if (state.current > state.maximum)
		state.current = state.maximum;
	if (state.current < 0)
		state.current = 0;

	if (save_state(&state, class) < 0) {
		printf("adjust: error saving class state\n");
		return 1;
	}

	len = (int)(log10((double)state.maximum) + 1);
	printf("%s: %*d/%d\n", classid, len, state.current, state.maximum);

	return 0;
}

int cmd_inc(int argc, char *argv[])
{
	int amount;
	if (argc < 3) {
		printf("inc: not enough arguments\n");
		return 1;
	}
	amount = atoi(argv[2]);
	return adjust(argv[1], -1, amount, 1);
}

int cmd_mul(int argc, char *argv[])
{
	double fac;
	if (argc < 3) {
		printf("mul: not enough arguments\n");
		return 1;
	}
	fac = atof(argv[2]);
	return adjust(argv[1], -1, 1, fac);
}

int cmd_dec(int argc, char *argv[])
{
	int amount;
	if (argc < 3) {
		printf("dec: not enough arguments\n");
		return 1;
	}
	amount = atoi(argv[2]);
	return adjust(argv[1], -1, -amount, 1);
}

int cmd_div(int argc, char *argv[])
{
	double fac;
	if (argc < 3) {
		printf("div: not enough arguments\n");
		return 1;
	}
	fac = atof(argv[2]);
	if (fac == 0) {
		printf("div: cannot divide by zero\n");
		return 1;
	}
	return adjust(argv[1], -1, 2, 1.0 / fac);
}

int cmd_set(int argc, char *argv[])
{
	int amount;
	if (argc < 3) {
		printf("set: not enough arguments\n");
		return 1;
	}
	amount = atoi(argv[2]);
	return adjust(argv[1], amount, 0, 1);
}

struct command {
	char *name;
	char *usage;
	char *help;
	int (*fn)(int argc, char *argv[]);
};
struct command brtcfg_commands[] = {
	{ "list", "[BRT]", "list current brightnesses",
		cmd_list },
	{ "inc", "BRT AMT", "increment brightness",
		cmd_inc },
	{ "mul", "BRT AMT", "multiply brightness",
		cmd_mul },
	{ "dec", "BRT AMT", "decrement brightness",
		cmd_dec },
	{ "div", "BRT AMT", "divide brightness",
		cmd_div },
	{ "set", "BRT AMT", "set brightness",
		cmd_set },
	{ NULL, NULL, NULL, NULL }
};

void usage_and_exit(char *progname)
{
	struct command *curr;
	
	// basic help message
	printf("usage: %s COMMAND [args]\n", progname);

	// print list of commands
	printf("commands:\n");
	printf("%5s %-10s   %s\n", "CMD", "OPTIONS", "DESCRIPTION");
	curr = brtcfg_commands;
	while (curr->name != NULL) {
		printf("%5s %-10s   %s\n", curr->name, curr->usage, curr->help);
		curr++;
	}

	exit(1);
}

int main(int argc, char *argv[])
{
	struct command *curr;

	if (argc < 2)
		usage_and_exit(argv[0]);

	curr = brtcfg_commands;
	while (curr->name != NULL) {
		if (strcmp(curr->name, argv[1]) == 0)
			break;
		curr++;
	}
	if (curr->name == NULL) {
		printf("error: couldn't find command '%s'\n", argv[1]);
		usage_and_exit(argv[0]);
	}

	return curr->fn(argc - 1, argv + 1);
}
