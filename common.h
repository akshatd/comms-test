#include <stdbool.h>
#include <stdint.h>

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

bool IsPacketValid(Packet packet) {
	// This function checks if the CRC of the packet is correct
	return packet.crc == crc(packet.seq_num, packet.distance);
}
