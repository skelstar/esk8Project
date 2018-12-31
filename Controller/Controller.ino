#include <SPI.h>
#include <RF24Network.h>
#include <RF24.h> 
// #include "WiFi.h"
#include <Adafruit_NeoPixel.h>
#include <TaskScheduler.h>

#include <myPushButton.h>
#include <debugHelper.h>

#include <esk8Lib.h>
#include <EncoderBasicModule.h>
#include <OnlineStatusLib.h>
#include <Rotary.h>

// #include "TFT_eSPI.h"
// #include "Free_Fonts.h" 

//--------------------------------------------------------------------------------



#define SEND_TO_BOARD_INTERVAL_MS 	200
#define BOARD_OFFLINE_PERIOD		1000


#define ENCODER_PIN_A		26
#define ENCODER_PIN_B 		36

#define	PIXEL_PIN			16	// was 5

#define DEADMAN_SWITCH		35

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

void attachEncoderInterrupts() {
	attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), callback, CHANGE);
	attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), callback, CHANGE);
}

EncoderBasicModule throttleDevice;

Rotary rotary(ENCODER_PIN_A, ENCODER_PIN_B);

void callback() {
	throttleDevice.encoderInterruptHandler();
}

void encoderChangedEventCallback() {
	debug.print(DEBUG, "Encoder changed: %d \n", throttleDevice.getEncoderCount());
}

// TFT_eSPI tft = TFT_eSPI();
// TFT_eSprite img = TFT_eSprite(&tft);

//--------------------------------------------------------------

int32_t throttleValue;
int32_t oldThrottleValue = 0;
uint8_t encoder_status;
// stats
int32_t packetsSent = 0;
int32_t packetsFailed = 0;

//--------------------------------------------------------------

volatile long lastRxFromBoard = 0;

portMUX_TYPE mmux = portMUX_INITIALIZER_UNLOCKED;

//--------------------------------------------------------------

#define SPI_CE        5 	//33    // white/purple
#define SPI_CS        13	//26  // green

RF24 radio(SPI_CE, SPI_CS);    // ce pin, cs pin
RF24Network network(radio); 

int offlineCount = 0;
int encoderOfflineCount = 0;
bool encoderOnline = true;

#define NUMPIXELS 10
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(/*numpixels*/ NUMPIXELS, /*pin*/ 15, NEO_GRB + NEO_KHZ800);

//--------------------------------------------------------------------------------

const char compile_date[] = __DATE__ " " __TIME__;
const char file_name[] = __FILE__;

//--------------------------------------------------------------

bool throttleChanged = false;
int throttle = 127;

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
	lastRxFromBoard = millis();
	Serial.printf("r");
	txCharsLength++;
	if ( txCharsLength > 30 ) {
		txCharsLength = 0;
		Serial.printf("\n");
	}
}

//--------------------------------------------------------------
void tSendControllerValues_callback() {

	taskENTER_CRITICAL(&mmux);
	esk8.controllerPacket.throttle++;
	bool sentOK = 	esk8.sendPacketToBoard();
    taskEXIT_CRITICAL(&mmux);

    if ( sentOK ) {
		throttleChanged = false;
    }
    else {
        debug.print(COMMUNICATION, "tSendControllerValues_callback(): ERR_NOT_SEND_OK \n");
    }
}
Task tSendControllerValues(SEND_TO_BOARD_INTERVAL_MS, TASK_FOREVER, &tSendControllerValues_callback);

//--------------------------------------------------------------
void boardOfflineCallback() {
	setPixelsColor(pixels.Color(0, 0, 120));
	//debug.print(ONLINE_STATUS, "offlineCallback();\n");
	// tFlashLeds.enable();
	// offlineCount++;
	// M5.Speaker.tone(330, 100);	// tone 330, 200ms
}

void boardOnlineCallback() {
	setPixelsColor(pixels.Color(120, 0, 0));
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
	debug.addOption(TIMING, "TIMING");
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
    setPixelsColor(pixels.Color(0, 150, 0));
	
	throttleChanged = true;	// initialise

	SPI.begin();                                           // Bring up the RF network
	radio.begin();
	radio.setAutoAck(true);
	esk8.begin(&radio, &network, esk8.RF24_CONTROLLER, packetAvailableCallback);

 	pinMode(ENCODER_PIN_A, INPUT_PULLUP);
	pinMode(ENCODER_PIN_B, INPUT_PULLUP);
   	attachEncoderInterrupts();
	throttleDevice.begin(DEADMAN_SWITCH, rotary, encoderChangedEventCallback);

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

	bool connected = millis() - lastRxFromBoard < BOARD_OFFLINE_PERIOD;
	boardStatus.serviceState(connected);

	runner.execute();

	esk8.service();

	throttleDevice.update();

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

void setPixelsColor(uint16_t color) {
	for (int i = 0; i < NUMPIXELS; i++){
		pixels.setPixelColor(i, color);
		vTaskDelay( 1 );
	}
	pixels.show();
}