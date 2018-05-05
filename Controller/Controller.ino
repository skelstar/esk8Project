#include <SPI.h>
#include "RF24.h"
#include <Rotary.h>
#include <FastLED.h>
#include <U8g2lib.h>

#include <TaskScheduler.h>

#include <myPushButton.h>
#include <debugHelper.h>
#include <myPowerButtonManager.h>
#include <esk8Lib.h>

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

#define	PIXEL_PIN			5
	
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

/* Hardware configuration: Set up nRF24L01 radio on SPI bus plus pins 7 & 8 */
RF24 radio(SPI_CE, SPI_CS);	// ce pin, cs pin
//--------------------------------------------------------------

#define     OLED_CONTRAST_HIGH	100        // 256 highest
#define     OLED_CONTRAST_LOW	20
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, OLED_SCL, OLED_SDA, U8X8_PIN_NONE);

//--------------------------------------------------------------------------------

debugHelper debug;

Rotary rotary = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);

esk8Lib esk8;

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
			 	//zeroThrottleReadyToSend();
			}
			debug.print(d_DEBUG, "EV_BUTTON_PRESSED (DEADMAN) \n");
			break;
		
		case deadmanSwitch.EV_RELEASED:
			if (esk8.controllerPacket.throttle > 127) {
			 	zeroThrottleReadyToSend();
			}
			debug.print(d_DEBUG, "EV_BUTTON_RELEASED (DEADMAN) \n");
			break;
		
		case deadmanSwitch.EV_HELD_SECONDS:
			//Serial.printf("EV_BUTTON_HELD (DEADMAN): %d \n", eventParam);
			break;
	}
}

// lower number = more coarse
#define ENCODER_COUNTER_MIN	-18 	// decceleration (ie -20 divides 0-127 into 20)
#define ENCODER_COUNTER_MAX	12 		// acceleration (ie 15 divides 127-255 into 15)

#define DEADMAN_SWITCH_USED		1

//--------------------------------------------------------------

int encoderCounter = 0;
bool encoderChanged = false;
volatile bool packetReadyToBeSent = false;
volatile long lastPacketFromMaster = 0;

void encoderInterruptHandler() {
	unsigned char result = rotary.process();

	// debug.print(d_DEBUG, "Encoder event \n");

	bool canAccelerate = deadmanSwitch.isPressed();	// || DEADMAN_SWITCH_USED == 0;

	if (result == DIR_CW && (canAccelerate || encoderCounter < 0)) {
		if (encoderCounter < ENCODER_COUNTER_MAX) {

			encoderCounter++;
			int throttle = getThrottleValue(encoderCounter);
			esk8.controllerPacket.throttle = throttle;
			packetReadyToBeSent = true;
			debug.print(d_DEBUG, "encoderCounter: %d, throttle: %d \n", encoderCounter, throttle);
		}
	}
	else if (result == DIR_CCW) {
		if (encoderCounter > ENCODER_COUNTER_MIN) {
			encoderCounter--;
			int throttle = getThrottleValue(encoderCounter);
			esk8.controllerPacket.throttle = throttle;
			packetReadyToBeSent = true;
			debug.print(d_DEBUG, "encoderCounter: %d, throttle: %d \n", encoderCounter, throttle);
		}
	}
}

//--------------------------------------------------------------------------------

#define 	NUM_PIXELS 		8

CRGB leds[NUM_PIXELS];
//Adafruit_NeoPixel leds = Adafruit_NeoPixel(NUM_PIXELS, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

#define BRIGHTNESS	20

CRGB COLOUR_OFF = CRGB::Black;
CRGB COLOUR_RED = CRGB::Red;
CRGB COLOUR_GREEN = CRGB::Green;
CRGB COLOUR_BLUE = CRGB::Blue;
CRGB COLOUR_WHITE = CRGB::White;

//--------------------------------------------------------------------------------

Scheduler runner;

bool tFlashLeds_onEnable();
void tFlashLedsOn_callback();
void tFlashLedsOff_callback();

CRGB tFlashLedsColour = COLOUR_RED;

Task tFlashLeds(500, TASK_FOREVER, &tFlashLedsOff_callback);

bool tFlashLeds_onEnable() {
	setPixels(tFlashLedsColour, 0);
	Serial.println("tFlashLeds_onEnable");
	tFlashLeds.enable();
}
void tFlashLedsOn_callback() {
	tFlashLeds.setCallback(&tFlashLedsOff_callback);
	setPixels(tFlashLedsColour, 0);
	Serial.println("tFlashLedsOn_callback");
}
void tFlashLedsOff_callback() {
	tFlashLeds.setCallback(&tFlashLedsOn_callback);
	setPixels(COLOUR_OFF, 0);
	Serial.println("tFlashLedsOff_callback");
}

Task tSendControllerValues(SEND_TO_BOARD_INTERVAL_MS, TASK_FOREVER, &tSendControllerValues_callback);

void tSendControllerValues_callback() {
	if (esk8.sendThenReadPacket() == true) {
		lastPacketFromMaster = millis();
	}
	updateOled = true;
	debug.print(d_COMMUNICATION, "tSendControllerValues_callback(): batteryVoltage:%.1f \n", esk8.boardPacket.batteryVoltage);
}
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
	debug.print(d_COMMUNICATION, "Sending message: throttle:%d \n", esk8.controllerPacket.throttle);
}
//--------------------------------------------------------------
myPowerButtonManager powerButton(ENCODER_BUTTON_PIN, HIGH, 3000, 3000, powerupEvent);

