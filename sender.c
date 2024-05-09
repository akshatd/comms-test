#include <arpa/inet.h> // for socket stuff
#include <math.h>      // for modf
#include <signal.h>    // for catching signals like keyboard interrupts
#include <stdio.h>     // for printf
#include <stdlib.h>    // for abs
#include <sys/time.h>  // for timeouts
#include <unistd.h>    // for sleep

#include "common.h"

static volatile int running = 1;

void sigint_handler(int sig) {
	printf("Caught signal %d\n", sig);
	running = 0;
}

int main() {
	// register signal handlers
	signal(SIGINT, sigint_handler);
	signal(SIGTERM, sigint_handler);
	signal(SIGKILL, sigint_handler);

	printf("Im the sender!\n");

	Config config = readConfig(CONFIG_FILE);
	printf("Server IP: %s\nPort: %hu\n", config.ip_server, config.port_sender);

	// create timeout
	struct timeval tv;
	double         to_sec, to_usec;
	to_usec    = modf(TIMEOUT_S, &to_sec) * 1000000;
	tv.tv_sec  = to_sec;
	tv.tv_usec = to_usec;

	// create UDP socket to send data
	int sockfd_sender = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd_sender < 0) {
		perror("Failed to create sender socket");
		return 1;
	} else {
		if (setsockopt(sockfd_sender, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
			perror("Failed to set timeout on sender socket");
			return 1;
		}
		printf("Sender socket created\n");
	}

	// create server address
	struct sockaddr_in addr_server;
	addr_server.sin_family      = AF_INET;
	addr_server.sin_addr.s_addr = inet_addr(config.ip_server);
	addr_server.sin_port        = htons(config.port_sender);

	// no need to bind socket when sending data to server

	// send data to server
	Packet    sent_packet;
	ssize_t   sent_packet_len;
	socklen_t addrlen = sizeof(addr_server);
	uint32_t  seq_num = 0;
	while (running) {
		sent_packet.seq_num  = seq_num++;
		sent_packet.distance = 100;
		sent_packet.crc      = crc(sent_packet.seq_num, sent_packet.distance);
		gettimeofday(&sent_packet.timestamp, NULL);
		sent_packet_len =
			sendto(sockfd_sender, &sent_packet, sizeof(sent_packet), 0, (struct sockaddr *)&addr_server, addrlen);
		if (sent_packet_len < 0) {
			perror("Failed to send packet, ignoring ...");
		} else {
#ifdef LOG_DEBUG
			printf(
				"Sent packet: %u %u %u (%ld bytes)\n", sent_packet.seq_num, sent_packet.distance, sent_packet.crc,
				sent_packet_len);
			printf("Sent packet to: %s:%hu\n", inet_ntoa(addr_server.sin_addr), ntohs(addr_server.sin_port));
#endif
		}
		usleep(SEND_INT_S * 1000000);
	}

	printf("Sender exiting\n");
	close(sockfd_sender);

	return 0;
}