CC = gcc
CFALGS = -Wall -O3
BUILD_DIR = build

$(BUILD_DIR)/server: server.c
	$(CC) $(CFALGS) server.c -o $(BUILD_DIR)/server common.h

$(BUILD_DIR)/sender: sender.c
	$(CC) $(CFALGS) sender.c -o $(BUILD_DIR)/sender common.h

$(BUILD_DIR)/receiver: receiver.c
	$(CC) $(CFALGS) receiver.c -o $(BUILD_DIR)/receiver common.h

all: $(BUILD_DIR)/server $(BUILD_DIR)/sender $(BUILD_DIR)/receiver

clean:
	rm -f $(BUILD_DIR)/*

.PHONY: all clean