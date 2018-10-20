#include <SPI.h>
#include "RF24.h"
#include "WiFi.h"

#include <TaskScheduler.h>

#include <Wire.h>
#include "i2cEncoderLib.h"
// https://github.com/Fattoresaimon/i2cencoder/tree/master/Arduino%20Library

#include <myPushButton.h>
#include <debugHelper.h>
#include <esk8Lib.h>

// #include <U8g2lib.h>
#include <Adafruit_NeoPixel.h>
#include <M5Stack.h>
//--------------------------------------------------------------------------------

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

#define 	ENCODER_MODULE_ADDR		0x4

//--------------------------------------------------------------------------------

// lower number = more coarse
#define ENCODER_COUNTER_MIN	-20 	// decceleration (ie -20 divides 0-127 into 20)
#define ENCODER_COUNTER_MAX	 15 	// acceleration (ie 15 divides 127-255 into 15)

int32_t encoderCounter;
int32_t oldEncoderCounter = 0;
uint8_t encoder_status;

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

int offlineCount = 0;
int encoderOfflineCount = 0;
bool encoderOnline = true;

//--------------------------------------------------------------------------------

const char compile_date[] = __DATE__ " " __TIME__;

int mapEncoderToThrottleValue(int raw);
void zeroThrottleReadyToSend();

//--------------------------------------------------------------

bool throttleChanged = false;
int throttle = 127;

#define SEND_TO_BOARD_INTERVAL_MS 			200

//--------------------------------------------------------------

#define ENCODER_MODULE_LED_CMD	1
#define ENCODER_MODULE_LED_COLOUR_BLACK	0
#define ENCODER_MODULE_LED_COLOUR_RED	1
#define ENCODER_MODULE_LED_COLOUR_BLUE	2
#define ENCODER_MODULE_LED_COLOUR_GREEN	3

class EncoderModule 
{
	typedef void ( *EncoderChangedEventCallback )( int value );
	typedef void ( *EncoderOnlineEventCallback )( bool online );

	private:
		int encoderCounterState = 0;
		bool encoderOnline = true;

		EncoderChangedEventCallback _encoderChangedEventCallback;
		EncoderOnlineEventCallback _encoderOnlineEventCallback;

	public:

		EncoderModule(EncoderChangedEventCallback encoderChangedEventCallback,
			EncoderOnlineEventCallback encoderOnlineEventCallback) 
		{
			_encoderChangedEventCallback = encoderChangedEventCallback;
			_encoderOnlineEventCallback = encoderOnlineEventCallback;
		}

		void setPixel(byte encoderLedColour) {
			Wire.beginTransmission(ENCODER_MODULE_ADDR);
			Wire.write(ENCODER_MODULE_LED_CMD);
			Wire.write(encoderLedColour);
			Wire.endTransmission();
		}

		void update() {
			Wire.requestFrom(ENCODER_MODULE_ADDR, 1);

			if (Wire.available()) {
				int encCounter = Wire.read();
				Serial.printf("Wire.read() = %d \n", encCounter);

				if (encoderCounterState != encCounter) {
					Serial.printf("encoderCounterState != encCounter \n");
					encoderCounterState = encCounter;
					_encoderChangedEventCallback(encCounter);
				}
				else {
					Serial.printf("encCounter = %d \n", encCounter);
				}
				if (encoderOnline == false) {
					encoderOnline = true;
					_encoderOnlineEventCallback(true);
				}
			}
			else {
				if (encoderOnline == true) {
					encoderOnline = false;
					_encoderOnlineEventCallback(false);	
				}
			}
		}
};

void encoderChangedEvent(int encoderValue) {
	debug.print(HARDWARE, "encoderChangedEvent(%d); \n", encoderValue);
	oldEncoderCounter = encoderCounter;
	encoderCounter = encoderValue;
}

void encoderOnlineEvent(bool online) {
	encoderOnline = online;
	updateDisplay();
}

EncoderModule encoderModule(&encoderChangedEvent, &encoderOnlineEvent);

//--------------------------------------------------------------

Scheduler runner;

bool tFlashLeds_onEnable();
void tFlashLeds_onDisable();
void tFlashLedsOn_callback();
void tFlashLedsOff_callback();

Task tFlashLeds(500, TASK_FOREVER, &tFlashLedsOff_callback);

bool tFlashLeds_onEnable() {
	encoderModule.setPixel(ENCODER_MODULE_LED_COLOUR_RED);
	tFlashLeds.enable();
    return true;
}
void tFlashLeds_onDisable() {
	encoderModule.setPixel(ENCODER_MODULE_LED_COLOUR_BLACK);
	tFlashLeds.disable();
}
void tFlashLedsOn_callback() {
	tFlashLeds.setCallback(&tFlashLedsOff_callback);
	encoderModule.setPixel(ENCODER_MODULE_LED_COLOUR_RED);
	debug.print(HARDWARE, "tFlashLedsOn_callback\n");
	return;
}
void tFlashLedsOff_callback() {
	tFlashLeds.setCallback(&tFlashLedsOn_callback);
	encoderModule.setPixel(ENCODER_MODULE_LED_COLOUR_BLACK);
	debug.print(HARDWARE, "tFlashLedsOff_callback\n");
	return;
}

