#include <arpa/inet.h> // for socket stuff
#include <inttypes.h>  // for uint32_t, uint16_t
#include <math.h>      // for modf
#include <stdbool.h>   // for bool
#include <stdio.h>     // for FILE, fopen, fscanf, fclose, perror
#include <stdlib.h>    // for qsort
#include <sys/time.h>  // for timeval

// #define LOG_DEBUG

#define SEND_INT_S   0.5 // interval between sending packets
#define TIMEOUT_S    1   // timeout for receiving packets
#define MAX_DELAY_MS 0.2 // maximum delay before warning
#define NUM_SYNC     5   // number of sync packets to send

typedef enum { Sync, Data } PacketType;

typedef struct {
		PacketType     type;
		uint32_t       seq_num;
		uint32_t       distance;
		struct timeval timestamp;
		uint32_t       crc;
} Packet;

int32_t crc(Packet packet) {
	// This is a simple CRC function that is used to calculate the CRC of a packet
	// The CRC is calculated by XORing
	return packet.type ^ packet.seq_num ^ packet.distance ^ packet.timestamp.tv_sec ^ packet.timestamp.tv_usec;
}

bool isPacketValid(Packet packet) {
	// This function checks if the CRC of the packet is correct
	bool is_valid = packet.crc == crc(packet);
	if (!is_valid) printf("*** ERROR: Packet is invalid: %u != %u\n", packet.crc, crc(packet));
	return is_valid;
}

#define CONFIG_FILE "config.txt"

typedef struct {
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
		perror("*** ERROR: Failed to create socket");
		return -1;
	}

	// set timeout on socket
	if (setsockopt(sockfd, SOL_SOCKET, is_receiver ? SO_RCVTIMEO : SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
		perror("*** ERROR: Failed to set timeout on socket");
		return -1;
	}

	// bind socket if it is a receiver
	if (is_receiver) {
		struct sockaddr_in addr;
		addr.sin_family      = AF_INET;
		addr.sin_addr.s_addr = inet_addr(ip);
		addr.sin_port        = htons(port);
		if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			perror("*** ERROR: Failed to bind socket");
			return -1;
		}
	}
	// no need to bind socket when sending data

	return sockfd;
}

int cmpDouble(const void *a, const void *b) {
	// this function is used to compare doubles for qsort
	double diff = (*(double *)a - *(double *)b);
	return diff == 0 ? 0 : diff > 0 ? 1 : -1;
}

double getMedian(double *arr, size_t len) {
	// expects odd size arrays
	qsort(arr, len, sizeof(*arr), cmpDouble);
	return arr[len / 2];
}