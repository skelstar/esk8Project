#include <SPI.h>
#include "RF24.h"
#include <Rotary.h>
#include <FastLED.h>
// #include <Adafruit_NeoPixel.h>
// #include <U8g2lib.h>

#include <TaskScheduler.h>

#include <myPushButton.h>
#include <debugHelper.h>
#include <myPowerButtonManager.h>
#include <esk8Lib.h>
#include <WiiChuck.h>

//--------------------------------------------------------------------------------

const char compile_date[] = __DATE__ " " __TIME__;

// https://sandhansblog.wordpress.com/2017/04/16/interfacing-displaying-a-custom-graphic-on-an-0-96-i2c-oled/
static const unsigned char wifiicon14x10 [] PROGMEM = {
   0xf0, 0x03, 0xfc, 0x0f, 0x1e, 0x1c, 0xc7, 0x39, 0xf3, 0x33, 0x38, 0x06, 0x1c, 0x0c, 0xc8, 0x04, 0xe0, 0x01, 0xc0, 0x00
};

//--------------------------------------------------------------------------------

#define ENCODER_BUTTON_PIN	34	// 36 didn't work
#define ENCODER_PIN_A		16
#define ENCODER_PIN_B		17

#define DEADMAN_SWITCH_PIN	25

#define POWER_WAKE_BUTTON_PIN		DEADMAN_SWITCH_PIN
#define POWER_OTHER_BUTTON_PIN		ENCODER_BUTTON_PIN

#define	PIXEL_PIN			5


// const char boardSetup[] = "DEV Board";
// #define SPI_CE				22	// white/purple
// #define SPI_CS				5  // green
const char boardSetup[] = "WEMOS TTGO Board";
#define SPI_MOSI			23 // Blue
#define SPI_MISO			19 // Orange
#define SPI_CLK				18 // Yellow
#define SPI_CE				33	// white/purple
#define SPI_CS				26  // green

#define OLED_SCL        	22    // std ESP32 (22)
#define OLED_SDA        	21    // std ESP32 (23??)

#define SEND_TO_BOARD_INTERVAL_MS	200
//--------------------------------------------------------------

#define ROLE_BOARD		1
#define ROLE_CONTROLLER	0

//int role = ROLE_MASTER;
int role = ROLE_CONTROLLER;
bool radioNumber = 1;

int sendIntervalMs = 200;
bool updateOled = false;

RF24 radio(SPI_CE, SPI_CS);	// ce pin, cs pin
//--------------------------------------------------------------

// #define     OLED_CONTRAST_HIGH	100        // 256 highest
// #define     OLED_CONTRAST_LOW	20
// U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, OLED_SCL, OLED_SDA, U8X8_PIN_NONE);

//--------------------------------------------------------------------------------

#define	STARTUP 		1 << 0
#define WARNING 		1 << 1
#define ERROR 			1 << 2
#define DEBUG 			1 << 3
#define COMMUNICATION 	1 << 4
#define THROTTLE_DEBUG	1 << 5
#define REGISTER 		1 << 6


debugHelper debug;

Rotary rotary = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);

esk8Lib esk8;

//--------------------------------------------------------------------------------

#define 	NUM_PIXELS 		8

CRGB leds[NUM_PIXELS];
//Adafruit_NeoPixel leds = Adafruit_NeoPixel(NUM_PIXELS, PIXEL_PIN, NEO_RGB + NEO_KHZ800);


#define BRIGHTNESS	20

// uint32_t COLOUR_OFF = leds.Color(0, 0, 0);
// uint32_t COLOUR_RED = leds.Color(255, 0, 0);
// uint32_t COLOUR_GREEN = leds.Color(0, 255, 0);
// uint32_t COLOUR_BLUE = leds.Color(0, 0, 255);
// uint32_t COLOUR_PINK = leds.Color(128, 0, 100);
// uint32_t COLOUR_ACCELERATING = COLOUR_BLUE;
// uint32_t COLOUR_DECELERATING = COLOUR_PINK;
// uint32_t COLOUR_THROTTLE_IDLE = COLOUR_GREEN;

CRGB COLOUR_OFF = CRGB::Black;
CRGB COLOUR_RED = CRGB::Red;
CRGB COLOUR_GREEN = CRGB::Green;
CRGB COLOUR_BLUE = CRGB::Blue;
CRGB COLOUR_PINK = CRGB::Pink;

CRGB COLOUR_ACCELERATING = CRGB::Navy;
CRGB COLOUR_DECELERATING = CRGB::Pink;
CRGB COLOUR_THROTTLE_IDLE = CRGB::Green;

