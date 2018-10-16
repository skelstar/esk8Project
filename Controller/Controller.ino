#include <SPI.h>
#include "RF24.h"

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

i2cEncoderLib encoder(0x30); //Initialize the I2C encoder class with the I2C address 0x30 is the address with all the jumper open

int32_t encoderCounter;
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

//--------------------------------------------------------------------------------

const char compile_date[] = __DATE__ " " __TIME__;

int mapEncoderToThrottleValue(int raw);
void zeroThrottleReadyToSend();

//--------------------------------------------------------------

bool throttleChanged = false;
int throttle = 127;

#define SEND_TO_BOARD_INTERVAL_MS 			400

//--------------------------------------------------------------

#define ENCODER_MODULE_LED_CMD	1
#define ENCODER_MODULE_LED_COLOUR_BLACK	0
#define ENCODER_MODULE_LED_COLOUR_RED	1
#define ENCODER_MODULE_LED_COLOUR_BLUE	2
#define ENCODER_MODULE_LED_COLOUR_GREEN	3

class EncoderModule 
{
	typedef void ( *ButtonPressCallback )();
	typedef void ( *EncoderChangedEventCallback )( int value );

	private:
		int encoderCounterState = 0;

		EncoderChangedEventCallback _encoderChangedEventCallback;

	public:

		EncoderModule(EncoderChangedEventCallback encoderChangedEventCallback) 
		{
			_encoderChangedEventCallback = encoderChangedEventCallback;
		}

		void setPixel(byte encoderLedColour) {
			Wire.beginTransmission(ENCODER_MODULE_ADDR);
			Wire.write(ENCODER_MODULE_LED_CMD);
			Wire.write(encoderLedColour);
			Wire.endTransmission();
		}

		void update() {
			Wire.requestFrom(ENCODER_MODULE_ADDR, 3);

			if (Wire.available()) {
				int encoderCounter = Wire.read();

				debug.print(HARDWARE, "Encoder: %d \n", encoderCounter);

				if (encoderCounterState != encoderCounter) {
					encoderCounterState = encoderCounter;
					_encoderChangedEventCallback(encoderCounter);
				}
			}
		}
};

void encoderChangedEvent(int value) {
	debug.print(HARDWARE, "encoderChangedEvent(%d); \n", value);
}

EncoderModule encoderModule(&encoderChangedEvent);

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
}

void boardOnlineCallback() {
	debug.print(ONLINE_STATUS, "onlineCallback();\n");	
	tFlashLeds.disable();
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

	// bool canAccelerate = true;// deadmanSwitch.isPressed();

	// int newCounter = encoder.readCounterByte(); //Read only the first byte of the counter register

	// bool accelerating = newCounter > encoderCounter;
	// bool decelerating = newCounter < encoderCounter;

	// if (accelerating && (canAccelerate || encoderCounter < 0)) {
	// 	encoderCounter = newCounter;
	// 	esk8.controllerPacket.throttle = mapEncoderToThrottleValue(encoderCounter);
	// 	debug.print(HARDWARE, "encoder: %d throttle: %d \n", encoderCounter, esk8.controllerPacket.throttle);
	// 	throttleChanged = true;
	// }
	// else if (decelerating) {
	// 	encoderCounter = newCounter;
	// 	esk8.controllerPacket.throttle = mapEncoderToThrottleValue(encoderCounter);
	// 	debug.print(HARDWARE, "encoder: %d throttle: %d \n", encoderCounter, esk8.controllerPacket.throttle);
	// 	throttleChanged = true;
	// }
	// else if (encoder.readStatus(E_PUSH)) {
	// 	encoderCounter = 0;
	// 	esk8.controllerPacket.throttle = mapEncoderToThrottleValue(encoderCounter);
	// 	debug.print(HARDWARE, "encoder: %d throttle: %d \n", encoderCounter, esk8.controllerPacket.throttle);
	// 	throttleChanged = true;
	// }
}

