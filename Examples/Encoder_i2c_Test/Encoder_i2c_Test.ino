//************************************************************
// Based on "startHere.ino" in painlessMesh library
//************************************************************
#include <M5Stack.h>
#include <Wire.h>
#include "i2cEncoderLib.h"
#include <myPushButton.h>
#include <debugHelper.h>
//--------------------------------------------------------------
#define	STARTUP 		1 << 0
#define WARNING 		1 << 1
#define ERROR 			1 << 2
#define DEBUG 			1 << 3
#define COMMUNICATION 	1 << 4

debugHelper debug;

//--------------------------------------------------------------------------------

// lower number = more coarse
#define ENCODER_COUNTER_MIN	-18 	// decceleration (ie -20 divides 0-127 into 20)
#define ENCODER_COUNTER_MAX	12 		// acceleration (ie 15 divides 127-255 into 15)

i2cEncoderLib encoder(0x30); //Initialize the I2C encoder class with the I2C address 0x30 is the address with all the jumper open

int32_t counter;
uint8_t encoder_status;

//--------------------------------------------------------------

// void encoderInterruptHandler() {
	
// 	unsigned char result = rotary.process();

// 	debug.print(DEBUG, "Encoder event \n");

// 	if (result == DIR_CW) {
// 		debug.print(DEBUG, "CW \n");
// 	}
// 	else if (result == DIR_CCW) {
// 		debug.print(DEBUG, "CCW \n");
// 	}
// }


// #define 	OFF_STATE_HIGH		HIGH
// #define 	OFF_STATE_LOW       0
// #define 	PULLUP              true

// void listener_encoderButton( int eventCode, int eventPin, int eventParam );
// myPushButton encoderButton(ENCODER_BUTTON_PIN, PULLUP, OFF_STATE_HIGH, listener_encoderButton);
// void listener_encoderButton( int eventCode, int eventPin, int eventParam ) {

// 	switch (eventCode) {

// 		case encoderButton.EV_BUTTON_PRESSED:
// 			debug.print(DEBUG, "EV_BUTTON_PRESSED (DEADMAN) \n");
// 			break;
		
// 		case encoderButton.EV_RELEASED:
// 			debug.print(DEBUG, "EV_BUTTON_RELEASED (DEADMAN) \n");
// 			break;
		
// 		case encoderButton.EV_HELD_SECONDS:
// 			debug.print(DEBUG, "EV_BUTTON_HELD (DEADMAN): %d \n", eventParam);
// 			break;
// 	}
// }

//--------------------------------------------------------------------------------

void setup() {

	Serial.begin(9600);

	debug.init();
	debug.addOption(DEBUG, "DEBUG");
	debug.addOption(STARTUP, "STARTUP");
	debug.addOption(COMMUNICATION, "COMMUNICATION");
	debug.addOption(ERROR, "ERROR");
	debug.setFilter(DEBUG | STARTUP | COMMUNICATION | ERROR);

	debug.print(STARTUP, "Ready \n");
	debug.print(STARTUP, __FILE__);
	debug.print(STARTUP, "\n");

	// encoder
	setupEncoder(10, -10);
}

void loop() {

	if (encoder.updateStatus()) {
		if (encoder.readStatus(E_PUSH)) {
			Serial.println("Encoder Pushed!");
		}
		if (encoder.readStatus(E_MAXVALUE)) {
			Serial.println("Encoder Max!");
			encoder.writeLEDA(0xFF);
			delay(50);
			// encoder.writeLEDA(0x00);
		}
		if (encoder.readStatus(E_MINVALUE)) {
			Serial.println("Encoder Min!");
			encoder.writeLEDB(0xFF);
			delay(50);
			// encoder.writeLEDB(0x00);
		}
		counter = encoder.readCounterByte(); //Read only the first byte of the counter register
		Serial.print("Encoder: ");
		Serial.println(counter, DEC);
	}
}
//----------------------------------------------------------------
void setupEncoder(int maxCounts, int minCounts) {

	encoder.begin(( INTE_DISABLE | LEDE_DISABLE | WRAP_DISABLE | DIRE_RIGHT | IPUP_DISABLE | RMOD_X1 )); //INTE_ENABLE | LEDE_ENABLE | 
	encoder.writeCounter(0);
	encoder.writeMax(maxCounts); //Set maximum threshold
	encoder.writeMin(minCounts); //Set minimum threshold
	encoder.writeLEDA(0x00);
	encoder.writeLEDB(0x00);
}
