#include <stdio.h>

extern int fib(int n);
extern int fib2(int n);

int main(int argc, char *argv[])
{
	int n;

	if (argc < 2) {
		fprintf(stderr, "usage: %s N\n", argv[0]);
		return 1;
	}

	n = atoi(argv[1]);
	printf("fib2(%d) = %d\n", n, fib2(n));
	printf("fib(%d) = %d\n", n, fib(n));
}
