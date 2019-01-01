#include <SPI.h>
#include <RF24Network.h>
#include <RF24.h> 
// #include "WiFi.h"
#include <Adafruit_NeoPixel.h>
#include <TaskScheduler.h>

#include <myPushButton.h>
#include <debugHelper.h>

#include <esk8Lib.h>
// #include <EncoderBasicModule.h>
// #include <Rotary.h>
#include <Encoderi2cModulev1Lib.h>
#include <OnlineStatusLib.h>

// #include "TFT_eSPI.h"
// #include "Free_Fonts.h" 

//--------------------------------------------------------------------------------



#define SEND_TO_BOARD_INTERVAL_MS 	200
#define BOARD_OFFLINE_PERIOD		500

#define ENCODER_MIN 	-20 	// decceleration (ie -20 divides 0-127 into 20)
#define ENCODER_MAX 	15 	// acceleration (ie 15 divides 127-255 into 15)

#define ENCODER_PIN_A		26
#define ENCODER_PIN_B 		36

#define	PIXEL_PIN			16	// was 5

// can't use pins: 17, 16, 35
#define DEADMAN_SWITCH		2

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

/*****************************************************/

void encoderChangedEventCallback(int newThrottleValue);
void encoderPressedEventCallback();
bool getEncoderCanAccelerateCallback();

Encoderi2cModulev1Lib throttleDevice(
		encoderChangedEventCallback,
		encoderPressedEventCallback,
		getEncoderCanAccelerateCallback,
		ENCODER_MIN, 
		ENCODER_MAX);

void encoderChangedEventCallback(int newThrottleValue) {
	esk8.controllerPacket.throttle = newThrottleValue;
	debug.print(HARDWARE, "Encoder changed: %d \n", newThrottleValue);
}

bool throttleChanged = false;

void encoderPressedEventCallback() {
	debug.print(HARDWARE, "Encoder Pressed! \n");
	// reset encoder/throttle back to 0 (127)
	esk8.controllerPacket.throttle = 127;
	throttleChanged = true;
	throttleDevice.setEncoderCount(0);
}

bool getEncoderCanAccelerateCallback() {
	bool deadmanPressed = digitalRead(DEADMAN_SWITCH);
	debug.print(HARDWARE, "Deadman pressed: %d \n", deadmanPressed);
	return deadmanPressed;
}

/*****************************************************/


volatile long lastAckFromBoard = 0;

// TFT_eSPI tft = TFT_eSPI();
// TFT_eSprite img = TFT_eSprite(&tft);

//--------------------------------------------------------------

portMUX_TYPE mmux = portMUX_INITIALIZER_UNLOCKED;

//--------------------------------------------------------------

#define SPI_CE        5 	//33    // white/purple
#define SPI_CS        13	//26  // green

RF24 radio(SPI_CE, SPI_CS);    // ce pin, cs pin
RF24Network network(radio); 

#define NUMPIXELS 10
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(/*numpixels*/ NUMPIXELS, /*pin*/ 15, NEO_GRB + NEO_KHZ800);

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
	bool sentOK = 	esk8.sendPacketToBoard();
    taskEXIT_CRITICAL(&mmux);

    if ( sentOK ) {
    	lastAckFromBoard = millis();
		throttleChanged = false;
    }
    else {
        debug.print(COMMUNICATION, "tSendControllerValues_callback(): ERR_NOT_SEND_OK \n");
    }
}
Task tSendControllerValues(SEND_TO_BOARD_INTERVAL_MS, TASK_FOREVER, &tSendControllerValues_callback);

//--------------------------------------------------------------
void boardOfflineCallback() {
	ledsUpdate(pixels.Color(0, 0, 120));
	//debug.print(ONLINE_STATUS, "offlineCallback();\n");
	// tFlashLeds.enable();
	// offlineCount++;
	// M5.Speaker.tone(330, 100);	// tone 330, 200ms
}

void boardOnlineCallback() {
	ledsUpdate(pixels.Color(120, 0, 0));
	//debug.print(ONLINE_STATUS, "onlineCallback();\n");	
	// tFlashLeds.disable();
	// updateDisplay(/* mode */ 1, /* backlight */ 1);
}

OnlineStatusLib boardStatus(boardOfflineCallback, boardOnlineCallback, /*debug*/ true);

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
	debug.setFilter( STARTUP | DEBUG | COMMUNICATION );
	//debug.setFilter( STARTUP );

	// tft.begin();

 //  	tft.setRotation(1);
	// tft.fillScreen(TFT_BLACK);            // Clear screen

	// //img.setColorDepth(8); // Optionally set depth to 8 to halve RAM use
	// img.createSprite(IWIDTH, IHEIGHT);
	// img.fillSprite(TFT_BLUE);
	// img.setFreeFont(FF18);                 // Select the font
 //  	img.setTextDatum(MC_DATUM);
	// img.setTextColor(TFT_YELLOW, TFT_BLACK);
	// img.drawString("Ready!", 10, 20, 2);
	
	// WiFi.mode( WIFI_OFF );	// WIFI_MODE_NULL
 //    btStop();   // turn bluetooth module off

    debug.print(STARTUP, "%s\n", file_name);
	debug.print(STARTUP, "%s\n", compile_date);
	
    pixels.begin();
    ledsUpdate(pixels.Color(0, 150, 0));
	
	throttleChanged = true;	// initialise

	SPI.begin();                                           // Bring up the RF network
	radio.begin();
	radio.setAutoAck(true);
	esk8.begin(&radio, &network, esk8.RF24_CONTROLLER, packetAvailableCallback);

	if ( throttleDevice.isConnected() == false ) {
		debug.print(STARTUP, "WARNING: encoder device is not connected!");
	}

	// img.pushSprite(200, 100); delay(500);	

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

void loop() {

	bool connected = millis() - lastAckFromBoard < BOARD_OFFLINE_PERIOD;
	boardStatus.serviceState(connected);

	runner.execute();

	esk8.service();

	throttleDevice.update();

	if ( throttleChanged ) {
		throttleChanged = false;
		tSendControllerValues.restart();
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

void updateDisplay(int mode, int backlight) {
	// #define LINE_1 20
	// #define LINE_2 45
	// #define LINE_3 70
	// #define LINE_4 95

	// if ( backlight == false ) {
	// 	digitalWrite(TFT_BL, LOW);
	// 	return;
	// }

	// switch (mode) {
	// 	case 0:
	// 		break;
	// 	case 1:
	// 	  	img.setTextDatum(MC_DATUM);
	// 		img.setTextSize(1);
	// 		img.fillScreen(TFT_DARKGREEN);
	// 		img.setTextColor(TFT_YELLOW, TFT_BLACK);
	// 		img.drawNumber( esk8.controllerPacket.throttle,  10, 10);		
	// 		img.pushSprite(20, 20); delay(0);	

	// 		digitalWrite(TFT_BL, HIGH);	// turn backlight off?

	// 		break;
	// }
}
//--------------------------------------------------------------
void ledsUpdate(uint16_t color) {
	for (int i = 0; i < NUMPIXELS; i++){
		pixels.setPixelColor(i, color);
		vTaskDelay( 1 );
	}
	pixels.show();
}
//--------------------------------------------------------------
