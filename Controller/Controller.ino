#include <SPI.h>
#include "RF24.h"
#include <Rotary.h>
#include <FastLED.h>

#include <TaskScheduler.h>

#include <myPushButton.h>
#include <debugHelper.h>
#include <esk8Lib.h>

#include <U8g2lib.h>

// #include "VescUart.h"

//--------------------------------------------------------------

#define	STARTUP 		1 << 0
#define DEBUG 			1 << 1
#define COMMUNICATION 	1 << 2
#define HARDWARE		1 << 3
#define BLE 			1 << 4
#define ONLINE_STATUS	1 << 5
#define TIMING			1 << 6

debugHelper debug;

esk8Lib esk8;

//--------------------------------------------------------------

long volatile lastSendGetValuesRequestToVesc = 0;
bool volatile valuesUpdated = true;

//--------------------------------------------------------------

U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);   // Adafruit Feather ESP8266/32u4 Boards + FeatherWing OLED

//--------------------------------------------------------------------------------

const char compile_date[] = __DATE__ " " __TIME__;

int mapEncoderToThrottleValue(int raw);
void setupEncoder();
void setPixels(uint32_t c) ;
void zeroThrottleReadyToSend();

//--------------------------------------------------------------------------------

#define ENCODER_BUTTON_PIN	34	// 36 didn't work
#define ENCODER_PIN_A		16
#define ENCODER_PIN_B		4

#define DEADMAN_SWITCH_PIN	25

#define	PIXEL_PIN			5

//--------------------------------------------------------------

// int sendIntervalMs = 200;

bool throttleChanged = false;
int throttle = 127;

#define SEND_TO_BOARD_INTERVAL_MS 			500
#define GET_VALUES_FROM_VESC_INTERVAL_MS	5000

//--------------------------------------------------------------

Rotary rotary = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);

//--------------------------------------------------------------

#define SPI_MOSI        23 // Blue
#define SPI_MISO        19 // Orange
#define SPI_CLK			18 // Yellow
#define SPI_CE          33    // white/purple
#define SPI_CS          26  // green

#define ROLE_BOARD        1
#define ROLE_CONTROLLER    0

//int role = ROLE_MASTER;
int role = ROLE_CONTROLLER;
bool radioNumber = 1;

int sendIntervalMs = 200;

RF24 radio(SPI_CE, SPI_CS);    // ce pin, cs pin




#define 	NUM_PIXELS 		8

#define BRIGHTNESS	5

CRGB COLOUR_OFF = CRGB::Black;
CRGB COLOUR_RED = CRGB::Red;
CRGB COLOUR_GREEN = CRGB::Green;
CRGB COLOUR_BLUE = CRGB::Blue;
CRGB COLOUR_PINK = CRGB::Pink;

CRGB COLOUR_ACCELERATING = CRGB::Navy;
CRGB COLOUR_BRAKING = CRGB::Crimson;
CRGB COLOUR_THROTTLE_IDLE = CRGB::Green;

CRGB COLOUR_WHITE = CRGB::White;

CRGB leds[NUM_PIXELS];

// Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_PIXELS, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

// uint32_t COLOUR_OFF = strip.Color(0, 0, 0);
// uint32_t COLOUR_RED = strip.Color(255, 0, 0);
// uint32_t COLOUR_GREEN = strip.Color(0, 255, 0);
// uint32_t COLOUR_BLUE = strip.Color(0, 0, 255);
// uint32_t COLOUR_WHITE = strip.Color(255, 255, 255);

// uint32_t COLOUR_BRAKING = COLOUR_GREEN;
// uint32_t COLOUR_THROTTLE_IDLE = COLOUR_WHITE;
// uint32_t COLOUR_ACCELERATING = COLOUR_BLUE;

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
            if (esk8.controllerPacket.throttle > 127) {
			 	zeroThrottleReadyToSend();
			}
			debug.print(HARDWARE, "EV_BUTTON_RELEASED (DEADMAN) \n");
			break;
	}
}

void listener_encoderButton( int eventCode, int eventPin, int eventParam );
myPushButton encoderButton(ENCODER_BUTTON_PIN, PULLUP, OFF_STATE_HIGH, listener_encoderButton);
void listener_encoderButton( int eventCode, int eventPin, int eventParam ) {

	switch (eventCode) {

		case encoderButton.EV_BUTTON_PRESSED:
            esk8.controllerPacket.encoderButton = 1;
			break;

		case encoderButton.EV_RELEASED:
            esk8.controllerPacket.encoderButton = 0;
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
        //setPixels(COLOUR_OFF);
        tFastFlash.disable();
    }
}

