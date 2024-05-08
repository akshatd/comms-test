#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#define TIMEOUT_S 0.5
typedef struct Packet {
		uint32_t seq_num;
		uint32_t distance;
		uint32_t crc;
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
		char     ip_server[16];
		uint16_t port_sender;
		uint16_t port_receiver;
		char     ip_sender[16];
		char     ip_receiver[16];
} Config;

Config readConfig(char *filename) {
	// This function reads the configuration from a file
	Config config;
	FILE  *file = fopen(filename, "r");
	fscanf(
		file, "%s %hu %hu %s %s",
		config.ip_server,      //
		&config.port_sender,   //
		&config.port_receiver, //
		config.ip_sender,      //
		config.ip_receiver);
	fclose(file);
	return config;
}