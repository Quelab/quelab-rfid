#include <SoftwareSerial.h>
#include "RFIDRdm630.h"
#include "Arduhdlc.h"
#include <ArduinoJson.h>
#include <elapsedMillis.h>

typedef enum {
  closed = 0,
  open = 1
} switch_state_t;

typedef enum {
  unlocked = 0,
  locked = 1
} lock_state_t;

typedef struct switch_status {
    uint32_t last_change_time;
    uint8_t input_pin;
    union {
        switch_state_t debounced_door_state;
        lock_state_t debounced_lock_state;
    };
    union {
        switch_state_t last_door_state;
        lock_state_t last_lock_state;
    };
} switch_status_t;

#define BOUNCE_PERIOD 200 //Contact switch state changes less than this period are considered bounces

/* Pre-allocated buffers
1) ArduinoJson Buffer > raw message buffer see online calculator
2) HDLC message buffer > slightly larger than raw buffer
3) raw message buffer > actual json bytes
*/
// messages sent over serial and the maximum size of a message in bytes
const size_t CRC_SIZE = 2;
const size_t FRAME_CHARS = 2;
const size_t max_message_size = 124;

// 1 bytes for constructing JSON data
StaticJsonBuffer<200> jsonBuffer;

// 2 buffer allocated for HDLC communications
const size_t max_hdlc_frame_length = max_message_size + CRC_SIZE + FRAME_CHARS;

// 3 raw message buffer
char send_message_buffer[max_message_size];
size_t send_message_size = 0;

// IO pins
const unsigned int rxPin = 2;  // pin that will receive the data
const unsigned int txPin = 3;  // connection not necessary.
const unsigned int lock_ctrl = 4; // Set high to signal door to unlock
const unsigned int red_led = 5;   // Turn on to indicate  locked key
const unsigned int green_led = 6; // Turn on to indicate unlocked key
const unsigned int door_contact = 7; // Is low when contact switch indicates door is closed
const unsigned int lock_contact = 8; // Is low when electronic lock indicates door is unlocked
const unsigned int open_sign = 11; // Set low to turn on the OPEN sign

switch_status_t door_status;
switch_status_t lock_status;
switch_state_t  open_sign_state;
int i = 0;

// RFID
RFIDtag  tag;  // RFIDtag object
RFIDRdm630 reader = RFIDRdm630(rxPin,txPin);

// Timer
elapsedMillis lock_timer;
elapsedMillis status_of_health_timer;
const unsigned int unlocked_interval = 5000; /* Lock is open for 5 seconds */
const unsigned int status_of_health_interval = 30000; /* health status every 30s */

boolean lock_timer_active;

/* Function to send out byte/char */
void send_character(uint8_t data);

/* Function to handle a valid HDLC frame */
void hdlc_frame_handler(const uint8_t *data, uint16_t length);

/* Initialize Arduhdlc library with three parameters.
1. Character send function, to send out HDLC frame one byte at a time.
2. HDLC frame handler function for received frame.
3. Length of the longest frame used, to allocate buffer in memory */
Arduhdlc hdlc(&send_character, &hdlc_frame_handler, max_hdlc_frame_length);

/* Function to send out one 8bit character */
void send_character(uint8_t data) {
    Serial.print((char)data);
}

void lockDoor(){
    lock_timer_active = false;
    digitalWrite(lock_ctrl, LOW);
}

void unlockDoor(){
    lock_timer = 0;
    lock_timer_active = true;
    digitalWrite(lock_ctrl, HIGH);
}

/* Frame handler function. What to do with received data? */
void hdlc_frame_handler(const uint8_t *data, uint16_t length) {
    // Do something with data that is in framebuffer
    JsonObject& root = jsonBuffer.parseObject(data);
    if (root.success()) {
        String command = root["message"];
        if (command == "lock_ctrl"){
            bool unlock = root["unlock"];
            if (unlock == true){
                unlockDoor();
            } else {
                lockDoor();
            }
        }
    }
    jsonBuffer.clear(); // global buffer is not reused otherwise
}

