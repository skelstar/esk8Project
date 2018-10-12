#include <SPI.h>
#include "RF24.h"
#include <Rotary.h>

#include <TaskScheduler.h>

#include <myPushButton.h>
#include <myPowerButtonManager.h>
#include <debugHelper.h>
#include <esk8Lib.h>

// #include <U8g2lib.h>
#include <Adafruit_NeoPixel.h>
#include <M5Stack.h>


// #include "VescUart.h"
//--------------------------------------------------------------------------------

#define ENCODER_BUTTON_PIN	26	// 34	// 36 didn't work
#define ENCODER_PIN_A		34	// 16
#define ENCODER_PIN_B		17	// 4

#define DEADMAN_SWITCH_PIN	25

#define	PIXEL_PIN			16	// was 5


#define M5_BUTTON_A			39
#define M5_BUTTON_B			38
#define M5_BUTTON_C			37


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

volatile long lastRxFromBoard = 0;
volatile bool rxDataFromBoard = false;
portMUX_TYPE mmux = portMUX_INITIALIZER_UNLOCKED;

//--------------------------------------------------------------

//U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);   // Adafruit Feather ESP8266/32u4 Boards + FeatherWing OLED

//--------------------------------------------------------------

// #define SPI_MOSI        23 // Blue
// #define SPI_MISO        19 // Orange
// #define SPI_CLK			18 // Yellow
#define SPI_CE          5 //33    // white/purple
#define SPI_CS          13	//26  // green

#define ROLE_BOARD        1
#define ROLE_CONTROLLER    0

int role = ROLE_CONTROLLER;
bool radioNumber = 1;

RF24 radio(SPI_CE, SPI_CS);    // ce pin, cs pin

//--------------------------------------------------------------------------------
#define BRIGHTNESS	20

#define 	NUM_PIXELS 		8

// CRGB leds[NUM_PIXELS];

// CRGB COLOUR_OFF = CRGB::Black;
// CRGB COLOUR_RED = CRGB::Red;
// CRGB COLOUR_GREEN = CRGB::Green;
// CRGB COLOUR_BLUE = CRGB::Blue;
// CRGB COLOUR_PINK = CRGB::Pink;
// CRGB COLOUR_ACCELERATING = CRGB::Navy;
// CRGB COLOUR_BRAKING = CRGB::Crimson;
// CRGB COLOUR_THROTTLE_IDLE = CRGB::Green;
// CRGB COLOUR_WHITE = CRGB::White;

Adafruit_NeoPixel leds = Adafruit_NeoPixel(NUM_PIXELS, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

uint32_t COLOUR_OFF = leds.Color(0, 0, 0);
uint32_t COLOUR_RED = leds.Color(255, 0, 0);
uint32_t COLOUR_GREEN = leds.Color(0, 255, 0);
uint32_t COLOUR_BLUE = leds.Color(0, 0, 255);
uint32_t COLOUR_PINK = leds.Color(128, 0, 100);

uint32_t COLOUR_ACCELERATING = COLOUR_BLUE;
uint32_t COLOUR_BRAKING = COLOUR_PINK;
uint32_t COLOUR_THROTTLE_IDLE = COLOUR_GREEN;

//--------------------------------------------------------------

const char compile_date[] = __DATE__ " " __TIME__;

int mapEncoderToThrottleValue(int raw);
void setupEncoder();
void setPixels(uint32_t c) ;
void zeroThrottleReadyToSend();

//--------------------------------------------------------------

int powerButtonIsPressed();
void powerupEvent(int state);

myPowerButtonManager powerButton(DEADMAN_SWITCH_PIN, HIGH, 3000, 3000, powerupEvent, powerButtonIsPressed);

int powerButtonIsPressed() {
	return digitalRead(DEADMAN_SWITCH_PIN) == 0 
		&& digitalRead(ENCODER_BUTTON_PIN) == 0;
}

void powerupEvent(int state) {

	switch (state) {
		case powerButton.TN_TO_POWERING_UP:
			setPixels(COLOUR_OFF);
			debug.print(STARTUP, "TN_TO_POWERING_UP \n");
			break;
		case powerButton.TN_TO_POWERED_UP_WAIT_RELEASE:
			setPixels(COLOUR_OFF);
			oledMessage("Ready...");
			debug.print(STARTUP, "TN_TO_POWERED_UP_WAIT_RELEASE \n");
			break;
		case powerButton.TN_TO_RUNNING:
			debug.print(STARTUP, "TN_TO_RUNNING \n");
			setPixels(COLOUR_OFF);
			//u8g2.clearBuffer();
			//u8g2.sendBuffer();
			break;
		case powerButton.TN_TO_POWERING_DOWN:
			setPixels(COLOUR_RED);
			oled2LineMessage("Powering", "Down", "");
			debug.print(STARTUP, "TN_TO_POWERING_DOWN \n");
			break;
		case powerButton.TN_TO_POWERING_DOWN_WAIT_RELEASE:
			debug.print(STARTUP, "TN_TO_POWERING_DOWN_WAIT_RELEASE \n");
			//u8g2.clearBuffer();
			//u8g2.sendBuffer();
			break;
		case powerButton.TN_TO_POWER_OFF:
			radio.powerDown();
			setPixels(COLOUR_OFF);
			//u8g2.clearBuffer();
			//u8g2.sendBuffer();
			debug.print(STARTUP, "TN_TO_POWER_OFF \n");
			delay(100);
			esp_deep_sleep_start();
			Serial.println("This will never be printed");
			break;
	}
}

//--------------------------------------------------------------

bool throttleChanged = false;
int throttle = 127;

#define SEND_TO_BOARD_INTERVAL_MS 			400

//--------------------------------------------------------------

Rotary rotary = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);

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
#define ENCODER_COUNTER_MIN		-20 	// decceleration (ie -20 divides 0-127 into 20)
#define ENCODER_COUNTER_MAX		15 		// acceleration (ie 15 divides 127-255 into 15)

