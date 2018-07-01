#include <SPI.h>
#include "RF24.h"
#include <Rotary.h>
#include <FastLED.h>

#include <TaskScheduler.h>

#include <myPushButton.h>
#include <debugHelper.h>
// #include <esk8Lib.h>

#include "BLEDevice.h"
#include <ESP8266VESC.h>
#include "VescUart.h"

//--------------------------------------------------------------

ESP8266VESC esp8266VESC = ESP8266VESC();

static BLEUUID serviceUUID("0000ffe0-0000-1000-8000-00805f9b34fb");
static BLEUUID    charUUID("0000ffe1-0000-1000-8000-00805f9b34fb");

static BLEAddress *pServerAddress;
static boolean doConnect = false;
static boolean connected = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;

//--------------------------------------------------------------------------------

const char compile_date[] = __DATE__ " " __TIME__;

int getThrottleValue(int raw);
void setupEncoder();
void setPixels(CRGB c) ;
void zeroThrottleReadyToSend();

//--------------------------------------------------------------------------------

#define ENCODER_BUTTON_PIN	34	// 36 didn't work
#define ENCODER_PIN_A		16
#define ENCODER_PIN_B		4

#define DEADMAN_SWITCH_PIN	25

#define	PIXEL_PIN			5

// #define SPI_MOSI			23 // Blue
// #define SPI_MISO			19 // Orange
// #define SPI_CLK				18 // Yellow
// #define SPI_CE				33	// white/purple
// #define SPI_CS				26  // green

#define OLED_SCL        	22    // std ESP32 (22)
#define OLED_SDA        	21    // std ESP32 (23??)

//--------------------------------------------------------------

int sendIntervalMs = 200;

int throttle = 127;

//--------------------------------------------------------------

#define	STARTUP 		1 << 0
#define WARNING 		1 << 1
#define ERROR 			1 << 2
#define DEBUG 			1 << 3
#define COMMUNICATION 	1 << 4
#define HARDWARE		1 << 5
#define TIMING			1 << 6
#define COMMS_STATE  	1 << 7
#define BLE 			1 << 8

debugHelper debug;

//--------------------------------------------------------------

Rotary rotary = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);

//--------------------------------------------------------------

#define 	NUM_PIXELS 		8

CRGB leds[NUM_PIXELS];

#define BRIGHTNESS	20

CRGB COLOUR_OFF = CRGB::Black;
CRGB COLOUR_RED = CRGB::Red;
CRGB COLOUR_GREEN = CRGB::Green;
CRGB COLOUR_BLUE = CRGB::Blue;
CRGB COLOUR_PINK = CRGB::Pink;

CRGB COLOUR_ACCELERATING = CRGB::Navy;
CRGB COLOUR_BRAKING = CRGB::Crimson;
CRGB COLOUR_THROTTLE_IDLE = CRGB::Green;

CRGB COLOUR_WHITE = CRGB::White;

//--------------------------------------------------------------------------------

#define	TN_ONLINE 	1
#define	ST_ONLINE 	2
#define	TN_OFFLINE  3
#define	ST_OFFLINE  4

uint8_t boardCommsState;
long lastBoardOnlineTime = 0;
long lastBoardOfflineTime = 0;

uint8_t serviceCommsState(uint8_t commsState, bool online) {
	switch (commsState) {
		case TN_ONLINE:
			//setPixels(COLOUR_OFF);
			debug.print(COMMS_STATE, "-> TN_ONLINE (offline for %ds) \n", (millis()-lastBoardOnlineTime)/1000);
			return online ? ST_ONLINE : TN_OFFLINE;
		case ST_ONLINE:
			lastBoardOnlineTime = millis();
			return  online ? ST_ONLINE : TN_OFFLINE;
		case TN_OFFLINE:
			//setPixels(COLOUR_RED);
			debug.print(COMMS_STATE, "-> TN_OFFLINE (online for %ds) \n", (millis()-lastBoardOfflineTime)/1000);
			return online ? TN_ONLINE : ST_OFFLINE;
		case ST_OFFLINE:
			return online ? TN_ONLINE : ST_OFFLINE;
		default:
			return online ? TN_ONLINE : TN_OFFLINE;
	}
}

