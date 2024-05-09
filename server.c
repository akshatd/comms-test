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

	printf("Im the server!\n");

	Config config = readConfig(CONFIG_FILE);
	printf(
		"For Sender:\n- IP: %s\n- Port: %hu\nFor Receiver:\n- IP: %s\n- Port: %hu\n",
		config.ip_server,   //
		config.port_sender, //
		config.ip_receiver, //
		config.port_receiver);

	// create timeout
	struct timeval tv;
	double         to_sec, to_usec;
	to_usec    = modf(TIMEOUT_S, &to_sec) * 1000000;
	tv.tv_sec  = to_sec;
	tv.tv_usec = to_usec;

	// create UDP sockets for the sender and receiver
	int sockfd_sender = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd_sender < 0) {
		perror("Failed to create server sender socket");
		return 1;
	} else {
		if (setsockopt(sockfd_sender, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
			perror("Failed to set timeout on server sender socket");
			return 1;
		}
		printf("Server sender socket created\n");
	}

	int sockfd_receiver = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd_receiver < 0) {
		perror("Failed to create server receiver socket");
		return 1;
	} else {
		if (setsockopt(sockfd_receiver, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
			perror("Failed to set timeout on server receiver socket");
			return 1;
		}
		printf("Server receiver socket created\n");
	}

	printf("Server sockets created\n");

	// create server address
	struct sockaddr_in addr_server, addr_recver;
	addr_server.sin_family      = AF_INET;
	addr_server.sin_addr.s_addr = inet_addr(config.ip_server);
	addr_server.sin_port        = htons(config.port_sender);

	addr_recver.sin_family      = AF_INET;
	addr_recver.sin_addr.s_addr = inet_addr(config.ip_receiver);
	addr_recver.sin_port        = htons(config.port_receiver);

	// bind socket so sender can send data to it
	if (bind(sockfd_sender, (struct sockaddr *)&addr_server, sizeof(addr_server)) < 0) {
		perror("Failed to bind server sender socket");
		return 1;
	} else {
		printf("Server sender socket bound\n");
	}

	// no need to bind socket when sending data to receiver

	Packet             packet;
	ssize_t            packet_len;
	socklen_t          addrlen = sizeof(addr_server);
	struct sockaddr_in addr_sender; // sender address
	uint32_t           seq_num  = 0;
	int64_t            seq_diff = 0;
	struct timeval     curr_time;
	double             delay;
	while (running) {
		// receive data from the sender
		packet_len = recvfrom(sockfd_sender, &packet, sizeof(packet), 0, (struct sockaddr *)&addr_sender, &addrlen);
		if (packet_len < 0) {
			perror("Failed to receive packet, ignoring ...");
			continue;
		} else {
#ifdef LOG_DEBUG
			printf("Received packet: %u %u %u (%ld bytes)\n", packet.seq_num, packet.distance, packet.crc, packet_len);
			printf("Received packet from %s:%hu\n", inet_ntoa(addr_sender.sin_addr), ntohs(addr_sender.sin_port));
#endif
			if (!isPacketValid(packet)) {
				printf("Packet is invalid\n");
				continue;
			}
		}

		// calculate the time taken to send the packet
		gettimeofday(&curr_time, NULL);
		delay =
			(curr_time.tv_sec - packet.timestamp.tv_sec) * 1000 + (curr_time.tv_usec - packet.timestamp.tv_usec) / 1000.0;
		if (abs(delay) > MAX_DELAY_MS) printf("High Delay: sender -> server: %f\n", delay);

		// check if sequence has been skipped
		seq_diff = packet.seq_num - seq_num;
		if (seq_diff % UINT32_MAX != 1) {
			printf("Sequence number skipped: %u -> %u\n", seq_num, packet.seq_num);
		}
		seq_num = packet.seq_num;

		// send data to the receiver
		packet_len = sendto(sockfd_receiver, &packet, sizeof(packet), 0, (struct sockaddr *)&addr_recver, addrlen);
		if (packet_len < 0) {
			perror("Failed to send packet to receiver, ignoring ...");
		} else {
#ifdef LOG_DEBUG
			printf("Sent packet: %u %u %u (%ld bytes)\n", packet.seq_num, packet.distance, packet.crc, packet_len);
			printf("Sent packet to: %s:%hu\n", inet_ntoa(addr_recver.sin_addr), ntohs(addr_recver.sin_port));
#endif
		}
	}

	printf("Server shutting down\n");
	close(sockfd_sender);
	close(sockfd_receiver);

	return 0;
}