CRGB COLOUR_WHITE = CRGB::White;

//--------------------------------------------------------------------------------

#define 	OFF_STATE_HIGH		HIGH
#define 	OFF_STATE_LOW       0
#define 	PULLUP              true

void listener_deadmanSwitch( int eventCode, int eventPin, int eventParam );
myPushButton deadmanSwitch(DEADMAN_SWITCH_PIN, PULLUP, OFF_STATE_HIGH, listener_deadmanSwitch);
void listener_deadmanSwitch( int eventCode, int eventPin, int eventParam ) {

	switch (eventCode) {

		case deadmanSwitch.EV_BUTTON_PRESSED:
			if (esk8.controllerPacket.throttle > 127) {
			}
			debug.print(DEBUG, "EV_BUTTON_PRESSED (DEADMAN) \n");
			break;

		case deadmanSwitch.EV_RELEASED:
			if (esk8.controllerPacket.throttle > 127) {
			 	zeroThrottleReadyToSend();
			 	//setPixels(COLOUR_DEADMAN_OFF, 0);
			}
			debug.print(DEBUG, "EV_BUTTON_RELEASED (DEADMAN) \n");
			break;

		case deadmanSwitch.EV_HELD_SECONDS:
			//Serial.printf("EV_BUTTON_HELD (DEADMAN): %d \n", eventParam);
			break;
	}
}

void listener_dialButton( int eventCode, int eventPin, int eventParam );
myPushButton dialButton(ENCODER_BUTTON_PIN, PULLUP, OFF_STATE_HIGH, listener_dialButton);
void listener_dialButton( int eventCode, int eventPin, int eventParam ) {

	switch (eventCode) {

		case dialButton.EV_BUTTON_PRESSED:
			break;

		case dialButton.EV_RELEASED:
			break;

		case dialButton.EV_HELD_SECONDS:
			break;
	}
}

//--------------------------------------------------------------

// lower number = more coarse
#define ENCODER_COUNTER_MIN	-18 	// decceleration (ie -20 divides 0-127 into 20)
#define ENCODER_COUNTER_MAX	10 		// acceleration (ie 15 divides 127-255 into 15)

#define DEADMAN_SWITCH_USED		1

//--------------------------------------------------------------

int encoderCounter = 0;
bool encoderChanged = false;
volatile bool statusChanged = true;
volatile bool packetReadyToBeSent = false;
volatile long lastPacketFromMaster = 0;
#define THROTTLE_STATUS_ACCEL	1
#define THROTTLE_STATUS_IDLE	0
#define THROTTLE_STATUS_BRAKE	-1

#define COMMS_STATUS_ONLINE		1
#define COMMS_STATUS_OFFLINE	0

#define REG_THROTTLE	0
#define REG_COMMS_STATE	1

void encoderInterruptHandler() {
	unsigned char result = rotary.process();

	// debug.print(DEBUG, "Encoder event \n");

	bool canAccelerate = deadmanSwitch.isPressed();

	if (result == DIR_CW && (canAccelerate || encoderCounter < 0)) {
		if (encoderCounter < ENCODER_COUNTER_MAX) {
			encoderCounter++;
			int throttle = getThrottleValue(encoderCounter);
			esk8.controllerPacket.throttle = throttle;
			packetReadyToBeSent = true;
			debug.print(THROTTLE_DEBUG, "encoderCounter: %d, throttle: %d \n", encoderCounter, throttle);
			statusChanged = true;
		}
	}
	else if (result == DIR_CCW) {
		if (encoderCounter > ENCODER_COUNTER_MIN) {
			encoderCounter--;
			int throttle = getThrottleValue(encoderCounter);
			esk8.controllerPacket.throttle = throttle;
			packetReadyToBeSent = true;
			debug.print(THROTTLE_DEBUG, "encoderCounter: %d, throttle: %d \n", encoderCounter, throttle);
			statusChanged = true;
		}
	}

	if (encoderCounter > 0) {
		//statusRegister[REG_THROTTLE] = THROTTLE_STATUS_ACCEL;
	}
	else if (encoderCounter < 0) {
		// statusRegister[REG_THROTTLE] = THROTTLE_STATUS_BRAKE;
	}
	else {
		//statusRegister[REG_THROTTLE] = THROTTLE_STATUS_IDLE;
	}
}

//--------------------------------------------------------------------------------

Scheduler runner;

bool tFlashLeds_onEnable();
void tFlashLedsOn_callback();
void tFlashLedsOff_callback();

// CRGB tFlashLedsColour = COLOUR_RED;