int encoderCounter = 0;

//--------------------------------------------------------------------------------

Scheduler runner;

bool tFlashLeds_onEnable();
void tFlashLeds_onDisable();
void tFlashLedsOn_callback();
void tFlashLedsOff_callback();

Task tFlashLeds(500, TASK_FOREVER, &tFlashLedsOff_callback);

bool tFlashLeds_onEnable() {
	// setPixels(COLOUR_RED);
	M5.Lcd.fillScreen(RED);
	tFlashLeds.enable();
    return true;
}
void tFlashLeds_onDisable() {
	// setPixels(COLOUR_OFF);
	M5.Lcd.fillScreen(BLACK);
	tFlashLeds.disable();
}
void tFlashLedsOn_callback() {
	tFlashLeds.setCallback(&tFlashLedsOff_callback);
	debug.print(HARDWARE, "tFlashLedsOn_callback\n");
	// setPixels(COLOUR_RED);
	M5.Lcd.fillScreen(RED);
	return;
}
void tFlashLedsOff_callback() {
	tFlashLeds.setCallback(&tFlashLedsOn_callback);
	debug.print(HARDWARE, "tFlashLedsOff_callback\n");
	// setPixels(COLOUR_OFF);
	M5.Lcd.fillScreen(BLACK);
	return;
}
//--------------------------------------------------------------
void sendMessage() {

	taskENTER_CRITICAL(&mmux);
    int result = esk8.sendThenReadACK();
    taskEXIT_CRITICAL(&mmux);

    if (result == esk8.CODE_SUCCESS) {
		throttleChanged = false;
		lastRxFromBoard = millis();
		rxDataFromBoard = true;
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
}

void tSendControllerValues_callback() {
    sendMessage();
}
Task tSendControllerValues(SEND_TO_BOARD_INTERVAL_MS, TASK_FOREVER, &tSendControllerValues_callback);

//--------------------------------------------------------------
void boardOfflineCallback() {
	debug.print(ONLINE_STATUS, "offlineCallback();\n");
	tFlashLeds.enable();
	oledMessage("OFFLINE");
}

void boardOnlineCallback() {
	debug.print(ONLINE_STATUS, "onlineCallback();\n");	
	tFlashLeds.disable();
	//u8g2.clearBuffer();
	//u8g2.sendBuffer();
}
//--------------------------------------------------------------
#define	TN_ONLINE 	1
#define	ST_ONLINE 	2
#define	TN_OFFLINE  3
#define	ST_OFFLINE  4

class OnlineStatus 
{
	typedef void ( *StatusCallback )();

	private:
		uint8_t state = ST_ONLINE;
		uint8_t oldstate = ST_ONLINE;

		StatusCallback _offlineCallback;
		StatusCallback _onlineCallback;

	public:

		OnlineStatus(StatusCallback offlineCallback, StatusCallback onlineCallback) {
			_offlineCallback = offlineCallback;
			_onlineCallback = onlineCallback;
			state = ST_ONLINE;
		}

		bool serviceState(bool online) {
			switch (state) {
				case TN_ONLINE:
					state = online ? ST_ONLINE : TN_OFFLINE;
					_onlineCallback();
					break;
				case ST_ONLINE:
					state = online ? ST_ONLINE : TN_OFFLINE;
					break;
				case TN_OFFLINE:
					state = online ? TN_ONLINE : ST_OFFLINE;
					_offlineCallback();
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

OnlineStatus boardCommsStatus(&boardOfflineCallback, &boardOnlineCallback);

//------------------------------------------------------------------------------

void encoderInterruptHandler() {

	unsigned char result = rotary.process();

	bool canAccelerate = deadmanSwitch.isPressed();

	if (result == DIR_CCW && (canAccelerate || encoderCounter < 0)) {
		if (encoderCounter < ENCODER_COUNTER_MAX) {
			encoderCounter++;
			esk8.controllerPacket.throttle = mapEncoderToThrottleValue(encoderCounter);
			debug.print(HARDWARE, "encoder: %d throttle: %d \n", encoderCounter, esk8.controllerPacket.throttle);
			throttleChanged = true;
		}
	}
	else if (result == DIR_CW) {
		if (encoderCounter > ENCODER_COUNTER_MIN) {
			encoderCounter--;
			esk8.controllerPacket.throttle = mapEncoderToThrottleValue(encoderCounter);
			debug.print(HARDWARE, "encoder: %d throttle: %d \n", encoderCounter, esk8.controllerPacket.throttle);
			throttleChanged = true;
		}
	}
}

void m5ButtonInterruptHandler() {
	int m5Astate = digitalRead(M5_BUTTON_A);
	if (m5Astate == 0 && encoderCounter == 0) {
		encoderCounter++;
		esk8.controllerPacket.throttle = mapEncoderToThrottleValue(encoderCounter);
		debug.print(HARDWARE, "encoder: %d throttle: %d \n", encoderCounter, esk8.controllerPacket.throttle);
		throttleChanged = true;	}
	else if (m5Astate == 1 && encoderCounter > 0) {
		encoderCounter--;
		esk8.controllerPacket.throttle = mapEncoderToThrottleValue(encoderCounter);
		debug.print(HARDWARE, "encoder: %d throttle: %d \n", encoderCounter, esk8.controllerPacket.throttle);
		throttleChanged = true;	
	} else {
		debug.print(HARDWARE, "void m5ButtonInterruptHandler() %d %d {\n", encoderCounter, m5Astate);
	}
}

//--------------------------------------------------------------

volatile uint32_t otherNode;
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
	debug.addOption(ONLINE_STATUS, "ONLINE_STATUS");
	debug.addOption(TIMING, "TIMING");
	debug.setFilter( STARTUP | HARDWARE | DEBUG | ONLINE_STATUS | TIMING );
	//debug.setFilter( STARTUP );

  	leds.setBrightness(BRIGHTNESS);
	leds.begin();
	leds.show();

	// initialize the M5Stack object
	M5.begin(true, false, false); //lcd, sd, serial

	//text print
	M5.Lcd.fillScreen(BLACK);
	M5.Lcd.setCursor(10, 10);
	M5.Lcd.setTextColor(WHITE);
	M5.Lcd.setTextSize(1);
	M5.Lcd.printf("Display Test!");

	powerButton.begin();

	debug.print(STARTUP, "%s \n", compile_date);
    debug.print(STARTUP, "esk8Project/Controller.ino \n");

	throttleChanged = true;	// initialise

    radio.begin();

    btStop();   // turn bluetooth module off

	// esk8.begin(&radio, ROLE_CONTROLLER, radioNumber, &debug);
	esk8.begin(&radio, ROLE_CONTROLLER, radioNumber);

	//u8g2.begin();

	sendMessage();

	runner.startNow();
 	// runner.addTask(tFastFlash);
	runner.addTask(tFlashLeds);
	runner.addTask(tSendControllerValues);
	tSendControllerValues.enable();

	boardCommsStatus.serviceState(false);

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
long nowms = 0;
long connectedNow = 0;
bool oldConnected = false;
#define BOARD_OFFLINE_PERIOD	1000

void loop() {

	bool connected = millis() - lastRxFromBoard < BOARD_OFFLINE_PERIOD;

	boardCommsStatus.serviceState(connected);

	if (powerButton.getState() != powerButton.STATE_RUNNING) {
		// catch
	}
	else if (throttleChanged && connected) {
		tSendControllerValues.restart();
	}
	else if (esk8.controllerPacket.throttle == 127 && rxDataFromBoard) {
		rxDataFromBoard = false;
		char buf[100];
		sprintf(buf, "%0.1f", esk8.boardPacket.batteryVoltage);
		oled2LineMessage("BATT. VOLTS", buf, "V");
		// setPixels(COLOUR_OFF);
	}
	else if (esk8.controllerPacket.throttle != 127) {
		//u8g2.clearBuffer();
		//u8g2.sendBuffer();
	}

	powerButton.serviceButton();

	runner.execute();

	delay(10);
}
/**************************************************************
					TASK 0
**************************************************************/
void codeForEncoderTask( void *parameter ) {

	setupEncoder();

	setupM5Button();

	// then loop forever	
	for (;;) {

		encoderButton.serviceEvents();

		deadmanSwitch.serviceEvents();

		delay(10);
	}

	vTaskDelete(NULL);
}
//**************************************************************
void setupM5Button() {
	pinMode(M5_BUTTON_A, INPUT_PULLUP);
	attachInterrupt(digitalPinToInterrupt(M5_BUTTON_A), m5ButtonInterruptHandler, CHANGE);
}

void setupEncoder() {

	pinMode(ENCODER_PIN_A, INPUT);
	pinMode(ENCODER_PIN_B, INPUT);

	attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), encoderInterruptHandler, CHANGE);
	attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), encoderInterruptHandler, CHANGE);
}
//--------------------------------------------------------------------------------
void setPixels(uint32_t c) {
	taskENTER_CRITICAL(&mmux);
	for (uint16_t i=0; i<NUM_PIXELS; i++) {
		leds.setPixelColor(i, c);
	}
	leds.show();
	taskEXIT_CRITICAL(&mmux);
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
	esk8.controllerPacket.throttle = mapEncoderToThrottleValue(encoderCounter);
	throttle = 127;
	throttleChanged = true;
    debug.print(HARDWARE, "encoderCounter: %d, throttle: %d [ZERO] \n", encoderCounter, throttle);
}
//--------------------------------------------------------------
void oled2LineMessage(char* line1, char* line2, char* unit) {
	
	//u8g2.clearBuffer();
	// line1
	//u8g2.setFont(u8g2_font_helvR08_tf);	// u8g2_font_courR08_tf
	//u8g2.drawStr(0, 10, line1);
	// unit
	//u8g2_uint_t unitWidth = //u8g2.getStrWidth((const char*) unit);
	//u8g2.setFont(u8g2_font_helvR08_tf);	// u8g2_font_courR08_tf u8g2_font_helvR08_tf
	//u8g2.drawStr(128-unitWidth, 32, unit);
	//line2
	//u8g2.setFont(u8g2_font_courB18_tf);	// u8g2_font_courB18_tf u8g2_font_courB24_tf
	//u8g2_uint_t width = //u8g2.getStrWidth((const char*) line2);
	// int unitOffset = unitWidth + 2;
	// if (strcmp(unit, "") == 0) {
	// 	unitOffset = 0;
	// }
	//u8g2.drawStr(128-width-unitOffset, 32, line2);

	//u8g2.sendBuffer();
}
//--------------------------------------------------------------
void oledMessage(char* line1) {
	
	//u8g2.clearBuffer();
	// line1
	//u8g2.setFont(u8g2_font_courB18_tf);	// u8g2_font_courB18_tf u8g2_font_courB24_tf
	//u8g2_uint_t width = //u8g2.getStrWidth((const char*) line1);
	//u8g2.drawStr(64-(width/2), (32/2)+(18/2), line1);
	//u8g2.sendBuffer();
}
//--------------------------------------------------------------
