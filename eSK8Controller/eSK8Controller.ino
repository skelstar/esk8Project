
#include <SPI.h>
#include "RF24.h"
#include <esk8Lib.h>
#include "esk8ControllerDatatypes.h"
#include <Rotary.h>
#include <U8g2lib.h>

#include <myPushButton.h>
#include <myPowerButtonManager.h>
#include <stdlib.h>

#include <FastLED.h>
#include <TaskScheduler.h>

#include <ESP8266VESC.h>
#include "VescUart.h"
#include "datatypes.h"
//--------------------------------------------------------------------------------
// https://sandhansblog.wordpress.com/2017/04/16/interfacing-displaying-a-custom-graphic-on-an-0-96-i2c-oled/
static const unsigned char wifiicon14x10 [] PROGMEM = {
   0xf0, 0x03, 0xfc, 0x0f, 0x1e, 0x1c, 0xc7, 0x39, 0xf3, 0x33, 0x38, 0x06, 0x1c, 0x0c, 0xc8, 0x04, 0xe0, 0x01, 0xc0, 0x00 
};

//--------------------------------------------------------------------------------

const char compile_date[] = __DATE__ " " __TIME__;

//--------------------------------------------------------------------------------

#define OLED_PWR			27
//#define OLED_GND
#define OLED_SCL        	22    // std ESP32 (22)
#define OLED_SDA        	21    // std ESP32 (23??)

#define ENCODER_BUTTON_PIN		34
#define ENCODER_PIN_A		4
#define ENCODER_PIN_B		16

#define TEST_PIN			12

#define POWER_UP_BUTTON		ENCODER_BUTTON_PIN
	
#define DEADMAN_SWITCH_PIN	25

#define	PIXEL_PIN			5
	
#define SPI_CE				33	// white/purple
#define SPI_CS				26  // green

//--------------------------------------------------------------------------------
bool radioNumber = 1;

/* Hardware configuration: Set up nRF24L01 radio on SPI bus plus pins 7 & 8 */
RF24 radio(SPI_CE, SPI_CS);	// ce pin, cs pin

#define 	ROLE_MASTER		1
#define 	ROLE_SLAVE		0
//--------------------------------------------------------------------------------

esk8Lib esk8;

long lastPacketFromMaster = 0;

// if this is set to 100 then goes offline every 3 seconds (because that's when VESC comms to myVESC)
#define 	COMMS_TIMEOUT_PERIOD	200 

//--------------------------------------------------------------------------------

#define 	NUM_PIXELS 		8

CRGB leds[NUM_PIXELS];

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

//--------------------------------------------------------------------------------

#define     OLED_CONTRAST_HIGH	100        // 256 highest
#define     OLED_CONTRAST_LOW	20
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, OLED_SCL, OLED_SDA, U8X8_PIN_NONE);

//--------------------------------------------------------------------------------

#define 	OFF_STATE_HIGH		HIGH
#define 	OFF_STATE_LOW       0
#define 	PULLUP              true

#define 	HELD_DOWN_POWER_OFF_SEC		3
#define 	HELD_DOWN_POWER_UP_SEC		3

//--------------------------------------------------------------------------------

volatile CommsStateType commsState = COMMS_UNKNOWN_STATE;

//--------------------------------------------------------------------------------

void listener_dialButton( int eventCode, int eventPin, int eventParam );
myPushButton dialButton(ENCODER_BUTTON_PIN, PULLUP, OFF_STATE_HIGH, listener_dialButton);
void listener_dialButton( int eventCode, int eventPin, int eventParam ) {

    switch (eventCode) {
        case dialButton.EV_BUTTON_PRESSED:
			zeroThrottle();
            Serial.println("EV_BUTTON_PRESSED (DIAL)");
            break;
        case dialButton.EV_RELEASED:
			zeroThrottle();
			Serial.println("EV_BUTTON_RELEASED (DIAL)");
            break;
		case dialButton.EV_HELD_SECONDS:
            Serial.printf("EV_BUTTON_HELD (DIAL): %d \n", eventParam);
            break;
    }
}

void listener_deadmanSwitch( int eventCode, int eventPin, int eventParam );
myPushButton deadmanSwitch(DEADMAN_SWITCH_PIN, PULLUP, OFF_STATE_HIGH, listener_deadmanSwitch);
void listener_deadmanSwitch( int eventCode, int eventPin, int eventParam ) {

	switch (eventCode) {

		case deadmanSwitch.EV_BUTTON_PRESSED:
			if (esk8.slavePacket.throttle > 127) {
				zeroThrottle();
			}
			Serial.println("EV_BUTTON_PRESSED (DEADMAN)");
			break;
		
		case deadmanSwitch.EV_RELEASED:
			if (esk8.slavePacket.throttle > 127) {
				zeroThrottle();
			}
			Serial.println("EV_BUTTON_RELEASED (DEADMAN)");
			break;
		
		case deadmanSwitch.EV_HELD_SECONDS:
			//Serial.printf("EV_BUTTON_HELD (DEADMAN): %d \n", eventParam);
			break;
	}
}
//--------------------------------------------------------------------------------

