#include <arpa/inet.h> // for socket stuff
#include <signal.h>    // for catching signals like keyboard interrupts
#include <stdio.h>     // for printf
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

	printf("Im the receiver!\n");

	Config config = readConfig(CONFIG_FILE);
	printf("Server: %s\nPort: %hu\n", config.ip_server, config.port_sender);

	// create timeout
	struct timeval tv;
	tv.tv_sec  = TIMEOUT_S;
	tv.tv_usec = TIMEOUT_S * 1000000;

	// create UDP socket to receive data
	int sockfd_receiver = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd_receiver < 0) {
		perror("Failed to create receiver socket");
		return 1;
	} else {
		if (setsockopt(sockfd_receiver, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
			perror("Failed to set timeout on receiver socket");
			return 1;
		}
		printf("Receiver socket created\n");
	}

	// create server address
	struct sockaddr_in server_addr;
	server_addr.sin_family      = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(config.ip_server);
	server_addr.sin_port        = htons(config.port_receiver);

	// bind socket so server can send data to it
	if (bind(sockfd_receiver, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		perror("Failed to bind receiver socket");
		return 1;
	} else {
		printf("Receiver socket bound\n");
	}

	// receive data from server
	Packet             received_packet;
	ssize_t            received_packet_len;
	socklen_t          addrlen = sizeof(server_addr);
	struct sockaddr_in server_addr_actual;
	uint32_t           seq_num  = 0;
	int64_t            seq_diff = 0;
	struct timeval     curr_time;
	double             delay;
	while (running) {
		// receive data from server
		received_packet_len = recvfrom(
			sockfd_receiver, &received_packet, sizeof(received_packet), 0, (struct sockaddr *)&server_addr_actual, &addrlen);
		if (received_packet_len < 0) {
			perror("Failed to receive packet, ignoring ...");
			continue;
		} else {
#ifdef LOG_DEBUG
			printf(
				"Received packet: %u %u %u (%ld bytes)\n", received_packet.seq_num, received_packet.distance,
				received_packet.crc, received_packet_len);
			printf(
				"Received packet from: %s:%hu\n", inet_ntoa(server_addr_actual.sin_addr), ntohs(server_addr_actual.sin_port));
#endif
			if (!isPacketValid(received_packet)) {
				printf("Packet is invalid\n");
			}
		}

		// calculate the time taken to receive the packet
		gettimeofday(&curr_time, NULL);
		delay = (curr_time.tv_sec - received_packet.timestamp.tv_sec) * 1000 +
		        (curr_time.tv_usec - received_packet.timestamp.tv_usec) / 1000.0;
		if (delay > MAX_DELAY_MS) printf("High Delay: sender -> receiver: %f\n", delay);

		// check if sequence has been skipped
		seq_diff = received_packet.seq_num - seq_num;
		if (seq_diff % UINT32_MAX != 1) {
			printf("Sequence number skipped: %u -> %u\n", seq_num, received_packet.seq_num);
		}
		seq_num = received_packet.seq_num;
	}

	printf("Receiver exiting\n");
	close(sockfd_receiver);

	return 0;
}