void powerupEvent(int state) {

	switch (state) {
		case powerButton.TN_TO_POWERING_UP:
			setPixels(COLOUR_GREEN, 0);
			break;
		case powerButton.TN_TO_POWERED_UP_WAIT_RELEASE:
			setPixels(COLOUR_OFF, 0);
			// skip this and go straighht to RUNNING
			powerButton.setState(powerButton.TN_TO_RUNNING);
			break;
		case powerButton.TN_TO_RUNNING:
			setPixels(COLOUR_OFF, 0);
			break;
		case powerButton.TN_TO_POWERING_DOWN:
			zeroThrottleReadyToSend();
			setPixels(COLOUR_RED, 0);
			break;
		case powerButton.TN_TO_POWERING_DOWN_WAIT_RELEASE: {
				// u8g2.clearBuffer();
				// u8g2.sendBuffer();	// clear screen
				setPixels(COLOUR_OFF, 0);
				long pixelTime = millis();
				powerButton.setState(powerButton.TN_TO_POWER_OFF);
			}
			break;
		case powerButton.TN_TO_POWER_OFF:
			setPixels(COLOUR_OFF, 0);
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

	debug.init(d_DEBUG | d_STARTUP | d_COMMUNICATION);

	debug.print(d_STARTUP, "%s \n", compile_date);

	esk8.begin(&radio, role, radioNumber, &debug);

	FastLED.addLeds<NEOPIXEL, PIXEL_PIN>(leds, NUM_PIXELS);

	FastLED.show();

	runner.startNow();
	runner.addTask(tFlashLeds);
	runner.addTask(tSendControllerValues);
	tSendControllerValues.setInterval(esk8.getSendInterval());
	tSendControllerValues.enable();

	powerButton.begin(d_DEBUG);

	// encoder
	setupEncoder();
}

void loop() {

	deadmanSwitch.serviceEvents();

	powerButton.serviceButton();

	runner.execute();

	serviceCommsState();
	
	serviceOLED();

	delay(10);
}
//----------------------------------------------------------------
void setupEncoder() {

	pinMode(ENCODER_PIN_A, INPUT_PULLUP);
	pinMode(ENCODER_PIN_B, INPUT_PULLUP);

	attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), encoderInterruptHandler, CHANGE);
	attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), encoderInterruptHandler, CHANGE);
}
//--------------------------------------------------------------------------------
void serviceOLED() {

	if (powerButton.isRunning()) {

		if (updateOled == true) {
			updateOled = false;
			u8g2.clearBuffer();
			drawOnline(commsState == COMMS_ONLINE);
			drawBatteryVoltage(esk8.boardPacket.batteryVoltage);
			drawThrottle(esk8.controllerPacket.throttle, deadmanSwitch.isPressed());
			u8g2.sendBuffer();
		}
	}
}
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
		debug.print(d_DEBUG, "Setting commsState: COMMS_OFFLINE\n");
		// start leds flashing
		tFlashLedsColour = COLOUR_RED;
		setPixels(tFlashLedsColour, 0);
		tFlashLeds.enable();
	}
	else if (newState == COMMS_ONLINE) {
		debug.print(d_DEBUG, "Setting commsState: COMMS_ONLINE\n");
		commsState = COMMS_ONLINE;
		// stop leds flashing
		tFlashLeds.disable();
		setPixels(COLOUR_OFF, 0);
	}
}
//--------------------------------------------------------------------------------
void initOLED() {

	u8g2.begin();
	u8g2.setContrast(OLED_CONTRAST_LOW);
}

//--------------------------------------------------------------------------------
void setPixels(CRGB c, uint8_t wait) {
	for (uint16_t i=0; i<NUM_PIXELS; i++) {
		leds[i] = c;
		if (wait > 0) {
			FastLED.show();
			delay(wait);
		}
	}
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

void drawOnline(bool online) {
	if (online) {
		u8g2.drawXBMP(0, 0, ONLINE_SYMBOL_WIDTH, 10, wifiicon14x10);
	}
	else {
		u8g2.setDrawColor(0);
		u8g2.drawBox(0, 0, ONLINE_SYMBOL_WIDTH, 20);
		u8g2.setDrawColor(1);
	}
	// u8g2.sendBuffer();
}
//--------------------------------------------------------------------------------
void drawBatteryVoltage(float voltage) {
	u8g2.setCursor(ONLINE_SYMBOL_WIDTH+5, 10);
	u8g2.setFont(u8g2_font_helvR08_tf);
	u8g2.print(voltage, 1);
	u8g2.print("v");
}
//--------------------------------------------------------------------------------
void drawThrottle(int throttleValue, bool show) {

	if (show) {
		char val[4];
		itoa(throttleValue, val, 10);
		int strWidth = u8g2.getStrWidth(val);

	  	u8g2.setFont(u8g2_font_courB18_tf);
		u8g2.setCursor(0, 32);
		u8g2.print(val);
		//u8g2.drawStr(128-strWidth, 32, val);
	}
}
//--------------------------------------------------------------------------------
