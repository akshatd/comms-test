#include <stdio.h>

#include "common.h"

int main() {
	printf("Im the receiver!\n");
	Config config = readConfig(CONFIG_FILE);
	printf(
		"Server: %s: Send: %hu, Rcv: %hu\nSender: %s:%hu\nReceiver: %s:%hu\n",
		config.ip_server,     //
		config.port_sender,   //
		config.port_receiver, //
		config.ip_sender,     //
		config.port_sender,   //
		config.ip_receiver,   //
		config.port_receiver);
	return 0;
}