Task tFlashLeds(500, TASK_FOREVER, &tFlashLedsOff_callback);

bool tFlashLeds_onEnable() {
	// setPixels(tFlashLedsColour, 0);
	Serial.println("tFlashLeds_onEnable");
	tFlashLeds.enable();
}
void tFlashLedsOn_callback() {
	tFlashLeds.setCallback(&tFlashLedsOff_callback);
	// setPixels(tFlashLedsColour, 0);
	Serial.println("tFlashLedsOn_callback");
}
void tFlashLedsOff_callback() {
	tFlashLeds.setCallback(&tFlashLedsOn_callback);
	// setPixels(COLOUR_OFF, 0);
	Serial.println("tFlashLedsOff_callback");
}

Task tSendControllerValues(SEND_TO_BOARD_INTERVAL_MS, TASK_FOREVER, &tSendControllerValues_callback);

void tSendControllerValues_callback() {
	if (esk8.sendThenReadPacket() == true) {
		lastPacketFromMaster = millis();
	}
	updateOled = true;
	debug.print(COMMUNICATION, "tSendControllerValues_callback(): batteryVoltage:%.1f \n", esk8.boardPacket.batteryVoltage);
}
//--------------------------------------------------------------

// Accessory chuk;

//--------------------------------------------------------------
// Prototypes
void sendMessage();

bool calc_delay = false;

//--------------------------------------------------------------

volatile uint32_t otherNode;
volatile long lastRxMillis = 0;
#define COMMS_TIMEOUT_PERIOD 	1000

//--------------------------------------------------------------
void sendMessage() {
	if (esk8.sendPacketToBoard()) {
		lastPacketFromMaster = millis();
		updateOled = true;
	}
	debug.print(COMMUNICATION, "Sending message: throttle:%d \n", esk8.controllerPacket.throttle);
}
//--------------------------------------------------------------

int powerButtonIsPressed() {
	return deadmanSwitch.isPressed() &&
			dialButton.isPressed();
}

//myPowerButtonManager powerButton(ENCODER_BUTTON_PIN, HIGH, 3000, 3000, powerupEvent);
myPowerButtonManager powerButton(POWER_WAKE_BUTTON_PIN, HIGH, 3000, 3000, powerupEvent, powerButtonIsPressed);

void powerupEvent(int state) {

	switch (state) {
		case powerButton.TN_TO_POWERING_UP:
			// setPixels(COLOUR_GREEN, 0);
			// drawMessage("Booting...");
			statusChanged = true;

			break;
		case powerButton.TN_TO_POWERED_UP_WAIT_RELEASE:
			// drawMessage("Ready!");
			// setPixels(COLOUR_OFF, 0);
			// skip this and go straighht to RUNNING
			statusChanged = true;
			powerButton.setState(powerButton.TN_TO_RUNNING);
			break;
		case powerButton.TN_TO_RUNNING:
			// setPixels(COLOUR_OFF, 0);
			statusChanged = true;
			break;
		case powerButton.TN_TO_POWERING_DOWN:
			tFlashLeds.disable();	// in case comms is offline
			// drawMessage("Power down");
			zeroThrottleReadyToSend();
			// setPixels(COLOUR_RED, 0);
			statusChanged = true;
			break;
		case powerButton.TN_TO_POWERING_DOWN_WAIT_RELEASE: {
				// u8g2.clearBuffer();
				// u8g2.sendBuffer();	// clear screen
				// setPixels(COLOUR_OFF, 0);
				long pixelTime = millis();
				powerButton.setState(powerButton.TN_TO_POWER_OFF);
				statusChanged = true;
			}
			break;
		case powerButton.TN_TO_POWER_OFF:
			// setPixels(COLOUR_OFF, 0);
			statusChanged = true;
			delay(100);
			esp_deep_sleep_start();
			Serial.println("This will never be printed");
			break;
	}
}

//--------------------------------------------------------------------------------

#define COMMS_ONLINE 		1
#define COMMS_OFFLINE		0
#define COMMS_UNKNOWN_STATE	-1

volatile int commsState = COMMS_UNKNOWN_STATE;

