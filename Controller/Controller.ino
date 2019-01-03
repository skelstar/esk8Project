#include <SPI.h>
#include <RF24Network.h>
#include <RF24.h> 
// #include "WiFi.h"
#include <Adafruit_NeoPixel.h>
#include <TaskScheduler.h>

#include <M5Stack.h>

#include <myPushButton.h>
#include <debugHelper.h>

#include <esk8Lib.h>

#include "Wire.h"
#define JOY_ADDR 0x52

#include <OnlineStatusLib.h>

/* Display */
// #include "TFT_eSPI.h"
#include "Free_Fonts.h" 
//--------------------------------------------------------------------------------



#define READ_JOYSTICK_INTERVAL		150
#define SEND_TO_BOARD_INTERVAL_MS 	200
#define BOARD_OFFLINE_CONSECUTIVE_TIMES_ALLOWANCE	4

#define ENCODER_MIN 	-20 	// decceleration (ie -20 divides 0-127 into 20)
#define ENCODER_MAX 	15 	// acceleration (ie 15 divides 127-255 into 15)

#define ENCODER_PIN_A		26
#define ENCODER_PIN_B 		36

#define	M5STACK_FIRE_PIXEL_PIN			15	// was 5

// can't use pins: 17, 16, 35
#define DEADMAN_SWITCH		2

// #define M5_BUTTON_A			39
// #define M5_BUTTON_B			38
// #define M5_BUTTON_C			37

//--------------------------------------------------------------

#define	STARTUP 		1 << 0
#define DEBUG 			1 << 1
#define COMMUNICATION 	1 << 2
#define HARDWARE		1 << 3
#define ONLINE_STATUS	1 << 5
#define TIMING			1 << 6



debugHelper debug;

esk8Lib esk8;

/*****************************************************/

bool updateBoardImmediately = false;
volatile long lastAckFromBoard = 0;

// raw min-max values (not something to configure)
int joystickDeadZone = 3;
int joystickMin = 11;
int joystickMiddle = 127;
int joystickMax = 246;		

// use these to limit power
int throttleMin = 60;	// 0
int throttleMax = 220;	// 255

/**************************************************************/
int setupJoystick() {
	Wire.begin(21, 22, 400000);

	if ( getRawValuesFromJoystick() == -1 ) {
		debug.print(STARTUP, "WARNING: encoder device is not connected!");
		return -1;
	}

	setRawJoystickValues(/*min*/ 11, /*middle*/ 127, /*max*/ 246, /*deadZoneSize*/ 7);
	setThrottleMinsMaxs(/*min*/throttleMin, /*max*/throttleMax);
	return 1;
}

volatile uint8_t x_data;
volatile uint8_t y_data;
volatile uint8_t button_data;

int getRawValuesFromJoystick() {

	Wire.requestFrom(JOY_ADDR, 3);
	if ( Wire.available() ) {
		x_data = Wire.read();
		y_data = Wire.read();
		button_data = Wire.read();
		// debug.print(HARDWARE, "x:%d y:%d button:%d\n", x_data, y_data, button_data);
	}
	else {
		return -1;	// ERROR condition
	}
	return y_data;
}

/*****************************************************/

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite img = TFT_eSprite(&tft);

#define IWIDTH	320
#define IHEIGHT	240

//--------------------------------------------------------------

portMUX_TYPE mmux = portMUX_INITIALIZER_UNLOCKED;

//--------------------------------------------------------------

#define SPI_CE        5 	//33    // white/purple
#define SPI_CS        13	//26  // green

RF24 radio(SPI_CE, SPI_CS);    // ce pin, cs pin
RF24Network network(radio); 

#define NUMPIXELS 10
Adafruit_NeoPixel pixels = Adafruit_NeoPixel( NUMPIXELS, /*pin*/ M5STACK_FIRE_PIXEL_PIN, NEO_GRB + NEO_KHZ800);

struct commsStatsStruct {
	int totalPacketsSent;
	int packetFailureCount;
	int timesOfflineCount;
};

volatile commsStatsStruct commsStats;

//--------------------------------------------------------------------------------

const char compile_date[] = __DATE__ " " __TIME__;
const char file_name[] = __FILE__;

//--------------------------------------------------------------

//--------------------------------------------------------------

// #define PULLUP		true
// #define OFFSTATE	LOW
// void m5ButtonACallback(int eventCode, int eventPin, int eventParam);
// myPushButton m5ButtonA(M5_BUTTON_A, PULLUP, /* offstate*/ HIGH, m5ButtonACallback);
// void m5ButtonACallback(int eventCode, int eventPin, int eventParam) {
    
// 	switch (eventCode) {
// 		case m5ButtonA.EV_BUTTON_PRESSED:
// 			break;
// 		// case m5ButtonA.EV_RELEASED:
// 		// 	break;
// 		// case m5ButtonA.EV_DOUBLETAP:
// 		// 	break;
// 		// case m5ButtonA.EV_HELD_SECONDS:
// 		// 	break;
//     }
// }

//--------------------------------------------------------------

Scheduler runner;