void m5ButtonInterruptHandler() {
	int m5Astate = digitalRead(M5_BUTTON_A);
	if (m5Astate == 0 && encoderCounter == 0) {
		// encoderCounter++;
		// esk8.controllerPacket.throttle = mapEncoderToThrottleValue(encoderCounter);
		// debug.print(HARDWARE, "encoder: %d throttle: %d \n", encoderCounter, esk8.controllerPacket.throttle);
		// throttleChanged = true;	
	}
	else if (m5Astate == 1 && encoderCounter > 0) {
		// encoderCounter--;
		// esk8.controllerPacket.throttle = mapEncoderToThrottleValue(encoderCounter);
		// debug.print(HARDWARE, "encoder: %d throttle: %d \n", encoderCounter, esk8.controllerPacket.throttle);
		// throttleChanged = true;	
	} else {
		// debug.print(HARDWARE, "void m5ButtonInterruptHandler() %d %d {\n", encoderCounter, m5Astate);
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

	// initialize the M5Stack object
	M5.begin(true, false, false); //lcd, sd, serial

	//text print
	M5.Lcd.fillScreen(BLACK);
	M5.Lcd.setCursor(10, 10);
	M5.Lcd.setTextColor(WHITE);
	M5.Lcd.setTextSize(1);
	M5.Lcd.printf("Display Test!");

	debug.print(STARTUP, "%s \n", compile_date);
    debug.print(STARTUP, "esk8Project/Controller.ino \n");

	throttleChanged = true;	// initialise

    radio.begin();

    btStop();   // turn bluetooth module off

	esk8.begin(&radio, ROLE_CONTROLLER, radioNumber);

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

	if (throttleChanged && connected) {
		tSendControllerValues.restart();
	}
	else if (esk8.controllerPacket.throttle == 127 && rxDataFromBoard) {
		rxDataFromBoard = false;
		char buf[100];
		sprintf(buf, "%0.1f", esk8.boardPacket.batteryVoltage);
		oled2LineMessage("BATT. VOLTS", buf, "V");
	}
	else if (esk8.controllerPacket.throttle != 127) {
		M5.Lcd.fillScreen(BLACK);
		M5.Lcd.setCursor(10, 10);
		M5.Lcd.setTextColor(WHITE);
		M5.Lcd.setTextSize(3);
		M5.Lcd.printf("Encoder: %d", esk8.controllerPacket.throttle);
	}

	if (millis() - nowms > 500) {
		nowms = millis();
	}

	runner.execute();

	delay(10);
}
/**************************************************************
					TASK 0
**************************************************************/
void codeForEncoderTask( void *parameter ) {

	setupEncoder();

	setupM5Button();

	long now = millis();

	// then loop forever	
	for (;;) {

		//encoderButton.serviceEvents();
		// if (encoder.updateStatus()) {
		// 	encoderEventHandler();
		// }

		encoderModule.update();
		delay(100);
	}

	vTaskDelete(NULL);
}
//**************************************************************
void setupM5Button() {
	pinMode(M5_BUTTON_A, INPUT_PULLUP);
	attachInterrupt(digitalPinToInterrupt(M5_BUTTON_A), m5ButtonInterruptHandler, CHANGE);
}

void setupEncoder() {

	encoder.begin(( INTE_DISABLE | LEDE_DISABLE | WRAP_DISABLE | DIRE_RIGHT | IPUP_DISABLE | RMOD_X1 )); //INTE_ENABLE | LEDE_ENABLE | 
	encoder.writeCounter(0);
	encoder.writeMax(ENCODER_COUNTER_MAX);
	encoder.writeMin(ENCODER_COUNTER_MIN);
	encoder.writeLEDA(0x00);
	encoder.writeLEDB(0x00);
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
}
//--------------------------------------------------------------
void oledMessage(char* line1) {
}
//--------------------------------------------------------------
