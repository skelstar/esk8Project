#include <SPI.h>
#include <RF24.h> 
#include "WiFi.h"

#include <TaskScheduler.h>

#include <Wire.h>

#include <myPushButton.h>
#include <debugHelper.h>
#include <esk8Lib.h>
#include <EncoderModuleLib.h>
#include <JoystickModuleLib.h>

#include "i2cEncoderLib.h"

// #include <Adafruit_NeoPixel.h>
// #include <M5Stack.h>
#include "TFT_eSPI.h"
#include "Free_Fonts.h" 

//--------------------------------------------------------------------------------

#define	PIXEL_PIN			16	// was 5

#define DEADMAN_SWITCH		26

#define M5_BUTTON_A			39
#define M5_BUTTON_B			38
#define M5_BUTTON_C			37

//--------------------------------------------------------------

#define	STARTUP 		1 << 0
#define DEBUG 			1 << 1
#define COMMUNICATION 	1 << 2
#define HARDWARE		1 << 3
#define ONLINE_STATUS	1 << 5
#define TIMING			1 << 6

debugHelper debug;

esk8Lib esk8;


#define IWIDTH	100
#define IHEIGHT	100

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite img = TFT_eSprite(&tft);

//--------------------------------------------------------------

enum PeripheralConnected {
	ENCODER_MODULE,
	JOYSTICK_MODULE
} _peripheralType;

//--------------------------------------------------------------------------------

int32_t throttleValue;
int32_t oldThrottleValue = 0;
uint8_t encoder_status;
// stats
int32_t packetsSent = 0;
int32_t packetsFailed = 0;

//--------------------------------------------------------------

volatile long lastRxFromBoard = 0;
volatile bool rxDataFromBoard = false;
portMUX_TYPE mmux = portMUX_INITIALIZER_UNLOCKED;

//--------------------------------------------------------------

#define SPI_CE        5 	//33    // white/purple
#define SPI_CS        13	//26  // green

#define ROLE_CONTROLLER    0

int role = ROLE_CONTROLLER;
bool radioNumber = 1;

RF24 radio(SPI_CE, SPI_CS);    // ce pin, cs pin

int offlineCount = 0;
int encoderOfflineCount = 0;
bool encoderOnline = true;

//--------------------------------------------------------------------------------

const char compile_date[] = __DATE__ " " __TIME__;

//--------------------------------------------------------------

bool throttleChanged = false;
int throttle = 127;

#define SEND_TO_BOARD_INTERVAL_MS 			200

//--------------------------------------------------------------

void encoderChangedEvent(int encoderValue);
void joystickChangedEvent(int joystickValue);
void encoderPressedEventCallback();
bool canAccelerateCallback();
void encoderOnlineEvent(bool online);
//--------------------------------------------------------------
EncoderModuleLib encoderModule(
	&encoderChangedEvent, 
	&encoderPressedEventCallback,
	&encoderOnlineEvent,
	&canAccelerateCallback,
	 -20, 15);

void encoderChangedEvent(int newThrottleValue) {
	debug.print(DEBUG, "encoderChangedEvent(%d); \n", throttleValue);
	throttleChanged = true;

	throttleValue = newThrottleValue;
	setControllerPacketThrottle();
	updateDisplay(/* mode */ 1, /* backlight */ esk8.controllerPacket.throttle <= 127 );
}

void encoderPressedEventCallback() {
	throttleValue = 127;
	esk8.controllerPacket.throttle = throttleValue;
	encoderModule.setEncoderCount(0);

	throttleChanged = true;	
	debug.print(HARDWARE, "encoderPressedEventCallback(); \n");
}
//--------------------------------------------------------------
JoystickModuleLib joystickModule(
	&joystickChangedEvent, 
	&canAccelerateCallback);

void joystickChangedEvent(int joystickValue) {
	debug.print(DEBUG, "joystickChangedEvent(%d); \n", joystickValue);
	throttleChanged = true;

	throttleValue = joystickValue;
	setControllerPacketThrottle();
	updateDisplay(/* mode */ 1, /* backlight */ esk8.controllerPacket.throttle <= 127 );
}