// void fastFlashLed(CRGB c) {
// 	setPixels(c);
//     tFastFlash.setIterations(2);
//     tFastFlash.enable();
// }

// void fastFlashLed() {
//     tFastFlash.setIterations(2);
//     tFastFlash.enable();
// }

//------------------------------------------------------------------------------

CRGB tFlashLedsColour = COLOUR_RED;

bool tFlashLeds_onEnable();
void tFlashLeds_onDisable();
void tFlashLedsOn_callback();
void tFlashLedsOff_callback();

Task tFlashLeds(500, TASK_FOREVER, &tFlashLedsOff_callback);

bool tFlashLeds_onEnable() {
	setPixels(tFlashLedsColour);
	tFlashLeds.enable();
    return true;
}
void tFlashLeds_onDisable() {
	setPixels(COLOUR_OFF);
	tFlashLeds.disable();
}
void tFlashLedsOn_callback() {
	tFlashLeds.setCallback(&tFlashLedsOff_callback);
	setPixels(tFlashLedsColour);
	return;
}
void tFlashLedsOff_callback() {
	tFlashLeds.setCallback(&tFlashLedsOn_callback);
	setPixels(COLOUR_OFF);
	return;
}
//--------------------------------------------------------------
//--------------------------------------------------------------

void sendMessage() {

    int result = esk8.sendThenReadACK();

    if (result == esk8.CODE_SUCCESS) {
		throttleChanged = false;
        debug.print(COMMUNICATION, "sendMessage(): batteryVoltage:%.1f \n", esk8.boardPacket.batteryVoltage);
    }
    else if (result == esk8.ERR_NOT_SEND_OK) {
        debug.print(COMMUNICATION, "tSendControllerValues_callback(): ERR_NOT_SEND_OK \n");
    }
    else if (result == esk8.ERR_TIMEOUT) {
        debug.print(COMMUNICATION, "tSendControllerValues_callback(): ERR_TIMEOUT \n");
    }
    else {
        debug.print(COMMUNICATION, "tSendControllerValues_callback(): UNKNOWN_CODE %d \n", result);    
    }
    // boardCommsState = serviceCommsState(boardCommsState, result == esk8.CODE_SUCCESS);
}

void tSendControllerValues_callback() {
    sendMessage();
}
Task tSendControllerValues(SEND_TO_BOARD_INTERVAL_MS, TASK_FOREVER, &tSendControllerValues_callback);

//--------------------------------------------------------------
#define	TN_ONLINE 	1
#define	ST_ONLINE 	2
#define	TN_OFFLINE  3
#define	ST_OFFLINE  4

class OnlineStatus 
{
	private:
		uint8_t state = ST_ONLINE;
		uint8_t oldstate = ST_ONLINE;

	public:

		bool serviceState(bool online) {
			switch (state) {
				case TN_ONLINE:
					state = online ? ST_ONLINE : TN_OFFLINE;
					tSendControllerValues.enable();
					break;
				case ST_ONLINE:
					state = online ? ST_ONLINE : TN_OFFLINE;
					break;
				case TN_OFFLINE:
					state = online ? TN_ONLINE : ST_OFFLINE;
					break;
				case ST_OFFLINE:
					state = online ? TN_ONLINE : ST_OFFLINE;
					break;
				default:
					state = online ? TN_ONLINE : TN_OFFLINE;
					break;
			}
			bool stateChanged = oldstate != state;
			oldstate = state;
			return stateChanged;
		}

		bool getState() { return state; }
		bool isOnline() { return state == ST_ONLINE; }
};

OnlineStatus vescCommsStatus;

//------------------------------------------------------------------------------