myPowerButtonManager powerButton(ENCODER_BUTTON_PIN, HIGH, 3000, 3000, powerupEvent);

	// TR_POWERING_UP = 0,
	// ST_POWERING_UP,
	// TR_POWERED_UP_WAIT_RELEASE,
	// ST_POWERED_UP_WAIT_RELEASE,
	// TR_RUNNING,
	// ST_RUNNING,
	// TR_POWERING_DOWN,
	// ST_POWERING_DOWN,
	// TR_POWERING_DOWN_WAIT_RELEASE,
	// ST_POWERING_DOWN_WAIT_RELEASE,
	// TR_POWER_OFF
	// ST_POWER_OFF




void powerupEvent(int state) {

	switch (state) {
		case powerButton.TR_POWERING_UP:
			message("Powering Up");
			break;
		case powerButton.TR_POWERED_UP_WAIT_RELEASE:
			// skip this and go straighht to RUNNING
			powerButton.setState(powerButton.TR_RUNNING);
			break;
		case powerButton.TR_RUNNING:
			break;
		case powerButton.ST_RUNNING:
			break;
		case powerButton.TR_POWERING_DOWN:
			message("Powering Down");
			break;
		case powerButton.TR_POWERING_DOWN_WAIT_RELEASE:
			u8g2.clearBuffer();
			u8g2.sendBuffer();	// clear screen
			setPixels(COLOUR_OFF, 0);
			powerButton.setState(powerButton.TR_POWER_OFF);
			break;
		case powerButton.TR_POWER_OFF:
			delay(100);
			esp_deep_sleep_start();
			Serial.println("This will never be printed");
			break;
	}
}

//--------------------------------------------------------------------------------

Rotary rotary = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);

// lower number = more coarse
#define ENCODER_COUNTER_MIN	-18 	// decceleration (ie -20 divides 0-127 into 20)
#define ENCODER_COUNTER_MAX	12 		// acceleration (ie 15 divides 127-255 into 15)

#define DEADMAN_SWITCH_USED		1

int encoderCounter = 0;
bool encoderChanged = false;

