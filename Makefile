# Simple Makefile to build and run the project

BUILD_DIR := build

.PHONY: all build run-client run-server run clean

all: build

build:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake .. && $(MAKE)

run-server: build
	./$(BUILD_DIR)/src/server

run-client: build
	./$(BUILD_DIR)/src/client

run: build
	./$(BUILD_DIR)/src/server &
	SERVER_PID=$$!
	./$(BUILD_DIR)/src/client
	kill $$SERVER_PID

clean:
	rm -rf $(BUILD_DIR)
