#include <arpa/inet.h> // for socket stuff
#include <inttypes.h>  // for uint32_t, uint16_t
#include <math.h>      // for modf
#include <stdbool.h>   // for bool
#include <stdio.h>     // for FILE, fopen, fscanf, fclose, perror
#include <sys/time.h>  // for timeval

#define LOG_DEBUG

#define SEND_INT_S   0.5            // interval between sending packets
#define TIMEOUT_S    SEND_INT_S * 2 // timeout for receiving packets
#define MAX_DELAY_MS 0.1            // maximum delay before warning

typedef struct Packet {
		uint32_t       seq_num;
		uint32_t       distance;
		uint32_t       crc;
		struct timeval timestamp;
} Packet;

int32_t crc(uint32_t seq_num, uint32_t distance) {
	// This is a simple CRC function that is used to calculate the CRC of a packet
	// The CRC is calculated by XORing the sequence number and distance
	return seq_num ^ distance;
}

bool isPacketValid(Packet packet) {
	// This function checks if the CRC of the packet is correct
	return packet.crc == crc(packet.seq_num, packet.distance);
}

#define CONFIG_FILE "config.txt"

typedef struct Config {
		char     ip_sender[16];
		uint16_t port_sender;
		char     ip_server[16];
		uint16_t port_receiver;
		char     ip_receiver[16];
} Config;

Config readConfig(char *filename) {
	// This function reads the configuration from a file
	Config config;
	FILE  *file = fopen(filename, "r");
	fscanf(
		file, "%s %hu %s %hu %s",
		config.ip_sender,      //
		&config.port_sender,   //
		config.ip_server,      //
		&config.port_receiver, //
		config.ip_receiver);
	fclose(file);
	return config;
}

int setupUdpSocket(bool is_receiver, char *ip, uint16_t port, double timeout_s) {
	// this function sets up a UDP socket and optionally binds it to an address if it is a receiver
	// create timeout
	struct timeval tv;
	double         to_sec, to_usec;
	to_usec    = modf(timeout_s, &to_sec) * 1000000;
	tv.tv_sec  = to_sec;
	tv.tv_usec = to_usec;

	// create UDP socket
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		perror("Failed to create socket");
		return -1;
	}

	// set timeout on socket
	if (setsockopt(sockfd, SOL_SOCKET, is_receiver ? SO_RCVTIMEO : SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
		perror("Failed to set timeout on socket");
		return -1;
	}

	// bind socket if it is a receiver
	if (is_receiver) {
		struct sockaddr_in addr;
		addr.sin_family      = AF_INET;
		addr.sin_addr.s_addr = inet_addr(ip);
		addr.sin_port        = htons(port);
		if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			perror("Failed to bind socket");
			return -1;
		}
	}
	// no need to bind socket when sending data

	return sockfd;
}