void encoderInterruptHandler() {
	unsigned char result = rotary.process();

	bool canAccelerate = deadmanSwitch.isPressed();

	if (result == DIR_CW && (canAccelerate || encoderCounter < 0)) {
		if (encoderCounter < ENCODER_COUNTER_MAX) {
			encoderCounter++;
			esk8.controllerPacket.throttle = mapEncoderToThrottleValue(encoderCounter);
			debug.print(HARDWARE, "encoderCounter: %d, throttle: %d \n", encoderCounter, esk8.controllerPacket.throttle);
			throttleChanged = true;
		}
	}
	else if (result == DIR_CCW) {
		if (encoderCounter > ENCODER_COUNTER_MIN) {
			encoderCounter--;
			esk8.controllerPacket.throttle = mapEncoderToThrottleValue(encoderCounter);
			debug.print(HARDWARE, "encoderCounter: %d, throttle: %d \n", encoderCounter, esk8.controllerPacket.throttle);
			throttleChanged = true;
		}
	}
}

//--------------------------------------------------------------

volatile uint32_t otherNode;
volatile long lastRxMillis = 0;
#define COMMS_TIMEOUT_PERIOD 	1000

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
	debug.addOption(BLE, "BLE");
	debug.addOption(ONLINE_STATUS, "ONLINE_STATUS");
	debug.addOption(TIMING, "TIMING");
    // debug.setFilter( STARTUP | HARDWARE | DEBUG | BLE | ONLINE_STATUS | TIMING );	// DEBUG | STARTUP | COMMUNICATION | ERROR | HARDWARE);
	debug.setFilter( STARTUP | COMMUNICATION );	// DEBUG | STARTUP | COMMUNICATION | ERROR | HARDWARE);

	debug.print(STARTUP, "%s \n", compile_date);
    debug.print(STARTUP, "esk8Project/Controller.ino \n");

	throttleChanged = true;	// initialise

    radio.begin();

    btStop();   // turn bluetooth module off

	esk8.begin(&radio, role, radioNumber, &debug);

	u8g2.begin();
	oled2LineMessage("AVG MOTOR CURRENT", "1000", "mAH");

	sendMessage();

	runner.startNow();
 	// runner.addTask(tFastFlash);
	runner.addTask(tFlashLeds);
	runner.addTask(tSendControllerValues);

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
long nowms = 0;
long connectedNow = 0;
bool oldConnected = false;

void loop() {

	bool connected = true;

	vescCommsStatus.serviceState(connected);

	if (throttleChanged && connected) {
		tSendControllerValues.restart();
	}

	runner.execute();

	delay(10);
}
/**************************************************************
					TASK 0
**************************************************************/
void codeForEncoderTask( void *parameter ) {

	long task0now = 0;
	long oldConnected = true;

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
	for (uint16_t i=0; i<NUM_PIXELS; i++) {
		leds[i] = c;		
	}
	FastLED.show();
}
//--------------------------------------------------------------------------------
int mapEncoderToThrottleValue(int raw) {
	int mappedThrottle = 0;
	int rawMiddle = 0;

	if (raw >= rawMiddle) {
		return map(raw, rawMiddle, ENCODER_COUNTER_MAX, 127, 255);
	}
	return map(raw, ENCODER_COUNTER_MIN, rawMiddle, 0, 127);
}
//--------------------------------------------------------------------------------
void zeroThrottleReadyToSend() {
	encoderCounter = 0;
	throttle = 127;
	throttleChanged = true;
    //debug.print(HARDWARE, "encoderCounter: %d, throttle: %d [ZERO] \n", encoderCounter, throttle);
}
//--------------------------------------------------------------
void oled2LineMessage(char* line1, char* line2, char* unit) {
	
	u8g2.clearBuffer();
	// line1
	u8g2.setFont(u8g2_font_helvR08_tf);	// u8g2_font_courR08_tf
	u8g2.drawStr(0, 10, line1);
	// unit
	u8g2_uint_t unitWidth = u8g2.getStrWidth((const char*) unit);
	u8g2.setFont(u8g2_font_helvR08_tf);	// u8g2_font_courR08_tf u8g2_font_helvR08_tf
	u8g2.drawStr(128-unitWidth, 32, unit);
	//line2
	u8g2.setFont(u8g2_font_courB18_tf);	// u8g2_font_courB18_tf u8g2_font_courB24_tf
	u8g2_uint_t width = u8g2.getStrWidth((const char*) line2);
	int unitOffset = unitWidth + 2;
	if (strcmp(unit, "") == 0) {
		unitOffset = 0;
	}
	u8g2.drawStr(128-width-unitOffset, 32, line2);

	u8g2.sendBuffer();
}