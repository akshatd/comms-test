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

	printf("Im the receiver!\n");

	Config config = readConfig(CONFIG_FILE);
	printf("IP: %s\nPort: %hu\n", config.ip_receiver, config.port_receiver);

	// setup socket to receive data
	int sockfd_receiver = setupUdpSocket(true, config.ip_receiver, config.port_receiver, TIMEOUT_S);
	if (sockfd_receiver < 0) {
		perror("Failed to create receiver socket");
		return 1;
	}

	// receive data from server
	Packet             packet;
	ssize_t            packet_len;
	struct sockaddr_in addr_server;
	socklen_t          addr_server_len = sizeof(addr_server);
	uint32_t           seq_num         = 0;
	int64_t            seq_diff        = 0;
	struct timeval     curr_time;
	double             delay;

	while (running) {
		// receive data from server
		packet_len =
			recvfrom(sockfd_receiver, &packet, sizeof(packet), 0, (struct sockaddr *)&addr_server, &addr_server_len);
		if (packet_len < 0) {
			perror("Failed to receive packet, ignoring ...");
			continue;
		}

#ifdef LOG_DEBUG
		printf(
			"Recd: %u %u %u (%ld bytes) from %s:%hu\n", packet.seq_num, packet.distance, packet.crc, packet_len,
			inet_ntoa(addr_server.sin_addr), ntohs(addr_server.sin_port));

		// printf(
		// 	"Received packet: %u %u %u (%ld bytes)\n", packet.seq_num, packet.distance, packet.crc,
		// 	packet_len);
		// printf("Received packet from: %s:%hu\n", inet_ntoa(addr_server.sin_addr), ntohs(addr_server.sin_port));
#endif

		// check if packet is valid
		if (!isPacketValid(packet)) {
			printf("Packet is invalid\n");
			continue;
		}

		// calculate the time taken to receive the packet
		gettimeofday(&curr_time, NULL);
		delay =
			(curr_time.tv_sec - packet.timestamp.tv_sec) * 1000 + (curr_time.tv_usec - packet.timestamp.tv_usec) / 1000.0;
		if (abs(delay) > MAX_DELAY_MS) printf("High Delay: sender -> receiver: %f\n", delay);

		// check if sequence has been skipped
		seq_diff = packet.seq_num - seq_num;
		if (seq_diff % UINT32_MAX != 1) {
			printf("Sequence number skipped: %u -> %u\n", seq_num, packet.seq_num);
		}
		seq_num = packet.seq_num;
	}

	printf("Receiver exiting\n");
	close(sockfd_receiver);

	return 0;
}