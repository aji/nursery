#include <stdio.h>
#include "markov.h"

static bool my_func(void *count_, const char *word) {
	int *count = count_;

	(*count)++;
	if (*count > 10)
		return false;

	printf("%s ", word);
	return true;
}

int main(int argc, char *argv[]) {
	mk_chain_t *mk;
	const char *words[4] = {"one", "two", "three", "one"};
	int count;

	mk = mk_chain_new(1, 0);
	mk_chain_associate(mk, words + 0);
	mk_chain_associate(mk, words + 1);
	mk_chain_associate(mk, words + 2);

	count = 0;
	mk_chain_generate(mk, my_func, &count);
	printf("\n");
}