//--------------------------------------------------------------------------------

#define 	OFF_STATE_HIGH		HIGH
#define 	OFF_STATE_LOW       0
#define 	PUSH_BUTTON_PULLUP  true

void listener_deadmanSwitch( int eventCode, int eventPin, int eventParam );
myPushButton deadmanSwitch(DEADMAN_SWITCH_PIN, PUSH_BUTTON_PULLUP, OFF_STATE_HIGH, listener_deadmanSwitch);
void listener_deadmanSwitch( int eventCode, int eventPin, int eventParam ) {

	switch (eventCode) {

		// case deadmanSwitch.EV_BUTTON_PRESSED:
		// case deadmanSwitch.EV_HELD_SECONDS:
		case deadmanSwitch.EV_RELEASED:
			if (throttle > 127) {
			 	zeroThrottleReadyToSend();
			}
			// debug.print(HARDWARE, "EV_BUTTON_RELEASED (DEADMAN) \n");
			break;
	}
}

void listener_encoderButton( int eventCode, int eventPin, int eventParam );
myPushButton encoderButton(ENCODER_BUTTON_PIN, PULLUP, OFF_STATE_HIGH, listener_encoderButton);
void listener_encoderButton( int eventCode, int eventPin, int eventParam ) {

	switch (eventCode) {

		case encoderButton.EV_BUTTON_PRESSED:
			break;

		case encoderButton.EV_RELEASED:
            zeroThrottleReadyToSend();
			break;

		case encoderButton.EV_HELD_SECONDS:
			break;
	}
}
//--------------------------------------------------------------
// lower number = more coarse
#define ENCODER_COUNTER_MIN	-18 	// decceleration (ie -20 divides 0-127 into 20)
#define ENCODER_COUNTER_MAX	10 		// acceleration (ie 15 divides 127-255 into 15)

int encoderCounter = 0;

//--------------------------------------------------------------------------------

Scheduler runner;

#define FAST_FLASH_DURATION     300

void tFastFlash_callback();
Task tFastFlash(FAST_FLASH_DURATION, 2, &tFastFlash_callback);
void tFastFlash_callback() {
    if (tFastFlash.isLastIteration()) {
        setPixels(COLOUR_OFF);
        tFastFlash.disable();
    }
}

void fastFlashLed(CRGB c) {
	setPixels(c);
    tFastFlash.setIterations(2);
    tFastFlash.enable();
}

void fastFlashLed() {
    tFastFlash.setIterations(2);
    tFastFlash.enable();
}

//------------------------------------------------------------------------------
bool tFlashLeds_onEnable();
void tFlashLedsOn_callback();
void tFlashLedsOff_callback();
CRGB tFlashLedsColour = COLOUR_RED;

Task tFlashLeds(500, TASK_FOREVER, &tFlashLedsOff_callback);

bool tFlashLeds_onEnable() {
	setPixels(tFlashLedsColour);
	tFlashLeds.enable();
    return true;
}
void tFlashLedsOn_callback() {
	tFlashLeds.setCallback(&tFlashLedsOff_callback);
	setPixels(tFlashLedsColour);
}
void tFlashLedsOff_callback() {
	tFlashLeds.setCallback(&tFlashLedsOn_callback);
	setPixels(COLOUR_OFF);
}
// //--------------------------------------------------------------
// void tSendControllerValues_callback() {
// 	sendThrottleToVESC();
// }
// Task tSendControllerValues(SEND_TO_BOARD_INTERVAL_MS, TASK_FOREVER, &tSendControllerValues_callback);
//------------------------------------------------------------------------------

