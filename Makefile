# Simple Makefile to build and run the project

BUILD_DIR := build

.PHONY: all build run-client run-server run clean

all: build

build:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake .. && $(MAKE)

run-server: build
	./$(BUILD_DIR)/server

run-client: build
	./$(BUILD_DIR)/client

run: build
	./$(BUILD_DIR)/server &
	SERVER_PID=$$!
	./$(BUILD_DIR)/client
	kill $$SERVER_PID

clean:
	rm -rf $(BUILD_DIR)