bool canAccelerateCallback() {
	digitalWrite(DEADMAN_SWITCH, 1);
	return digitalRead(DEADMAN_SWITCH) == 0;
}

bool canAccelerate() {
	return digitalRead(DEADMAN_SWITCH) == 0;
}

void encoderOnlineEvent(bool online) {
	// encoderOnline = online;
	// updateDisplay(/* mode */ 1, /* backlight */ 1);
}

//--------------------------------------------------------------
void m5ButtonACallback(int eventCode, int eventPin, int eventParam);

#define PULLUP		true
#define OFFSTATE	LOW

myPushButton m5ButtonA(M5_BUTTON_A, PULLUP, /* offstate*/ HIGH, m5ButtonACallback);

void m5ButtonACallback(int eventCode, int eventPin, int eventParam) {
    
	switch (eventCode) {
		case m5ButtonA.EV_BUTTON_PRESSED:
			break;
		// case m5ButtonA.EV_RELEASED:
		// 	break;
		// case m5ButtonA.EV_DOUBLETAP:
		// 	break;
		// case m5ButtonA.EV_HELD_SECONDS:
		// 	break;
    }
}

//--------------------------------------------------------------

Scheduler runner;

bool tFlashLeds_onEnable();
void tFlashLeds_onDisable();
void tFlashLedsOn_callback();
void tFlashLedsOff_callback();

Task tFlashLeds(500, TASK_FOREVER, &tFlashLedsOff_callback);

bool tFlashLeds_onEnable() {
	//encoderModule.setPixel(ENCODER_MODULE_LED_COLOUR_RED);
	tFlashLeds.enable();
    return true;
}
void tFlashLeds_onDisable() {
	//encoderModule.setPixel(ENCODER_MODULE_LED_COLOUR_BLACK);
	tFlashLeds.disable();
}
void tFlashLedsOn_callback() {
	tFlashLeds.setCallback(&tFlashLedsOff_callback);
	//encoderModule.setPixel(ENCODER_MODULE_LED_COLOUR_RED);
	debug.print(HARDWARE, "tFlashLedsOn_callback\n");
	return;
}
void tFlashLedsOff_callback() {
	tFlashLeds.setCallback(&tFlashLedsOn_callback);
	//encoderModule.setPixel(ENCODER_MODULE_LED_COLOUR_BLACK);
	debug.print(HARDWARE, "tFlashLedsOff_callback\n");
	return;
}

//--------------------------------------------------------------
void tSendControllerValues_callback() {

	taskENTER_CRITICAL(&mmux);
	bool sentOK = sendToBoard();
    taskEXIT_CRITICAL(&mmux);

    if ( sentOK ) {
		throttleChanged = false;
		lastRxFromBoard = millis();
    }
    else {
        debug.print(COMMUNICATION, "tSendControllerValues_callback(): ERR_NOT_SEND_OK \n");
    }
}
Task tSendControllerValues(SEND_TO_BOARD_INTERVAL_MS, TASK_FOREVER, &tSendControllerValues_callback);

//--------------------------------------------------------------
void boardOfflineCallback() {
	debug.print(ONLINE_STATUS, "offlineCallback();\n");
	tFlashLeds.enable();
	offlineCount++;
	updateDisplay(/* mode */ 1, /* backlight */ 1);
	// M5.Speaker.tone(330, 100);	// tone 330, 200ms
}

