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
	printf(
		"***\nWaiting for Sender at: %s:%hu\nRelaying to Receiver: %s:%hu\n***\n",
		config.ip_server,   //
		config.port_sender, //
		config.ip_receiver, //
		config.port_receiver);

	// setup sockets for the sender and receiver
	int sockfd_sender   = setupUdpSocket(true, config.ip_server, config.port_sender, TIMEOUT_S);
	int sockfd_receiver = setupUdpSocket(false, NULL, 0, TIMEOUT_S);
	if (sockfd_sender < 0 || sockfd_receiver < 0) {
		perror("*** ERROR: Failed to create server sockets");
		return 1;
	}

	// create receiver address
	struct sockaddr_in addr_recver;
	addr_recver.sin_family      = AF_INET;
	addr_recver.sin_addr.s_addr = inet_addr(config.ip_receiver);
	addr_recver.sin_port        = htons(config.port_receiver);

	// send data to receiver
	Packet             packet;
	ssize_t            packet_len;
	struct sockaddr_in addr_sender; // sender address
	socklen_t          addr_sender_len = sizeof(addr_sender);
	uint32_t           seq_num         = 0;
	int64_t            seq_diff        = 0;
	struct timeval     curr_time;
	double             delay;

	while (running) {
		// receive data from the sender
		packet_len = recvfrom(sockfd_sender, &packet, sizeof(packet), 0, (struct sockaddr *)&addr_sender, &addr_sender_len);
		if (packet_len < 0) {
			perror("Did not receive packet, continuing ...");
			continue;
		}

#ifdef LOG_DEBUG
		printf(
			"Recd: %u %u %u (%ld bytes) from %s:%hu\n", packet.seq_num, packet.distance, packet.crc, packet_len,
			inet_ntoa(addr_sender.sin_addr), ntohs(addr_sender.sin_port));
#endif

		// check if packet is valid
		if (!isPacketValid(packet)) {
			printf("*** ERROR: Invalid Packet: %u %u %u \n", packet.seq_num, packet.distance, packet.crc);
			continue;
		}

		// calculate the time taken to send the packet
		gettimeofday(&curr_time, NULL);
		delay =
			(curr_time.tv_sec - packet.timestamp.tv_sec) * 1000 + (curr_time.tv_usec - packet.timestamp.tv_usec) / 1000.0;
		if (abs(delay) > MAX_DELAY_MS) printf("*** WARN: High Delay: sender -> server: %f\n", delay);

		// check if sequence has been skipped
		seq_diff = packet.seq_num - seq_num;
		if (seq_diff % UINT32_MAX != 1) printf("*** WARN: Sequence number skipped: %u -> %u\n", seq_num, packet.seq_num);
		seq_num = packet.seq_num;

		// send data to the receiver
		packet_len = sendto(sockfd_receiver, &packet, sizeof(packet), 0, (struct sockaddr *)&addr_recver, addr_sender_len);
		if (packet_len < 0) {
			perror("*** ERROR: Failed to send packet, ignoring ...");
			continue;
		}

#ifdef LOG_DEBUG
		printf(
			"Sent: %u %u %u (%ld bytes) to %s:%hu\n", packet.seq_num, packet.distance, packet.crc, packet_len,
			inet_ntoa(addr_recver.sin_addr), ntohs(addr_recver.sin_port));
#endif
	}

	printf("Server shutting down...\n");
	close(sockfd_sender);
	close(sockfd_receiver);

	return 0;
}