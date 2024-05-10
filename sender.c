#include <arpa/inet.h> // for socket stuff
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

	Config config = readConfig(CONFIG_FILE);
	printf("***\nSending to: %s:%hu\n***\n", config.ip_server, config.port_sender);

	// set up socket to send data
	int sockfd_sender = setupUdpSocket(false, NULL, 0, TIMEOUT_S);
	if (sockfd_sender < 0) {
		perror("*** ERROR: Failed to create sender socket");
		return 1;
	}

	// create server address
	struct sockaddr_in addr_server;
	addr_server.sin_family      = AF_INET;
	addr_server.sin_addr.s_addr = inet_addr(config.ip_server);
	addr_server.sin_port        = htons(config.port_sender);

	// send data to server
	Packet    packet;
	ssize_t   packet_len;
	socklen_t addrlen = sizeof(addr_server);
	uint32_t  seq_num = 0;
	uint16_t  syncs   = 0;

	while (running) {
		usleep(SEND_INT_S * 1000000);

		// create packet
		if (syncs++ < NUM_SYNC) {
			packet.type     = Sync;
			packet.seq_num  = 0;
			packet.distance = 0;
		} else {
			packet.type     = Data;
			packet.seq_num  = seq_num++;
			packet.distance = 100;
		}
		packet.crc = crc(packet.seq_num, packet.distance);
		gettimeofday(&packet.timestamp, NULL);

		// send packet
		packet_len = sendto(sockfd_sender, &packet, sizeof(packet), 0, (struct sockaddr *)&addr_server, addrlen);
		if (packet_len < 0) {
			perror("*** ERROR: Failed to send packet, ignoring ...");
			continue;
		}

#ifdef LOG_DEBUG
		printf(
			"Sent: %u %u %u (%ld bytes) to %s:%hu\n", packet.seq_num, packet.distance, packet.crc, packet_len,
			inet_ntoa(addr_server.sin_addr), ntohs(addr_server.sin_port));
#endif
	}

	printf("Sender shutting down...\n");
	close(sockfd_sender);

	return 0;
}