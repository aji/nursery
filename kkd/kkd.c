#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include <stdio.h>

#define PACKET_SIZE 65536

struct icmp_echo_hdr {
	uint8_t type;
	uint8_t i_dont_care_about_these_bytes[7];
	uint8_t data[0];
};

struct packet {
	char data[PACKET_SIZE];
	ssize_t len;
	struct sockaddr addr;
	socklen_t addrlen;
};

static uint32_t knock_code = 0;

void find_knock(void *data, size_t len)
{
	struct icmp_echo_hdr *icmp = data;
	uint32_t test;
	
	/* ICMP type 8 is echo requst */
	if (icmp->type != 8)
		return;

	test = 0;
	memcpy(&test, icmp->data + 8, 4);
	test = ntohl(test);

	if (test == knock_code)
		printf("Knock knock! Who's there?\n");
}

void recv_loop(int fd)
{
	struct packet pk;
	size_t hdrlen;

	for (;;) {
		pk.addrlen = sizeof(pk.addr); /* i have no idea what i'm doing */
		pk.len = recvfrom(fd, pk.data, PACKET_SIZE, 0, &pk.addr, &pk.addrlen);

		if (pk.len == 0)
			break;

		hdrlen = (pk.data[0] & 0xf) << 2;
		find_knock(pk.data + hdrlen, pk.len - hdrlen);
	}
}

int welcome_mat(void)
{
	int fd;
	struct sockaddr_in addr;

	if ((fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
		fprintf(stderr, "socket() failed. Are you EUID 0?\n");
		return -1;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "bind() failed. Are you EUID 0?\n");
		return -1;
	}

	return fd;
}

int main(int argc, char *argv[])
{
	int fd;

	if (argc != 2) {
		fprintf(stderr, "usage: kkd CODE\n");
		return 1;
	}

	knock_code = strtoul(argv[1], NULL, 16);

	fd = welcome_mat();
	if (fd < 0)
		return 1;

	printf("Listening for knocks with 0x%08x\n", knock_code);
	recv_loop(fd);

	return 0;
}
