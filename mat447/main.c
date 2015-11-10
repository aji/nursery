/* main.c -- the main testing framework */
/* Copyright (C) 2012 Alex Iadicicco */
/* Please see the README for licensing terms */

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <pari/pari.h>
#include "test.h"

static FILE *outfile;
static int maxtime;
static int maxtimeouts;

static jmp_buf jmpenv;
static struct timespec ptimer;

/*
   These were generated with gp2c from the following source:
   rand_digits(n) = random(9*10^(n-1))+10^(n-1)
   rand_prime(n) = nextprime(rand_digits(n))
   rand_composite(n) = rand_prime(floor(n/2.0))*rand_prime(ceil(n/2.0))
 */

/* Generates n random digits, i.e. rand_digits(6) => 561377. This is
   not exact, but is usually close enough. */
GEN rand_digits(GEN n, long prec)
{
	return gadd(genrand(gmulsg(9, gpow(stoi(10), gsubgs(n, 1), prec))),
		    gpow(stoi(10), gsubgs(n, 1), prec));
}

/* Generates a random n-digit prime number. This is not exact, but is
   usually close enough. */
GEN rand_prime(GEN n, long prec)
{
	return gnextprime(rand_digits(n, prec));
}

/* Generates a random n-digit number of the form N=pq with p and q prime */
GEN rand_composite(GEN n, long prec)
{
	pari_sp ltop = avma;
	GEN p, q;

	p = rand_prime(gfloor(gdiv(n, stor(2, prec))), prec);
	do {
		q = rand_prime(gceil(gdiv(n, stor(2, prec))), prec);
	} while (!gcmp(p, q));

	return gerepile(ltop, avma, gmul(p, q));
}

/* Outputs the CSV headers */
void out_headers(void)
{
	fprintf(outfile, "n,log10n,t\n");
}

/* Outputs a CSV row */
void out_row(GEN n, GEN logn, double t)
{
	pari_fprintf(outfile, "%Ps,%Ps,%lf\n", n, logn, t);
}

/* Starts the timer */
void ptimer_start(void)
{
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ptimer);
}

/* Stops the timer, returns the time */
double ptimer_stop(void)
{
	struct timespec now;
	double t;

	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now);

	t = (now.tv_nsec - ptimer.tv_nsec) / 1000000000.0;
	t += now.tv_sec - ptimer.tv_sec;

	return t;
}

/* Runs a single test for a given number of digits */
int run_one_test(long digits, long prec)
{
	pari_sp ltop = avma;
	GEN n;
	int ret;
	double t;

	n = gerepileupto(ltop, rand_composite(stoi(digits), prec));
	pari_printf("Factoring %Ps...\n", n);

	ptimer_start();
	ret = do_factor(n, prec);
	t = ptimer_stop();

	out_row(n, gdiv(glog(n, prec), glog(stoi(10), prec)), t);

	return ret;
}

/* Handles SIGALRM by making a longjmp */
void sigalrm_handler(int sig)
{
	sigset_t ss;
	printf("in signal handler\n");
	sigemptyset(&ss);
	sigaddset(&ss, SIGALRM);
	sigprocmask(SIG_UNBLOCK, &ss, NULL);
	longjmp(jmpenv, 0);
}

/* Runs all tests */
void run_tests(void)
{
	pari_sp ltop;
	long digits, i, ret;
	long timeouts = 0;
	void (*old_handler)(int);
	GEN n;

	old_handler = signal(SIGALRM, sigalrm_handler);

	printf("Testing %s\n", test_name);

	for (digits=3; ; digits++) {
		for (i=0; i<20; i++) {
			if (setjmp(jmpenv) != 0) {
				timeouts++;
				printf("test timed out (%d)\n", timeouts);
				if (timeouts >= maxtimeouts) {
					alarm(0);
					printf("maxtimeouts reached, terminating\n");
					signal(SIGALRM, old_handler);
					return;
				}
				continue;
			}

			alarm(maxtime);

			ltop = avma;
			ret = run_one_test(digits, DEFAULTPREC);
			avma = ltop;

			if (ret < 0)
				goto out;
		}
	}

out:
	alarm(0);
	printf("Factorizer failed\n");
}

/* Opens the output file */
int outfile_open(const char *fname)
{
	if (outfile != NULL) {
		printf("outfile is already open!\n");
		return -1;
	}

	outfile = fopen(fname, "w");

	if (outfile == NULL) {
		perror("Error opening outfile");
		return -1;
	}

	out_headers();

	return 0;
}

/* Close the output file */
int outfile_close()
{
	if (outfile == NULL) {
		printf("outfile is not open!\n");
		return -1;
	}

	fclose(outfile);

	outfile = NULL;

	return 0;
}

/* Prints a usage message */
void usage(const char *argv0)
{
	printf("usage: %s OUTFILE MAXTIME MAXTIMEOUTS\n", argv0);
	printf("\n");
	printf("OUTFILE is the file in which to dump the test data. The data\n");
	printf("is in CSV form. The columns are N, and running time in seconds.\n");
	printf("\n");
	printf("MAXTIME is the maximum time in seconds to let a test run. If a test\n");
	printf("takes longer than this amount of time to run, a counter is increased,\n");
	printf("and a new test is run.\n");
	printf("\n");
	printf("MAXTIMEOUTS is the maximum number of timeouts to allow before shutting\n");
	printf("down a test. This means that the uninterrupted running time of the\n");
	printf("program will be MAXTIME*MAXTIMEOUTS, so be careful when choosing these.\n");
	printf("\n");
	printf("Random numbers of the form N=p*q with p and q prime are generated\n");
	printf("and then factored. The testing software attempts to generate twenty\n");
	printf("values of N for each digit length, starting from 3. In other words,\n");
	printf("Twenty 3 digits numbers are factored, then twenty four digit numbers,\n");
	printf("then twenty five digit numbers, etc. until the factoring algorithm\n");
	printf("exceeds the given maximum time\n");
}

int main(int argc, char *argv[])
{
	if (argc != 4) {
		usage(argv[0]);
		return 1;
	}

	if (outfile_open(argv[1]) < 0)
		return 1;

	maxtime = atoi(argv[2]);
	maxtimeouts = atoi(argv[3]);

	pari_init(1<<19, 1<<16);

	setrand(stoi(time(NULL)));

	run_tests();

	outfile_close();

	return 0;
}
