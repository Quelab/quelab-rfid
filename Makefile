ARDUINO_DIR := /build/arduino-1.8.3
BOARD_TXT = $(ARDUINO_DIR)/arduino/hardware/arduino/avr/boards.txt
BOARD_TAG = nano
BOARD_SUB = atmega328
MONITOR_PORT = /dev/ttyUSB0
ARDUINO_LIBS = SoftwareSerial RFIDRdm630 elapsedMillis Arduhdlc ArduinoJson
USER_LIB_PATH = /sketchbook
SKETCH := communications.ino
include /build/makefiles/Arduino.mk