void encoderInterruptHandler() {
	unsigned char result = rotary.process();

	bool canAccelerate = deadmanSwitch.isPressed();

	if (result == DIR_CW) {
		debug.print(HARDWARE, "DIR_CW %d \n", encoderCounter);		
	}
	else if (result == DIR_CCW) {
		debug.print(HARDWARE, "DIR_CCW %d \n", encoderCounter);
	}

	if (result == DIR_CW && (canAccelerate || encoderCounter < 0)) {
		if (encoderCounter < ENCODER_COUNTER_MAX) {
			encoderCounter++;
			throttle = getThrottleValue(encoderCounter);
			debug.print(HARDWARE, "encoderCounter: %d, throttle: %d \n", encoderCounter, throttle);
		}
	}
	else if (result == DIR_CCW) {
		if (encoderCounter > ENCODER_COUNTER_MIN) {
			encoderCounter--;
			throttle = getThrottleValue(encoderCounter);
			debug.print(HARDWARE, "encoderCounter: %d, throttle: %d \n", encoderCounter, throttle);
		}
	}
}

//--------------------------------------------------------------

volatile uint32_t otherNode;
volatile long lastRxMillis = 0;
#define COMMS_TIMEOUT_PERIOD 	1000

//--------------------------------------------------------------
void sendThrottleToVESC() {
}
//--------------------------------------------------------------------------------

/**************************************************************
					SETUP
**************************************************************/
void setup() {

	Serial.begin(9600);

	debug.init();
	debug.addOption(DEBUG, "DEBUG");
	debug.addOption(STARTUP, "STARTUP");
	debug.addOption(COMMUNICATION, "COMMUNICATION");
	debug.addOption(ERROR, "ERROR");
	debug.addOption(HARDWARE, "HARDWARE");
	debug.addOption(COMMS_STATE, "COMMS_STATE");
    debug.setFilter( BLE | STARTUP | HARDWARE );	// DEBUG | STARTUP | COMMUNICATION | ERROR | HARDWARE);

	debug.print(STARTUP, "%s \n", compile_date);
    debug.print(STARTUP, "esk8Project/Controller.ino \n");

	// FastLED.addLeds<NEOPIXEL, PIXEL_PIN>(leds, NUM_PIXELS);
	// FastLED.show();
    debug.print(STARTUP, "FastLED \n");

	runner.startNow();
    runner.addTask(tFastFlash);
	runner.addTask(tFlashLeds);
    debug.print(STARTUP, "runner \n");

	xTaskCreatePinnedToCore (
		codeForEncoderTask,	// function
		"Task_Encoder",		// name
		10000,			// stack
		NULL,			// parameter
		1,				// priority
		NULL,	// handle
		0);				// port	
}
/**************************************************************
					LOOP
**************************************************************/
long now = 0;

void loop() {
    //debug.print(STARTUP, "loop \n");

	delay(10);
}
/**************************************************************
					TASK 0
**************************************************************/
void codeForEncoderTask( void *parameter ) {

	long task0now = 0;

    // debug.print(STARTUP, "codeForEncoderTask \n");

	setupEncoder();

	// then loop forever	
	for (;;) {
		encoderButton.serviceEvents();
		deadmanSwitch.serviceEvents();
		delay(10);
	}

	vTaskDelete(NULL);
}
//**************************************************************
void setupEncoder() {

	pinMode(ENCODER_PIN_A, INPUT_PULLUP);
	pinMode(ENCODER_PIN_B, INPUT_PULLUP);

	attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), encoderInterruptHandler, CHANGE);
	attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), encoderInterruptHandler, CHANGE);
}
//--------------------------------------------------------------------------------
void setPixels(CRGB c) {
	// for (uint16_t i=0; i<NUM_PIXELS; i++) {
	// 	leds[i] = c;
	// 	leds[i] /= 10;
	// }
	// FastLED.show();
}
//--------------------------------------------------------------------------------
int getThrottleValue(int raw) {
	int mappedThrottle = 0;

	if (raw >= 0) {
		mappedThrottle = map(raw, 0, ENCODER_COUNTER_MAX, 127, 255);
	}
	else {
		mappedThrottle = map(raw, ENCODER_COUNTER_MIN, 0, 0, 127);
	}

	return mappedThrottle;
}
//--------------------------------------------------------------------------------
void zeroThrottleReadyToSend() {
	encoderCounter = 0;
	throttle = 127;
    debug.print(HARDWARE, "encoderCounter: %d, throttle: %d [ZERO] \n", encoderCounter, throttle);
}
//--------------------------------------------------------------
#define ONLINE_SYMBOL_WIDTH 	14