bool tFlashLeds_onEnable();
void tFlashLeds_onDisable();
void tFlashLedsOn_callback();
void tFlashLedsOff_callback();

Task tFlashLeds(500, TASK_FOREVER, &tFlashLedsOff_callback);

bool tFlashLeds_onEnable() {
	//encoderModule.setPixel(ENCODER_MODULE_LED_COLOUR_RED);
	// tFlashLeds.enable();
    return true;
}
void tFlashLeds_onDisable() {
	//encoderModule.setPixel(ENCODER_MODULE_LED_COLOUR_BLACK);
	// tFlashLeds.disable();
}
void tFlashLedsOn_callback() {
	// tFlashLeds.setCallback(&tFlashLedsOff_callback);
	//encoderModule.setPixel(ENCODER_MODULE_LED_COLOUR_RED);
	debug.print(HARDWARE, "tFlashLedsOn_callback\n");
	return;
}
void tFlashLedsOff_callback() {
	// tFlashLeds.setCallback(&tFlashLedsOn_callback);
	//encoderModule.setPixel(ENCODER_MODULE_LED_COLOUR_BLACK);
	debug.print(HARDWARE, "tFlashLedsOff_callback\n");
	return;
}
//--------------------------------------------------------------
int txCharsLength = 0;

void packetAvailableCallback( uint16_t from ) {
	lastAckFromBoard = millis();
	debug.print(COMMUNICATION, "Rx from Board: batt voltage %.1f \n", esk8.boardPacket.batteryVoltage);
}

//--------------------------------------------------------------

void tSendControllerValues_callback() {

	taskENTER_CRITICAL(&mmux);

	bool sentOK = esk8.sendPacketToBoard();

    taskEXIT_CRITICAL(&mmux);

    commsStats.totalPacketsSent++;

    if ( sentOK ) {
    	lastAckFromBoard = millis();
		updateBoardImmediately = false;
    }
    else {
    	commsStats.packetFailureCount++;
        debug.print(COMMUNICATION, "tSendControllerValues_callback(): ERR_NOT_SEND_OK \n");
    }
}
Task tSendControllerValues(SEND_TO_BOARD_INTERVAL_MS, TASK_FOREVER, &tSendControllerValues_callback);

//--------------------------------------------------------------
void boardOfflineCallback() {
    uint32_t red = pixels.Color(255, 0, 0);
	ledsUpdate(red);
	// tFlashLeds.enable();
	// M5.Speaker.tone(330, 100);	// tone 330, 200ms
}

void boardOnlineCallback() {
    uint32_t green = pixels.Color(0, 255, 0);
	ledsUpdate(green);
	commsStats.timesOfflineCount++;
	//debug.print(ONLINE_STATUS, "onlineCallback();\n");	
	// tFlashLeds.disable();
	// updateDisplay(/* mode */ 1, /* backlight */ 1);
}

OnlineStatusLib boardStatus(
	boardOfflineCallback, 
	boardOnlineCallback, 
	BOARD_OFFLINE_CONSECUTIVE_TIMES_ALLOWANCE, 
	/*debug*/ true);

//--------------------------------------------------------------

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
	// debug.addOption(TIMING, "TIMING");
	//debug.setFilter( STARTUP | COMMUNICATION | ONLINE_STATUS | TIMING );
	debug.setFilter( STARTUP );	// | DEBUG | COMMUNICATION | HARDWARE );
	//debug.setFilter( STARTUP );

	// disable speaker noise
	dacWrite(25, 0);

	M5.begin();

	// WiFi.mode( WIFI_OFF );	// WIFI_MODE_NULL
 //    btStop();   // turn bluetooth module off

    debug.print(STARTUP, "%s\n", file_name);
	debug.print(STARTUP, "%s\n", compile_date);
	
    pixels.begin();
    ledsUpdate(pixels.Color(0, 120, 0));
	
	updateBoardImmediately = true;	// initialise

	SPI.begin();                                           // Bring up the RF network
	radio.begin();
	commsStats.totalPacketsSent = 0;
	commsStats.packetFailureCount = 0;
	commsStats.timesOfflineCount = 0;
	radio.setAutoAck(true);
	esk8.begin(&radio, &network, esk8.RF24_CONTROLLER, packetAvailableCallback);

	if ( setupJoystick() == -1 ) {
		// error condition (joystick not connected)
	}
	
	setupDisplay();

	runner.startNow();
	runner.addTask(tFlashLeds);
	runner.addTask(tSendControllerValues);
	tSendControllerValues.enable();

	boardStatus.serviceState(/*online*/ false);

	pinMode(DEADMAN_SWITCH, INPUT_PULLUP);

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
long timeLastReadJoystick = 0;
long nowMs = 0;

