#include <stdint.h>
#include <endian.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "rom.h"

uint16_t rom_data[ROM_N_WORDS];
uint16_t rom_info[ROM_N_WORDS];
size_t rom_words;

static void warn(const char *f, ...)
{
	char buf[65536];
	va_list va;

	va_start(va, f);
	vsnprintf(buf, 65536, f, va);
	va_end(va);

	printf("Warning: %s\n", buf);
}

void rom_reset(void)
{
	uint32_t i;

	for (i=0; i<ROM_N_WORDS; i++) {
		rom_data[i] = 0;
		rom_info[i] = 0;
	}
}

static int load_bin(FILE *f)
{
	uint16_t c;

	rom_words = 0;
	while (!feof(f) && rom_words < ROM_N_WORDS) {
		fread(&c, 1, 2, f);
		rom_data[rom_words++] = be16toh(c);
	}

	if (!feof(f))
		warn("Binary image input too large");

	return 0;
}

int rom_load(const char *file)
{
	int x, n;
	enum { UNK, BIN, SMD } ftype = UNK;
	FILE *f;

	if (!(f = fopen(file, "r"))) {
		perror("fopen");
		return -1;
	}

	n = strlen(file) - 3;
	if (!strcasecmp(file + n, "bin")) ftype = BIN; else
	if (!strcasecmp(file + n, "smd")) ftype = SMD; else
	                                  ftype = UNK;

	switch (ftype) {
	case SMD:
		warn("Cannot yet load SMD files");
	case UNK:
		warn("Assuming input is BIN");
	case BIN:
		x = load_bin(f);
		break;
	}

	fclose(f);

	return x;
}
