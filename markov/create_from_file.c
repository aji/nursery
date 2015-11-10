/* create_from_input.c -- markov.c usage, from a file */
/* Copyright (C) 2015 Alex Iadicicco */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "markov.h"

#define WORD_BUF_SIZE 128
#define WORD_MAX 512

int read_words(char words[][WORD_MAX], int n) {
	int i, j, c;

	for (i=0; i<n; i++) {
		for (;;) {
			c = getc(stdin);
			if (c == EOF)
				return i;
			if (!isspace(c)) {
				ungetc(c, stdin);
				break;
			}
		}

		for (j=0; j<WORD_MAX-1; j++) {
			c = getc(stdin);
			if (c == EOF) {
				words[i][j] = '\0';
				return i + 1;
			}
			if (isspace(c)) {
				break;
			}

			words[i][j] = c;
		}

		words[i][j] = '\0';
	}

	return i;
}

static bool my_func(void *count_, const char *word) {
	int *count = count_;

	(*count)++;
	if (*count > 100)
		return false;

	printf("%s ", word);
	return true;
}

int main(int argc, char *argv[]) {
	int wpn, overlap;
	mk_chain_t *mk;
	char words[WORD_BUF_SIZE][WORD_MAX];
	const char *word_ptrs[WORD_BUF_SIZE];
	int i, n, at, got, total_words, last_print;

	if (argc < 3) {
		fprintf(stderr, "usage: %s (WORDS PER NODE) (OVERLAP)\n", argv[0]);
		return 1;
	}

	wpn = atoi(argv[1]);
	overlap = atoi(argv[2]);

	if (overlap >= wpn) {
		fprintf(stderr, "overlap cannot be >= words per node\n");
		return 2;
	}

	for (i=0; i<WORD_BUF_SIZE; i++) {
		word_ptrs[i] = words[i];
	}

	mk = mk_chain_new(wpn, overlap);
	n = 2 * wpn + overlap;
	at = 0;
	total_words = 0;
	last_print = 0;

	for (;;) {
		got = read_words(words + at, WORD_BUF_SIZE - at);
		total_words += got;
		at += got;

		for (i=0; i < at - n; i++) {
			mk_chain_associate(mk, &word_ptrs[i]);
		}

		if (total_words - last_print > 1000) {
			last_print = total_words;
			printf("\r%d total words, %d nodes, %d edges",
			       total_words, mk->n_len, mk->edge_count);
			fflush(stdout);
		}

		if (at < WORD_BUF_SIZE)
			break;

		memmove(words, words + at - n, WORD_MAX * n);
		at = n;
	}
	printf("\r%d total words, %d nodes, %d edges\ndone\n",
	       total_words, mk->n_len, mk->edge_count);

	srand(time(NULL));

	int count = 0;
	mk_chain_generate(mk, my_func, &count);
	printf("\n");
}