void loop() {

	bool connected = millis() - lastAckFromBoard < (SEND_TO_BOARD_INTERVAL_MS + 100);
	boardStatus.serviceState(connected);

	runner.execute();

	esk8.service();

	if ( readJoystickOk() == false ) {
		// joystick not connected
		updateBoardImmediately = true;
		esk8.controllerPacket.throttle = 127;
	}

	if ( updateBoardImmediately ) {
		updateBoardImmediately = false;
		tSendControllerValues.restart();
	}
	
	M5.update();
	if ( M5.BtnA.isPressed() && M5.BtnC.isPressed() ) {
		powerDown();
	}

	if ( millis() - nowMs > 1000 ) {
		nowMs = millis();
		updateDisplay();
	}


	vTaskDelay( 10 );
}
/**************************************************************
					TASK 0
**************************************************************/
void codeForEncoderTask( void *parameter ) {

	#define TX_INTERVAL 200
	
	for (;;) {
		vTaskDelay( 10 );

	}

	vTaskDelete(NULL);
}
//**************************************************************

bool readJoystickOk() {
	
	if (millis() - timeLastReadJoystick > READ_JOYSTICK_INTERVAL) {
		timeLastReadJoystick = millis();
		byte result = getRawValuesFromJoystick();
		// check ERROR condition
		if ( result == -1 ) {
			debug.print(STARTUP, "WARNING: encoder device is not connected!");
			return false;
		}

		byte mappedThrottle = mapJoystickToThrottle(result);
		debug.print(HARDWARE, "Mapped throttle: %d \n", mappedThrottle);
		if ( mappedThrottle != esk8.controllerPacket.throttle ) {
			updateBoardImmediately = true;
			esk8.controllerPacket.throttle = mappedThrottle;
		}
		return true;
	}
	return true;
}
//--------------------------------------------------------------
void setRawJoystickValues(int min, int middle, int max, int deadZoneSize) {

	joystickMin = min;
	joystickMax = max;		
	joystickMiddle = middle;
	joystickDeadZone = 3;
}

void setThrottleMinsMaxs(int min, int max) {

	if (max < 127) {
		throttleMax = 127;
	} 
	else if (max > 255) {
		throttleMax = 255;
	}
	else {
		throttleMax = max;
	}

	if (min > 127) {
		throttleMin = 127;
	}
	else if (min < 0) {
		throttleMin = 0;
	}
	else {
		throttleMin = min;	// mapped min value
	}
}
//--------------------------------------------------------------
byte mapJoystickToThrottle(byte joystickValue) {

	if ( joystickValue > joystickMiddle + joystickDeadZone ) {
		// acelerate
		return map(joystickValue, 
			/*r-mid*/ joystickMiddle + joystickDeadZone, 
			/*r-max*/ joystickMax, 
			/*mid*/ 127+joystickDeadZone, 
			/*max*/ throttleMax);
	}
	else if ( joystickValue < joystickMiddle - joystickDeadZone ) {
		// braking
		return map(joystickValue, 
			/*r-min*/ joystickMin, 
			/*r-mid*/ joystickMiddle - joystickDeadZone, 
			/*min*/ throttleMin, 
			/*max*/ 127-joystickDeadZone);
	}
	return 127;
}

//--------------------------------------------------------------

void setupDisplay() {

	tft.begin();

  	tft.setRotation(1); // 0 is portrait
	tft.fillScreen(TFT_BLACK);            // Clear screen

	img.setColorDepth(8); // Optionally set depth to 8 to halve RAM use
	img.createSprite(IWIDTH, IHEIGHT);
	// img.fillSprite(TFT_BLUE);
	img.setFreeFont(FF18);                 // Select the font
  	img.setTextDatum(MC_DATUM);
	img.setTextColor(TFT_YELLOW, TFT_BLACK);
	img.drawString("READY!", /*x*/ 320/2, /*y*/240/2, /*font*/2);
	img.pushSprite(0, 0);
}

//--------------------------------------------------------------

void updateDisplay() {
	// commsStats
	char stats[6];	// xxx.x\0

	if (commsStats.packetFailureCount > 0 && commsStats.totalPacketsSent > 0) {
		float ratio = (float)commsStats.packetFailureCount / (float)commsStats.totalPacketsSent;
		dtostrf(ratio*100, 6, 1, stats);

		Serial.printf("%d %d %f \n", 
			commsStats.packetFailureCount, 
			commsStats.totalPacketsSent,
			ratio
			);

		// String failRatio = String((float)commsStats.packetFailureCount / (float)commsStats.totalPacketsSent);
		// char *result = failRatio.c_str();
		// sprintf(stats, "Fail Rate: %s", result);

		img.drawString(stats, /*x*/ 320/2, /*y*/240/2, /*font*/4);
		img.pushSprite(0, 0);
	}
}
//--------------------------------------------------------------
void ledsUpdate(uint32_t color) {
	for (int i = 0; i < NUMPIXELS; i++){
		pixels.setPixelColor(i, color);
		vTaskDelay( 1 );
	}
	pixels.show();
}
//--------------------------------------------------------------
void powerDown() {
	img.drawString("POWER DOWN!", /*x*/ 320/2, /*y*/240/2, /*font*/2);
	// radio
	radio.stopListening();
	radio.powerDown();
	// leds
	ledsUpdate(pixels.Color(0, 0, 0));
	// message
	img.pushSprite(0, 0);
	delay(300);
    M5.powerOFF();
}