void boardOnlineCallback() {
	debug.print(ONLINE_STATUS, "onlineCallback();\n");	
	tFlashLeds.disable();
	updateDisplay(/* mode */ 1, /* backlight */ 1);
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
void setControllerPacketThrottle() {

	bool accelerating = throttleValue > oldThrottleValue;
	bool decelerating = throttleValue < oldThrottleValue;

	esk8.controllerPacket.throttle = throttleValue;

	throttleChanged = accelerating || decelerating;
}
//--------------------------------------------------------------

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
	//debug.setFilter( STARTUP | COMMUNICATION | ONLINE_STATUS | TIMING );
	debug.setFilter( STARTUP | DEBUG | ONLINE_STATUS );
	//debug.setFilter( STARTUP );

	setupRadio();

	Wire.begin();

	tft.begin();

  	tft.setRotation(1);
	tft.fillScreen(TFT_BLACK);            // Clear screen

	//img.setColorDepth(8); // Optionally set depth to 8 to halve RAM use
	img.createSprite(IWIDTH, IHEIGHT);
	img.fillSprite(TFT_BLUE);
	img.setFreeFont(FF18);                 // Select the font
  	img.setTextDatum(MC_DATUM);
	img.setTextColor(TFT_YELLOW, TFT_BLACK);
	img.drawString("Ready!", 10, 20, 2);
	
	WiFi.mode( WIFI_OFF );	// WIFI_MODE_NULL
    btStop();   // turn bluetooth module off

	debug.print(STARTUP, "%s \n", compile_date);
    debug.print(STARTUP, "esk8Project/Controller.ino \n");

	throttleChanged = true;	// initialise

	esk8.begin(esk8.RF24_CONTROLLER);

	img.pushSprite(200, 100); delay(500);	

	runner.startNow();
	runner.addTask(tFlashLeds);
	runner.addTask(tSendControllerValues);
	tSendControllerValues.enable();

	boardCommsStatus.serviceState(false);

	pinMode(DEADMAN_SWITCH, INPUT_PULLUP);

	_peripheralType = getPeripheral();
	// peripheral

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

	bool connected = millis() - lastRxFromBoard < BOARD_OFFLINE_PERIOD;

	boardCommsStatus.serviceState(connected);

	runner.execute();

	if (_peripheralType == ENCODER_MODULE) {	
		encoderModule.update();
 	}
 	else if (_peripheralType == JOYSTICK_MODULE) {
 		joystickModule.update();
 	}

	if ( throttleChanged ) {
		tSendControllerValues.restart();
		throttleChanged = false;
	}

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

PeripheralConnected getPeripheral() {

	if (encoderModule.isConnected())	 {
		debug.print(STARTUP, " Encoder: Connected \n");
		return ENCODER_MODULE;
	} else if (joystickModule.isConnected()) {
		debug.print(STARTUP, " Joystick: Connected \n");
		joystickModule.begin(joystickModule.Y_AXIS);
		return JOYSTICK_MODULE;
	}
}

bool sendToBoard() {

	radio.stopListening();

	bool sentOK = radio.write( &esk8.controllerPacket, sizeof(esk8.controllerPacket) );

	radio.startListening();

    bool timeout = false;                    
    // wait until response has arrived
    if ( !radio.available() ){                             // While nothing is received
		debug.print(COMMUNICATION, "\n NO_ACK");
	}
    else {
    	while ( radio.available() ) {
			radio.read( &esk8.boardPacket, sizeof(esk8.boardPacket) );
		}

		debug.print(COMMUNICATION, "%d %s \n",
			esk8.controllerPacket.throttle,
			canAccelerateCallback() == true ? "d" : ".");
    }
	
    return sentOK;
}
//--------------------------------------------------------------
void setupRadio() {
    radio.begin();
    radio.setPALevel(RF24_PA_MIN);
    radio.setAutoAck(1);                    // Ensure autoACK is enabled
  	radio.enableAckPayload();               // Allow optional ack payloads

  	radio.openWritingPipe( esk8.listening_pipes[esk8.RF24_BOARD] );
  	radio.openReadingPipe( 1, esk8.talking_pipes[esk8.RF24_BOARD] );

    radio.printDetails();
}
//--------------------------------------------------------------

void updateDisplay(int mode, int backlight) {
	#define LINE_1 20
	#define LINE_2 45
	#define LINE_3 70
	#define LINE_4 95

	if ( backlight == false ) {
		digitalWrite(TFT_BL, LOW);
		return;
	}

	switch (mode) {
		case 0:
			break;
		case 1:
		  	img.setTextDatum(MC_DATUM);
			img.setTextSize(1);
			img.fillScreen(TFT_DARKGREEN);
			img.setTextColor(TFT_YELLOW, TFT_BLACK);
			img.drawNumber( esk8.controllerPacket.throttle,  10, 10);		
			img.pushSprite(20, 20); delay(0);	

			digitalWrite(TFT_BL, HIGH);	// turn backlight off?

			break;
	}
}