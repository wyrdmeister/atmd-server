#include <iostream>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

using namespace std;

#define PORT 5000
#define BUFLEN 512

void sig_handler(int sig) {
	cout << "Terminating ATMD bunch number tester." << endl;
	exit(0);
}

int main() {
	cout << "atmd_bntest: ATMD bunch number tester." << endl;

	signal(SIGINT, sig_handler);

	int sock;
	if ( (sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		cout << "Error creating UDP socket. (" << strerror(errno) << ")" << endl;
		return -1;
	}

	// Bind socket to address
	struct sockaddr_in addr_local, addr_remote;
	memset((char *) &addr_local, 0, sizeof(struct sockaddr_in));
	memset((char *) &addr_remote, 0, sizeof(struct sockaddr_in));

	addr_local.sin_family = AF_INET;
	addr_local.sin_port = htons(PORT);
	addr_local.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(sock, (struct sockaddr*)&addr_local, sizeof(struct sockaddr_in))) {
		cout << "Error binding UDP socket. (" << strerror(errno) << ")" << endl;
		close(sock);
		return -1;
	}

	char buffer[BUFLEN];
	memset(buffer, 0, BUFLEN);
	socklen_t addrlen = sizeof(struct sockaddr_in);

	uint32_t oldbn = 0;

	printf("Waiting for input on %s:%d\n", inet_ntoa(addr_local.sin_addr), PORT);

	struct timeval old, now;
	memset((void*)&old, 0, sizeof(struct timeval));
	memset((void*)&now, 0, sizeof(struct timeval));
	gettimeofday(&old, NULL);

	int i = 0;
	uint64_t intdt = 0;
	while(true) {
		if(recvfrom(sock, buffer, BUFLEN, 0, (struct sockaddr*)&addr_remote, &addrlen) == -1) {
			cout << "Error in recvfrom. (" << strerror(errno) << ")" << endl;
			close(sock);
			return -1;
		}

		uint32_t bn;
		uint64_t bnts;
		sscanf(buffer, "BN: %u:%lu", &bn, &bnts);

		gettimeofday(&now, NULL);
		uint64_t dt = (uint64_t)(now.tv_sec - old.tv_sec)*1000000 + (now.tv_usec - old.tv_usec);
		memcpy(&old, &now, sizeof(struct timeval));

		intdt += dt;

		i++;
		if(i == 10) {
			printf("BN: %u : %lu (time %.5f)\n", bn, bnts, intdt/1e6);
			i = 0;
			intdt = 0;
		}

		if(oldbn != 0) {
			if(oldbn != bn-1) {
				cout << "==> Missed packet from " << inet_ntoa(addr_remote.sin_addr) << ":" << ntohs(addr_remote.sin_port) << " - " << oldbn << " => " << bn << endl;
			}
		}
		oldbn = bn;
	}

	return 0;
}
