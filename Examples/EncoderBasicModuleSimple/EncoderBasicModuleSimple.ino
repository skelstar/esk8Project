#include <debugHelper.h>

#include <EncoderBasicModule.h>
#include <Rotary.h>

//--------------------------------------------------------------------------------

#define ENCODER_PIN_A		26
#define ENCODER_PIN_B 		36

#define DEADMAN_SWITCH		35

//--------------------------------------------------------------

#define	STARTUP 		1 << 0
#define DEBUG 			1 << 1
#define COMMUNICATION 	1 << 2
#define HARDWARE		1 << 3
#define ONLINE_STATUS	1 << 5
#define TIMING			1 << 6

debugHelper debug;

void attachEncoderInterrupts() {
	attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), callback, CHANGE);
	attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), callback, CHANGE);
}

EncoderBasicModule throttleDevice;

Rotary rotary(ENCODER_PIN_A, ENCODER_PIN_B);

void callback() {
	throttleDevice.encoderInterruptHandler();
}

void encoderChangedEventCallback() {
	debug.print(DEBUG, "Encoder changed: %d \n", throttleDevice.getEncoderCount());
}
//--------------------------------------------------------------

portMUX_TYPE mmux = portMUX_INITIALIZER_UNLOCKED;

//--------------------------------------------------------------

const char compile_date[] = __DATE__ " " __TIME__;

/**************************************************************
					SETUP
**************************************************************/
void setup() {

	Serial.begin(9600);

	debug.init();
	debug.addOption(STARTUP, "STARTUP");
	debug.addOption(DEBUG, "DEBUG");
	debug.addOption(HARDWARE, "HARDWARE");
	debug.addOption(COMMUNICATION, "COMMUNICATION");
	debug.addOption(ONLINE_STATUS, "ONLINE_STATUS");
	debug.addOption(TIMING, "TIMING");
	//debug.setFilter( STARTUP | COMMUNICATION | ONLINE_STATUS | TIMING );
	debug.setFilter( STARTUP | DEBUG );

	debug.print(STARTUP, "%s \n", compile_date);
    debug.print(STARTUP, "esk8Project/Controller.ino \n");

 	pinMode(ENCODER_PIN_A, INPUT_PULLUP);
	pinMode(ENCODER_PIN_B, INPUT_PULLUP);
   	attachEncoderInterrupts();

	throttleDevice.begin(DEADMAN_SWITCH, rotary, encoderChangedEventCallback);

	xTaskCreatePinnedToCore (
		codeForEncoderTask,	// function
		"Task_Encoder",		// name
		10000,			// stack
		NULL,			// parameter
		1,				// priority
		NULL,	// handle
		0
	);				// port	
}
/**************************************************************
					LOOP
**************************************************************/
#define BOARD_OFFLINE_PERIOD	1000

void loop() {

	throttleDevice.update();

	vTaskDelay( 10 );
}
/**************************************************************
					TASK 0
**************************************************************/
void codeForEncoderTask( void *parameter ) {

	#define TX_INTERVAL 200
	long nowMs = 0;
	
	for (;;) {


		vTaskDelay( 10 );
	}

	vTaskDelete(NULL);
}
//**************************************************************

