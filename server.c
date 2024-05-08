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

	printf("Im the server!\n");

	Config config = readConfig(CONFIG_FILE);
	printf(
		"Server: %s\nSend Port: %hu, Recv Port: %hu\n",
		config.ip_server,   //
		config.port_sender, //
		config.port_receiver);

	// create timeout
	struct timeval tv;
	tv.tv_sec  = TIMEOUT_S;
	tv.tv_usec = TIMEOUT_S * 1000000;

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
	struct sockaddr_in sender_addr, recver_addr;
	sender_addr.sin_family      = AF_INET;
	recver_addr.sin_family      = AF_INET;
	sender_addr.sin_addr.s_addr = inet_addr(config.ip_server);
	recver_addr.sin_addr.s_addr = inet_addr(config.ip_server);
	sender_addr.sin_port        = htons(config.port_sender);
	recver_addr.sin_port        = htons(config.port_receiver);

	// bind socket so sender can send data to it
	if (bind(sockfd_sender, (struct sockaddr *)&sender_addr, sizeof(sender_addr)) < 0) {
		perror("Failed to bind server sender socket");
		return 1;
	} else {
		printf("Server sender socket bound\n");
	}

	// no need to bind socket when sending data to receiver

	Packet             packet;
	ssize_t            packet_len;
	socklen_t          addrlen = sizeof(sender_addr);
	struct sockaddr_in sender_addr_actual; // sender address
	while (running) {
		// receive data from the sender
		packet_len = recvfrom(sockfd_sender, &packet, sizeof(packet), 0, (struct sockaddr *)&sender_addr_actual, &addrlen);
		if (packet_len < 0) {
			perror("Failed to receive packet, ignoring ...");
			continue;
		} else {
			printf("Received packet: %u %u %u (%ld bytes)\n", packet.seq_num, packet.distance, packet.crc, packet_len);
			printf(
				"Received packet from %s:%hu\n", inet_ntoa(sender_addr_actual.sin_addr), ntohs(sender_addr_actual.sin_port));
			if (!isPacketValid(packet)) {
				printf("Packet is invalid\n");
				continue;
			}
		}

		// send data to the receiver
		packet_len = sendto(sockfd_receiver, &packet, sizeof(packet), 0, (struct sockaddr *)&recver_addr, addrlen);
		if (packet_len < 0) {
			perror("Failed to send packet to receiver, ignoring ...");
		} else {
			printf("Sent packet: %u %u %u (%ld bytes)\n", packet.seq_num, packet.distance, packet.crc, packet_len);
			printf("Sent packet to: %s:%hu\n", inet_ntoa(recver_addr.sin_addr), ntohs(recver_addr.sin_port));
		}
	}

	printf("Server shutting down\n");
	close(sockfd_sender);
	close(sockfd_receiver);

	return 0;
}