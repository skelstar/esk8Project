//************************************************************
// Based on "startHere.ino" in painlessMesh library
//************************************************************
#include <Rotary.h>
#include <myPushButton.h>
#include <debugHelper.h>
//--------------------------------------------------------------
#define ENCODER_BUTTON_PIN	34	// 36 didn't work
#define ENCODER_PIN_A		16
#define ENCODER_PIN_B		17
//--------------------------------------------------------------

#define	STARTUP 		1 << 0
#define WARNING 		1 << 1
#define ERROR 			1 << 2
#define DEBUG 			1 << 3
#define COMMUNICATION 	1 << 4

debugHelper debug;

Rotary rotary = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);

//--------------------------------------------------------------------------------

// lower number = more coarse
#define ENCODER_COUNTER_MIN	-18 	// decceleration (ie -20 divides 0-127 into 20)
#define ENCODER_COUNTER_MAX	12 		// acceleration (ie 15 divides 127-255 into 15)

//--------------------------------------------------------------

void encoderInterruptHandler() {
	
	unsigned char result = rotary.process();

	// debug.print(DEBUG, "Encoder event \n");

	if (result == DIR_CW) {
		debug.print(DEBUG, "CW \n");
	}
	else if (result == DIR_CCW) {
		debug.print(DEBUG, "CCW \n");
	}
}


#define 	OFF_STATE_HIGH		HIGH
#define 	OFF_STATE_LOW       0
#define 	PULLUP              true

void listener_encoderButton( int eventCode, int eventPin, int eventParam );
myPushButton encoderButton(ENCODER_BUTTON_PIN, PULLUP, OFF_STATE_HIGH, listener_encoderButton);
void listener_encoderButton( int eventCode, int eventPin, int eventParam ) {

	switch (eventCode) {

		case encoderButton.EV_BUTTON_PRESSED:
			debug.print(DEBUG, "EV_BUTTON_PRESSED (DEADMAN) \n");
			break;
		
		case encoderButton.EV_RELEASED:
			debug.print(DEBUG, "EV_BUTTON_RELEASED (DEADMAN) \n");
			break;
		
		case encoderButton.EV_HELD_SECONDS:
			debug.print(DEBUG, "EV_BUTTON_HELD (DEADMAN): %d \n", eventParam);
			break;
	}
}

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

	// encoder
	setupEncoder();
}

void loop() {

	encoderButton.serviceEvents();
}
//----------------------------------------------------------------
void setupEncoder() {

	pinMode(ENCODER_PIN_A, INPUT_PULLUP);
	pinMode(ENCODER_PIN_B, INPUT_PULLUP);

	attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), encoderInterruptHandler, CHANGE);
	attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), encoderInterruptHandler, CHANGE);
}