void encoderInterruptHandler() {
	unsigned char result = rotary.process();

	bool canAccelerate = deadmanSwitch.isPressed() || DEADMAN_SWITCH_USED == 0;

	if (result == DIR_CW && (canAccelerate || encoderCounter < 0)) {
		if (encoderCounter < ENCODER_COUNTER_MAX) {
			encoderCounter++;

			int throttleValue = getThrottleValue();
			esk8.updateSlavePacket(throttleValue);
			Serial.println(throttleValue);
		}
	}
	else if (result == DIR_CCW) {
		if (encoderCounter > ENCODER_COUNTER_MIN) {
			encoderCounter--;

			int throttleValue = getThrottleValue();
			esk8.updateSlavePacket(throttleValue);
			Serial.println(throttleValue);
		}
	}
}
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void setup() {

	Serial.begin(9600);

	Serial.println(compile_date);

	initOLED();

	FastLED.addLeds<NEOPIXEL, PIXEL_PIN>(leds, NUM_PIXELS);

	powerButton.begin();

	while (powerButton.isRunning() == false) {
		powerButton.serviceButton(true);
		serviceLedRing();
		delay(50);
	}

	serviceLedRing();

	runner.init();
	runner.addTask(tFlashLeds);

	radio.begin();

	esk8.begin(&radio, ROLE_SLAVE, radioNumber);

	// encoder
	setupEncoder();

	FastLED.show();

	serviceLedRing();
}
//--------------------------------------------------------------------------------
void loop() {

	deadmanSwitch.serviceEvents();
	dialButton.serviceEvents();

	powerButton.serviceButton(true);

	if (powerButton.isRunning()) {

		int result = esk8.pollMasterForPacket();	// || digitalRead(TEST_PIN) == 0;
		if (result == true) {
			lastPacketFromMaster = millis();
			// response from Master
			Serial.printf("Master battery: %d \n", esk8.masterPacket.batteryVoltage);
		}
		else {
		}
	}
	else {
	}

	runner.execute();

	serviceCommsState();

	serviceOLED();

	serviceLedRing();

	delay(50);
}
//--------------------------------------------------------------------------------
void serviceOLED() {

	if (powerButton.isRunning()) {
		u8g2.clearBuffer();
		drawOnline(commsState == COMMS_ONLINE);
		drawBatteryVoltage(esk8.masterPacket.batteryVoltage);
		drawThrottle(esk8.slavePacket.throttle, deadmanSwitch.isPressed());
		u8g2.sendBuffer();
	}
}
//--------------------------------------------------------------------------------
void initOLED() {

	pinMode(OLED_PWR, OUTPUT);
	digitalWrite(OLED_PWR, HIGH);
	delay(100);	// settle

	u8g2.begin();
	u8g2.setContrast(OLED_CONTRAST_LOW);
}
//--------------------------------------------------------------------------------
void serviceCommsState() {

	bool timedOut = millis() - lastPacketFromMaster > COMMS_TIMEOUT_PERIOD;

	if (commsState == COMMS_ONLINE && timedOut) {
		setCommsState(COMMS_OFFLINE);
	}
	else if (commsState == COMMS_OFFLINE && !timedOut) {
		setCommsState(COMMS_ONLINE);
	}
	else if (commsState == COMMS_UNKNOWN_STATE) {
		setCommsState(timedOut ? COMMS_OFFLINE : COMMS_ONLINE);
	}
}
//--------------------------------------------------------------------------------
void setCommsState(CommsStateType newState) {
	if (newState == COMMS_OFFLINE) {
		commsState = COMMS_OFFLINE;
		Serial.println("Setting commsState: COMMS_OFFLINE");
		// start leds flashing
		tFlashLedsColour = COLOUR_RED;
		setPixels(tFlashLedsColour, 0);
		tFlashLeds.enable();
	}
	else if (newState == COMMS_ONLINE) {
		Serial.println("Setting commsState: COMMS_ONLINE");
		commsState = COMMS_ONLINE;
		// stop leds flashing
		tFlashLeds.disable();
		setPixels(COLOUR_OFF, 0);
	}
}
//--------------------------------------------------------------------------------
void zeroThrottle() {
	encoderCounter = 0;
	int throttleValue = getThrottleValue();
	esk8.updateSlavePacket(throttleValue);
	//updateLcdThrottle(throttleValue);
	Serial.println(throttleValue);
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
void serviceLedRing() {

	switch (powerButton.getState()) {

		case powerButton.ST_POWERING_UP:
		case powerButton.ST_POWERED_UP_WAIT_RELEASE:
		case powerButton.TR_RUNNING:
		case powerButton.ST_POWERING_DOWN:
		case powerButton.ST_POWERING_DOWN_WAIT_RELEASE:
		case powerButton.ST_POWER_OFF:
			setPixels(COLOUR_GREEN, 0);
			break;

		case powerButton.ST_RUNNING:
			if (commsState == COMMS_ONLINE) {

			}
			break;
	}
}
//--------------------------------------------------------------------------------
int getThrottleValue() {
	int mappedThrottle = 0;

	if (encoderCounter >= 0) {
		mappedThrottle = map(encoderCounter, 0, ENCODER_COUNTER_MAX, 127, 255);
	}
	else {
		mappedThrottle = map(encoderCounter, ENCODER_COUNTER_MIN, 0, 0, 127);
	}

	return mappedThrottle;
}
//--------------------------------------------------------------------------------
void setupEncoder() {

	pinMode(ENCODER_PIN_A, INPUT_PULLUP);
	pinMode(ENCODER_PIN_B, INPUT_PULLUP);

	attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), encoderInterruptHandler, CHANGE);
	attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), encoderInterruptHandler, CHANGE);

	esk8.slavePacket.throttle = getThrottleValue();
}
//--------------------------------------------------------------------------------
void message(char* text) {

	u8g2.clearBuffer();					// clear the internal memory

	// reduce font size if long string
	u8g2_uint_t width = 0;
	int len = strlen(text);
	if (len <= 8) {
		u8g2.setFont(u8g2_font_courB18_tf);
	}
	else {
		u8g2.setFont(u8g2_font_helvB12_tf);	
	}

	width = u8g2.getStrWidth((const char*) text);

	int startX = (128/2)-(width/2);
	int startY = 32-5;
	u8g2.drawStr(startX, startY, (const char*) text);

	u8g2.sendBuffer();
}
//--------------------------------------------------------------------------------
void message(char* line1_text, char* line2_text) {
	u8g2.clearBuffer();					// clear the internal memory

	oledSmallBattery(95, 0, 0);

	if (line1_text != NULL) {
	 	u8g2.setFont(u8g2_font_helvR08_tf);
	 	u8g2_uint_t width = u8g2.getStrWidth((const char*) line1_text);
	 	int startX = 128-width;
	 	u8g2.drawStr(startX, 8, (const char*) line1_text);
	}
	if (line2_text != NULL) {
	 	u8g2.setFont(u8g2_font_courB18_tf);
	 	u8g2.drawStr(0, 32, (const char*) line1_text);
	}
	u8g2.sendBuffer();
}
//--------------------------------------------------------------------------------
#define 	BATTERY_WIDTH	20
#define 	BATTERY_HEIGHT	8
//--------------------------------------------------------------------------------
void oledSmallBattery(int percent, int x, int y) {
	int width = BATTERY_WIDTH;
	int height = BATTERY_HEIGHT;
	
	// outside box (solid)
	u8g2.setDrawColor(1);
	u8g2.drawBox(x+2, y, width, height); 
	// capacity
	u8g2.setDrawColor(0); 
	int cap = ((percent/100.0) * (width-2));
	int notcap = width - cap;
	u8g2.drawBox(x+2+1, y+1, notcap, height-2);
	// nipple
	u8g2.setDrawColor(1);
	u8g2.drawBox(x, y+2, 2, height - (2*2));
}

#define ONLINE_SYMBOL_WIDTH 	14
//--------------------------------------------------------------------------------
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