//--------------------------------------------------------------
void setup() {

	Serial.begin(9600);

	initOLED();

	radio.begin();

	debug.init();

	debug.addOption(DEBUG, "DEBUG");
	debug.addOption(STARTUP, "STARTUP");
	debug.addOption(COMMUNICATION, "COMMUNICATION");
	debug.addOption(ERROR, "ERROR");
	debug.addOption(THROTTLE_DEBUG, "THROTTLE_DEBUG");
	debug.addOption(REGISTER, "REGISTER");

	debug.setFilter(THROTTLE_DEBUG | REGISTER);	//DEBUG | STARTUP | COMMUNICATION | ERROR);

	debug.print(STARTUP, "%s \n", compile_date);
	debug.print(STARTUP, "NOTE: %s\n", boardSetup);

	esk8.begin(&radio, role, radioNumber, &debug);

	FastLED.addLeds<TM1804, PIXEL_PIN, RGB>(leds, NUM_PIXELS);

	// leds.begin();
	FastLED.show();

	// FastLED.addLeds<NEOPIXEL, PIXEL_PIN>(leds, NUM_PIXELS);

	// FastLED.show();

	runner.startNow();
	runner.addTask(tFlashLeds);
	runner.addTask(tSendControllerValues);
	tSendControllerValues.setInterval(esk8.getSendInterval());
	tSendControllerValues.enable();

	powerButton.begin(DEBUG);

	while (powerButton.isRunning() == false) {
		powerButton.serviceButton();
	}

	// encoder
	setupEncoder();
}

void loop() {

	// chuk.readData();    // Read inputs and update maps
	// startButtonState = chuk.getButtonPlus();

	dialButton.serviceEvents();

	deadmanSwitch.serviceEvents();

	powerButton.serviceButton();

	runner.execute();

	serviceCommsState();

	// serviceOLED();

	servicePixels();

	delay(10);
}
//--------------------------------------------------------------
void servicePixels() {

	uint8_t powerState = powerButton.getState();
	debug.print(DEBUG, "powerState: %d \n", powerState);

	if (powerState == powerButton.TN_TO_POWERING_UP) {
		setPixel(COLOUR_GREEN);
		// setPixels(COLOUR_GREEN, 0);
	}
	else if (powerState == powerButton.TN_TO_POWERED_UP_WAIT_RELEASE) {
		setPixel(COLOUR_OFF);
		// setPixels(COLOUR_OFF, 0);
	}
	else if (powerState == powerButton.TN_TO_RUNNING) {
		setPixel(COLOUR_RED);
		// setPixels(COLOUR_RED, 0);
	}
	else if (powerState == powerButton.TN_TO_POWERING_DOWN) {
		setPixel(COLOUR_RED);
		// setPixels(COLOUR_RED, 0);
	}
	else if (powerState == powerButton.TN_TO_POWERING_DOWN_WAIT_RELEASE) {
		// setPixels(COLOUR_OFF, 0);
		setPixel(COLOUR_OFF);
	}
	else if (powerState == powerButton.TN_TO_POWER_OFF) {
		setPixel(COLOUR_OFF);
		// setPixels(COLOUR_OFF, 0);
	}
	else {
		setStatusRegisterPixels();
	}
}
//----------------------------------------------------------------
void setupEncoder() {

	pinMode(ENCODER_PIN_A, INPUT_PULLUP);
	pinMode(ENCODER_PIN_B, INPUT_PULLUP);

	attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), encoderInterruptHandler, CHANGE);
	attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), encoderInterruptHandler, CHANGE);
}
//--------------------------------------------------------------------------------
// void serviceOLED() {

// 	if (powerButton.isRunning()) {

// 		if (updateOled == true) {
// 			updateOled = false;
// 			u8g2.clearBuffer();
// 			drawOnline(commsState == COMMS_ONLINE);
// 			drawBatteryVoltage(esk8.boardPacket.batteryVoltage);
// 			drawThrottle(esk8.controllerPacket.throttle, deadmanSwitch.isPressed());
// 			u8g2.sendBuffer();
// 		}
// 	}
// }
//--------------------------------------------------------------------------------
void serviceCommsState() {

	bool online = esk8.boardOnline();

	if (commsState == COMMS_ONLINE && online == false) {
		setCommsState(COMMS_OFFLINE);
	}
	else if (commsState == COMMS_OFFLINE && online) {
		setCommsState(COMMS_ONLINE);
	}
	else if (commsState == COMMS_UNKNOWN_STATE) {
		setCommsState(online == false ? COMMS_OFFLINE : COMMS_ONLINE);
	}
}
//--------------------------------------------------------------------------------
void setCommsState(int newState) {
	if (newState == COMMS_OFFLINE) {
		commsState = COMMS_OFFLINE;
		statusChanged = true;
		debug.print(DEBUG, "Setting commsState: COMMS_OFFLINE\n");
		// start leds flashing
		//tFlashLedsColour = COLOUR_RED;
		//setPixels(tFlashLedsColour, 0);
		//tFlashLeds.enable();
	}
	else if (newState == COMMS_ONLINE) {
		debug.print(DEBUG, "Setting commsState: COMMS_ONLINE\n");
		commsState = COMMS_ONLINE;
		statusChanged = true;
		// stop leds flashing
		// tFlashLeds.disable();
		// setPixels(COLOUR_OFF, 0);
	}
}
//--------------------------------------------------------------------------------
void initOLED() {

	// u8g2.begin();
	// u8g2.setContrast(OLED_CONTRAST_LOW);
}