//--------------------------------------------------------------
void sendMessage() {

	// taskENTER_CRITICAL(&mmux);
 //    int result = esk8.sendThenReadACK();
 //    taskEXIT_CRITICAL(&mmux);

 //    if (result == esk8.CODE_SUCCESS) {
	// 	throttleChanged = false;
	// 	lastRxFromBoard = millis();
	// 	rxDataFromBoard = true;
 //        debug.print(COMMUNICATION, "sendMessage(%d): batteryVoltage:%.1f \n", esk8.controllerPacket.throttle, esk8.boardPacket.batteryVoltage);
 //    }
 //    else if (result == esk8.ERR_NOT_SEND_OK) {
 //        debug.print(COMMUNICATION, "tSendControllerValues_callback(): ERR_NOT_SEND_OK \n");
 //    }
 //    else if (result == esk8.ERR_TIMEOUT) {
 //        debug.print(COMMUNICATION, "tSendControllerValues_callback(): ERR_TIMEOUT \n");
 //    }
 //    else {
 //        debug.print(COMMUNICATION, "tSendControllerValues_callback(): UNKNOWN_CODE %d \n", result);    
 //    }
}

void tSendControllerValues_callback() {
    sendMessage();
}
Task tSendControllerValues(SEND_TO_BOARD_INTERVAL_MS, TASK_FOREVER, &tSendControllerValues_callback);

//--------------------------------------------------------------
void boardOfflineCallback() {
	debug.print(ONLINE_STATUS, "offlineCallback();\n");
	tFlashLeds.enable();
	offlineCount++;
	updateDisplay();
	M5.Speaker.tone(330, 100);	// tone 330, 200ms
}

void boardOnlineCallback() {
	debug.print(ONLINE_STATUS, "onlineCallback();\n");	
	tFlashLeds.disable();
	updateDisplay();
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

void encoderEventHandler() {

	bool canAccelerate = true;// deadmanSwitch.isPressed();

	bool accelerating = encoderCounter > oldEncoderCounter;
	bool decelerating = encoderCounter < oldEncoderCounter;

	if (accelerating && (canAccelerate || encoderCounter < 0)) {
		esk8.controllerPacket.throttle = mapEncoderToThrottleValue(encoderCounter);
		debug.print(HARDWARE, "encoder: %d throttle: %d \n", encoderCounter, esk8.controllerPacket.throttle);
		throttleChanged = true;
	}
	else if (decelerating) {
		esk8.controllerPacket.throttle = mapEncoderToThrottleValue(encoderCounter);
		debug.print(HARDWARE, "encoder: %d throttle: %d \n", encoderCounter, esk8.controllerPacket.throttle);
		throttleChanged = true;
	}
	// else if (encoder.readStatus(E_PUSH)) {
	// 	encoderCounter = 0;
	// 	esk8.controllerPacket.throttle = mapEncoderToThrottleValue(encoderCounter);
	// 	debug.print(HARDWARE, "encoder: %d throttle: %d \n", encoderCounter, esk8.controllerPacket.throttle);
	// 	throttleChanged = true;
	// }
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
	debug.setFilter( STARTUP | HARDWARE | DEBUG | COMMUNICATION | ONLINE_STATUS | TIMING );
	//debug.setFilter( STARTUP );

	Wire.begin();

	// initialize the M5Stack object
	WiFi.mode( WIFI_OFF );	// WIFI_MODE_NULL
    btStop();   // turn bluetooth module off

	debug.print(STARTUP, "%s \n", compile_date);
    debug.print(STARTUP, "esk8Project/Controller.ino \n");

	throttleChanged = true;	// initialise

    radio.begin();

//	esk8.begin(&radio, ROLE_CONTROLLER, radioNumber);

//	sendMessage();

	runner.startNow();
 	// runner.addTask(tFastFlash);
	runner.addTask(tFlashLeds);
//	runner.addTask(tSendControllerValues);
//	tSendControllerValues.enable();

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

	runner.execute();

	delay(10);
}
/**************************************************************
					TASK 0
**************************************************************/
void codeForEncoderTask( void *parameter ) {

	// setupEncoder();

	long now = millis();

	// then loop forever	
	for (;;) {

		encoderModule.update();
		vTaskDelay( 300 );
	}

	vTaskDelete(NULL);
}
//**************************************************************

//--------------------------------------------------------------
void updateDisplay() {
	// #define LINE_1 20
	// #define LINE_2 45
	// #define LINE_3 70
	// #define LINE_4 95

	// // offline count
	// M5.Lcd.fillScreen(BLACK);
	// M5.Lcd.setCursor(10, LINE_1);
	// M5.Lcd.setTextColor(WHITE);
	// M5.Lcd.setTextSize(3);
	// M5.Lcd.printf("Offline count: %d", offlineCount);

	// // online/offline
	// M5.Lcd.setCursor(10, LINE_2);
	// M5.Lcd.setTextSize(3);
	// if (boardCommsStatus.isOnline()) {
	// 	M5.Lcd.setTextColor(GREEN);
	// 	M5.Lcd.printf("Online");
	// }
	// else {
	// 	M5.Lcd.setTextColor(RED);
	// 	M5.Lcd.printf("Offline");	
	// }

	// // encoder Module
	// M5.Lcd.setCursor(10, LINE_3);
	// if (encoderOnline == true) {
	// 	M5.Lcd.setTextColor(GREEN);
	// 	M5.Lcd.printf("Encoder: Online");
	// }
	// else {
	// 	M5.Lcd.setTextColor(RED);
	// 	M5.Lcd.printf("Encoder: Offline");	
	// }
}