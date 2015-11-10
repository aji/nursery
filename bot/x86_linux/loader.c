#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

int main(int argc, char *argv[])
{
	struct addrinfo hints;
	struct addrinfo *res;
	int fd;

	if (argc < 4) {
		fprintf(stderr, "Usage: %s HOST PORT EXE [ARGS...]\n", argv[0]);
		fprintf(stderr, "Opens a TCP socket to the given host on fd 3\n");
		return 1;
	}

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) != 3) {
		fprintf(stderr, "socket() did not return 3!\n");
		return 1;
	}

	hints.ai_flags = AI_CANONNAME;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;

	if (getaddrinfo(argv[1], argv[2], &hints, &res) < 0) {
		perror("getaddrinfo()");
		return 1;
	}

	fprintf(stderr, "connecting to %s...\n", res->ai_canonname);

	if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
		perror("connect()");
		return 1;
	}

	freeaddrinfo(res);

	execvp(argv[3], argv + 3);
	perror("execvp()");
	return 1;
}