void sendStatus(){
    JsonObject& root = jsonBuffer.createObject();
    root["message"] = "status";
    root["door_open"] = (door_status.debounced_door_state == open)? true: false;
    root["locked"] = (lock_status.debounced_lock_state == locked)? true: false;
    root["lock_open"] = (digitalRead(lock_ctrl)? true: false);
    root["open_sign_on"] =   (open_sign_state == open)? true: false;
    send_message_size = root.printTo(send_message_buffer, max_message_size);
    hdlc.frameDecode(send_message_buffer, send_message_size);
    jsonBuffer.clear(); // global buffer is not reused otherwise
}

void sendTagInfo(RFIDtag  *tag){
    JsonObject& root = jsonBuffer.createObject();
    root["message"] = "rfid_card";
    root["rfid_hex"] = tag->getTag();
    root["rfid"] = tag->getCardNumber();
    send_message_size = root.printTo(send_message_buffer, max_message_size);
    hdlc.frameDecode(send_message_buffer, send_message_size);
    jsonBuffer.clear(); // global buffer is not reused otherwise
}

char switchState(switch_status_t *switch_status) {

  unsigned long newtime;
  unsigned char current_state;

  current_state = digitalRead(switch_status->input_pin);
  newtime=millis();

  if(current_state != switch_status->last_door_state) {
    switch_status->last_door_state = current_state;
    switch_status->last_change_time=newtime;
  } else if((newtime - switch_status->last_change_time) >= BOUNCE_PERIOD){
    switch_status->debounced_door_state = current_state;
  }
  return switch_status->debounced_door_state;
}

void processSwitches() {
  /* Send a message based on Door State + Door Lock State */
  static switch_state_t last_door_state = closed;
  static lock_state_t last_lock_state = locked;

  switchState(&door_status);
  switchState(&lock_status);

  if (door_status.debounced_door_state != last_door_state ||
      lock_status.debounced_lock_state != last_lock_state){
    last_lock_state = lock_status.debounced_lock_state;
    last_door_state = door_status.debounced_door_state;
    digitalWrite(red_led,  lock_status.debounced_lock_state);
    digitalWrite(green_led, !lock_status.debounced_lock_state);

    sendStatus();
  }
}

void setup() {
    pinMode(1,OUTPUT); // Serial port TX to output
    pinMode(lock_ctrl, OUTPUT);
    pinMode(red_led, OUTPUT);
    pinMode(green_led, OUTPUT);
    pinMode(door_contact, INPUT);
    pinMode(lock_contact, INPUT);
    pinMode(open_sign, OUTPUT);

    door_status.input_pin = door_contact;
    door_status.last_door_state = open;
    door_status.debounced_door_state = open;
    door_status.last_change_time = 0;

    lock_status.input_pin = lock_contact;
    lock_status.last_lock_state = locked;
    lock_status.debounced_lock_state = locked;
    lock_status.last_change_time = 0;

    open_sign_state = closed;
    digitalWrite(open_sign, open_sign_state);
    digitalWrite(green_led, lock_status.debounced_lock_state);
    digitalWrite(red_led, !lock_status.debounced_lock_state);
    digitalWrite(lock_ctrl, LOW);

    lock_timer_active = false;

    // initialize serial port to 9600 baud
    Serial.begin(9600);
}

void loop() {
    if (reader.isAvailable()){  // tests if a card was read by the module
        tag = reader.getTag();  // if true, then receives a tag object
        sendTagInfo(&tag);
    }
    processSwitches();
    if (status_of_health_timer >= status_of_health_interval){
        status_of_health_timer -= status_of_health_interval;
        sendStatus();
    }
    if (lock_timer_active && lock_timer >= unlocked_interval){
        lockDoor();
    }
}

/*
SerialEvent occurs whenever a new data comes in the
hardware serial RX.  This routine is run between each
time loop() runs, so using delay inside loop can delay
response.  Multiple bytes of data may be available.
*/
void serialEvent() {
    while (Serial.available()) {
        // get the new byte:
        char inChar = (char)Serial.read();
        // Pass all incoming data to hdlc char receiver
        hdlc.charReceiver(inChar);
    }
}