//--------------------------------------------------------------------------------
void setStatusRegisterPixels() {

	if (statusChanged == false) {
		return;
	}

	debug.print(REGISTER, "setStatusRegisterPixels()");

	if (commsState == COMMS_OFFLINE) {
		setPixel(COLOUR_RED);
	}
	else if (encoderCounter > 0) {
		setPixel(COLOUR_ACCELERATING);
	}
	else if (encoderCounter < 0) {
		setPixel(COLOUR_DECELERATING);
	}
	else if (encoderCounter == 0) {
		setPixel(COLOUR_GREEN);
	}
	else {
		setPixel(COLOUR_OFF);
	}

	statusChanged = false;



	// for (uint16_t i=0; i<NUM_PIXELS; i++) {
	// 	switch (i) {
	// 		case REG_THROTTLE:
	// 			switch (statusRegister[REG_THROTTLE]) {
	// 				case THROTTLE_STATUS_ACCEL:
	// 					leds[i] = COLOUR_ACCELERATING;
	// 					break;
	// 				case THROTTLE_STATUS_BRAKE:
	// 					leds[i] = COLOUR_DECELERATING;
	// 					break;
	// 				default:
	// 					leds[i] = COLOUR_OFF;
	// 					break;
	// 			}
	// 			break;
	// 		case REG_COMMS_STATE:
	// 			if (statusRegister[i] == COMMS_STATUS_ONLINE) {
	// 				leds[i] = COLOUR_OFF;
	// 			}
	// 			else {
	// 				leds[i] = COLOUR_RED;
	// 			}
	// 			break;
	// 	// leds[i] /= 10;
	// 	}
	// }
}
//--------------------------------------------------------------
// void setPixels(CRGB c, uint8_t wait) {
// 	for (uint16_t i=0; i<NUM_PIXELS; i++) {
// 		leds[i] = c;
// 		// leds[i] /= 10;
// 		if (wait > 0) {
// 			//FastLED.show();
// 			delay(wait);
// 		}
// 	}
// 	//FastLED.show();
// }
//--------------------------------------------------------------------------------
void setPixel(CRGB c) {
	leds[0] = c;
	//leds.show();
	FastLED.show();
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
	esk8.controllerPacket.throttle = 127;
	packetReadyToBeSent = true;
}
//--------------------------------------------------------------
#define ONLINE_SYMBOL_WIDTH 	14

// void drawOnline(bool online) {
// 	if (online) {
// 		u8g2.drawXBMP(0, 0, ONLINE_SYMBOL_WIDTH, 10, wifiicon14x10);
// 	}
// 	else {
// 		u8g2.setDrawColor(0);
// 		u8g2.drawBox(0, 0, ONLINE_SYMBOL_WIDTH, 20);
// 		u8g2.setDrawColor(1);
// 	}
// 	// u8g2.sendBuffer();
// }
// //--------------------------------------------------------------------------------
// void drawBatteryVoltage(float voltage) {
// 	u8g2.setCursor(ONLINE_SYMBOL_WIDTH+5, 10);
// 	u8g2.setFont(u8g2_font_helvR08_tf);
// 	u8g2.print(voltage, 1);
// 	u8g2.print("v");
// }
// //--------------------------------------------------------------------------------
// void drawThrottle(int throttleValue, bool show) {

// 	if (show) {
// 		char val[4];
// 		itoa(throttleValue, val, 10);
// 		int strWidth = u8g2.getStrWidth(val);

// 	  	u8g2.setFont(u8g2_font_courB18_tf);
// 		u8g2.setCursor(0, 32);
// 		u8g2.print(val);
// 		//u8g2.drawStr(128-strWidth, 32, val);
// 	}
// }
// //--------------------------------------------------------------------------------
// void drawMessage(char* msg) {
// 	u8g2.clearBuffer();
//   	u8g2.setFont(u8g2_font_courB18_tf);
// 	int strWidth = u8g2.getStrWidth(msg);
// 	u8g2.setCursor(64-(strWidth/2), 16+9);
// 	u8g2.print(msg);
// 	u8g2.sendBuffer();
// }
// //--------------------------------------------------------------------------------
