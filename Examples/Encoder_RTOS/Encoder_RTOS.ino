
#include <Rotary.h>
#include <myPushButton.h>
#include <debugHelper.h>

#define ENCODER_BUTTON_PIN	34	// 36 didn't work
#define ENCODER_PIN_A		16
#define ENCODER_PIN_B		17
//--------------------------------------------------------------

#define	STARTUP 		1 << 0
#define WARNING 		1 << 1
#define ERROR 			1 << 2
#define DEBUG 			1 << 3
#define TASK1		 	1 << 4
#define TASK2		 	1 << 5

debugHelper debug;

Rotary rotary = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);

TaskHandle_t Task1;

//--------------------------------------------------------------

void encoderInterruptHandler() {
	
	unsigned char event = rotary.process();

	if (event == DIR_CW) {
		debug.print(DEBUG, "CW \n");
	}
	else if (event == DIR_CCW) {
		debug.print(DEBUG, "CCW \n");
	}
}


#define 	OFF_STATE_HIGH		HIGH
#define 	OFF_STATE_LOW       0
#define 	PULLUP              true

void listener_encoderButton( int eventCode, int eventPin, int eventParam );
myPushButton encoderButton(ENCODER_BUTTON_PIN, PULLUP, OFF_STATE_HIGH, listener_encoderButton);
void listener_encoderButton( int eventCode, int eventPin, int eventParam ) {

	debug.print(DEBUG, "Core: %d \n", xPortGetCoreID());
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

//--------------------------------------------------------------
void setup() {
	Serial.begin(9600);

	debug.init();
	debug.addOption(DEBUG, "DEBUG");
	debug.addOption(STARTUP, "STARTUP");
	debug.addOption(TASK1, "TASK1");
	debug.addOption(TASK2, "TASk2");
	debug.setFilter( DEBUG | STARTUP | TASK1 | TASK2 );

	debug.print(STARTUP, "Ready \n");

	xTaskCreatePinnedToCore (
		codeForTaskEncoder,	// function
		"Task_Encoder",		// name
		10000,			// stack
		NULL,			// parameter
		1,				// priority
		&Task1,
		0);			// handle
}

void loop() {
	debug.print(TASK2, "This is loop(); core: %d \n", xPortGetCoreID());
	delay(3000);
}
//--------------------------------------------------------------
void codeForTaskEncoder( void *parameter ) {

	long now = 0;
	setupEncoder();
	// loop forever	
	for (;;) {
		if (millis()-now > 3000) {
			debug.print(TASK1, "this is codeForTaskEncoder() core: %d \n", xPortGetCoreID());
			now = millis();
		}
		encoderButton.serviceEvents();
		delay(50);
	}

	vTaskDelete(NULL);
}
//----------------------------------------------------------------
void setupEncoder() {

	pinMode(ENCODER_PIN_A, INPUT_PULLUP);
	pinMode(ENCODER_PIN_B, INPUT_PULLUP);

	attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), encoderInterruptHandler, CHANGE);
	attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), encoderInterruptHandler, CHANGE);
}
//--------------------------------------------------------------