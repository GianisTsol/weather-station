# Makefile for Arduino Pro Mini 3.3V 8MHz
PORT=/dev/ttyUSB0
FQBN=arduino:avr:pro:cpu=8MHzatmega328
SRC_DIR=.

all: compile

compile:
	arduino-cli compile --fqbn $(FQBN) $(SRC_DIR)

upload: compile
	arduino-cli upload -p $(PORT) --fqbn $(FQBN) $(SRC_DIR)

clean:
	rm -rf $(SRC_DIR)/build
