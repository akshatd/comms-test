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
	// Register signal handlers
	signal(SIGINT, sigint_handler);
	signal(SIGTERM, sigint_handler);
	signal(SIGKILL, sigint_handler);

	printf("Im the sender!\n");

	Config config = readConfig(CONFIG_FILE);
	printf("Server: %s\nPort: %hu\n", config.ip_server, config.port_sender);

	// Create a timeout
	struct timeval tv;
	tv.tv_sec  = TIMEOUT_S;
	tv.tv_usec = TIMEOUT_S * 1000000;

	// Create UDP socket to send data
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

	// Create the server address
	struct sockaddr_in server_addr;
	server_addr.sin_family      = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(config.ip_server);
	server_addr.sin_port        = htons(config.port_sender);

	// Send data to server
	Packet    sent_packet;
	ssize_t   sent_packet_len;
	socklen_t addrlen = sizeof(server_addr);
	uint32_t  seq_num = 0;
	while (running) {
		sent_packet.seq_num  = seq_num++;
		sent_packet.distance = 100;
		sent_packet.crc      = crc(sent_packet.seq_num, sent_packet.distance);
		sent_packet_len =
			sendto(sockfd_sender, &sent_packet, sizeof(sent_packet), 0, (struct sockaddr *)&server_addr, addrlen);
		if (sent_packet_len < 0) {
			perror("Failed to send packet, ignoring ...");
		} else {
			printf(
				"Sent packet: %u %u %u (%ld bytes)\n", sent_packet.seq_num, sent_packet.distance, sent_packet.crc,
				sent_packet_len);
			printf("Sent packet to: %s:%hu\n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));
		}
		sleep(TIMEOUT_S);
	}

	printf("Sender exiting\n");
	close(sockfd_sender);

	return 0;
}