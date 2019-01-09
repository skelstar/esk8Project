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
// #include "Org_01.h"

//--------------------------------------------------------------------------------

#define READ_JOYSTICK_INTERVAL		150
#define SEND_TO_BOARD_INTERVAL_MS 	200
#define BOARD_OFFLINE_CONSECUTIVE_TIMES_ALLOWANCE	4

#define ENCODER_MIN 	-20 	// decceleration (ie -20 divides 0-127 into 20)
#define ENCODER_MAX 	15 	// acceleration (ie 15 divides 127-255 into 15)

#define ENCODER_PIN_A		26
#define ENCODER_PIN_B 		36

#define BUTTON_A_PIN 39
#define BUTTON_B_PIN 38
#define BUTTON_C_PIN 37

#define	M5STACK_FIRE_PIXEL_PIN			15	// was 5

// can't use pins: 17, 16, 35
// #define DEADMAN_SWITCH		2

/*****************************************************/

#define	STARTUP 		1 << 0
#define DEBUG 			1 << 1
#define COMMUNICATION 	1 << 2
#define HARDWARE		1 << 3
#define ONLINE_STATUS	1 << 5
#define LOGGING			1 << 6

debugHelper debug;

/*****************************************************/

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
		debug.print(STARTUP, "WARNING: jopystick is not connected!");
		return -1;
	}

	setRawJoystickValues(/*min*/ 11, /*middle*/ 127, /*max*/ 246, /*deadZoneSize*/ 7);
	setThrottleMinsMaxs(/*min*/throttleMin, /*max*/throttleMax);
	return 1;
}
//--------------------------------------------------------------
volatile uint8_t x_data;
volatile uint8_t y_data;
volatile uint8_t button_data;
//--------------------------------------------------------------
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

TFT_eSprite img_topLeft = TFT_eSprite(&tft);
TFT_eSprite img_topRight = TFT_eSprite(&tft);
TFT_eSprite img_middle = TFT_eSprite(&tft);
TFT_eSprite img_bottomLeft = TFT_eSprite(&tft);
TFT_eSprite img_bottomRight = TFT_eSprite(&tft);

#define SPRITE_SMALL_WIDTH	320/2
#define SPRITE_SMALL_HEIGHT	240/4
#define SPRITE_MED_WIDTH	320
#define SPRITE_MED_HEIGHT	240/2

#include "Display.h"

//--------------------------------------------------------------

portMUX_TYPE mmux = portMUX_INITIALIZER_UNLOCKED;

//--------------------------------------------------------------

#define SPI_CE        5 	//33    // white/purple
#define SPI_CS        13	//26  // green

RF24 radio(SPI_CE, SPI_CS);    // ce pin, cs pin
RF24Network network(radio); 

//--------------------------------------------------------------

// -- Task handles for use in the notifications
static TaskHandle_t FastLEDshowTaskHandle = 0;
static TaskHandle_t userTaskHandle = 0;

#define NUMPIXELS 10
Adafruit_NeoPixel pixels = Adafruit_NeoPixel( NUMPIXELS, /*pin*/ M5STACK_FIRE_PIXEL_PIN, NEO_GRB + NEO_KHZ800);

void NeoPixelsShowESP32() {
	if ( userTaskHandle == 0 ) {
       // -- Store the handle of the current task, so that the show task can notify it when it's done
        userTaskHandle = xTaskGetCurrentTaskHandle();
		// make sure is not null otherwise will fail
		if ( FastLEDshowTaskHandle != NULL ) {
        // -- Trigger the show task
			xTaskNotifyGive(FastLEDshowTaskHandle);
			// -- Wait to be notified that it's done
			const TickType_t xMaxBlockTime = pdMS_TO_TICKS( 200 );
			ulTaskNotifyTake(pdTRUE, xMaxBlockTime);
			userTaskHandle = 0;	
		}
	}
}


const uint32_t COLOUR_LIGHT_GREEN = pixels.Color(0, 50, 0);
const uint32_t COLOUR_BRIGHT_RED = pixels.Color(255, 0, 0);

volatile int timesOfflineCount;

//--------------------------------------------------------------------------------

const char compile_date[] = __DATE__ " " __TIME__;
const char file_name[] = __FILE__;

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
	debug.print(COMMUNICATION, "Rx from Board: batt voltage %.1f, vescOnline: %d \n", 
		esk8.boardPacket.batteryVoltage, 
		esk8.boardPacket.vescOnline);
}

//--------------------------------------------------------------
void boardOfflineCallback() {
	ledsUpdate( COLOUR_BRIGHT_RED );
	// tFlashLeds.enable();
	// M5.Speaker.tone(330, 100);	// tone 330, 200ms
}

void boardOnlineCallback() {
	ledsUpdate( COLOUR_LIGHT_GREEN );
	timesOfflineCount++;
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

#define PACKET_LOG_SIZE		50 	// 5/sec *10 minutes
#define BLANK_LOG_ENTRY		255
volatile byte packetLog[PACKET_LOG_SIZE];
volatile int packetLogPtr = 0;
volatile float currentFailRatio = 0;

void initPacketLog() {
	for (int i=0; i<PACKET_LOG_SIZE; i++) {
		packetLog[i] = BLANK_LOG_ENTRY;
	}
	packetLogPtr = 0;
}

void logPacket(bool failed) {
	packetLog[packetLogPtr] = failed == false;
	debug.print(LOGGING, "logged: %d (%d)\n", packetLog[packetLogPtr], packetLogPtr);
	packetLogPtr = packetLogPtr < PACKET_LOG_SIZE - 1 
		? packetLogPtr + 1 
		: 0;
}

float calculatePacketRatio() {
	int packetSum = 0;
	int failSum = 0;
	long start = micros();
	for (int i=0; i<PACKET_LOG_SIZE; i++) {
		if (packetLog[i] != BLANK_LOG_ENTRY) {
			packetSum++;
			if (packetLog[i] == 1) {
				failSum++;
			}
		}
	}
	char stats[5];
	currentFailRatio = (float)failSum / (float)packetSum;
	dtostrf(currentFailRatio*100, 4, 1, stats);

	debug.print(LOGGING, "ratio: %s and took %ulus\n", stats, micros() - start);
}

void tSendControllerValues_callback() {

	taskENTER_CRITICAL(&mmux);

	bool sentOK = esk8.sendPacketToBoard();
	debug.print(COMMUNICATION, "Sending: %d \n", esk8.controllerPacket.throttle);

    taskEXIT_CRITICAL(&mmux);

    logPacket( sentOK );
    calculatePacketRatio();

    if ( sentOK ) {
    	lastAckFromBoard = millis();
		updateBoardImmediately = false;
    }
    else {
		debug.print(COMMUNICATION, "tSendControllerValues_callback(): ERR_NOT_SEND_OK \n");
    }
	boardStatus.serviceState( sentOK );
}
Task tSendControllerValues(SEND_TO_BOARD_INTERVAL_MS, TASK_FOREVER, &tSendControllerValues_callback);

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
	debug.addOption(LOGGING, "LOGGING");
	//debug.setFilter( STARTUP | COMMUNICATION | ONLINE_STATUS | TIMING );
	debug.setFilter( STARTUP );// DEBUG | COMMUNICATION );// | COMMUNICATION | HARDWARE );
	//debug.setFilter( STARTUP );

	// disable speaker noise
	dacWrite(25, 0);

	M5.begin();
	M5.setWakeupButton(BUTTON_A_PIN);

	initPacketLog();

	// WiFi.mode( WIFI_OFF );	// WIFI_MODE_NULL
	// btStop();   // turn bluetooth module off

    debug.print(STARTUP, "%s\n", file_name);
	debug.print(STARTUP, "%s\n", compile_date);
	
    pixels.begin();
	
	updateBoardImmediately = true;	// initialise

	SPI.begin();                                           // Bring up the RF network
	radio.begin();
	
	timesOfflineCount = 0;
	radio.setAutoAck(true);
	esk8.begin(&radio, &network, esk8.RF24_CONTROLLER, packetAvailableCallback);

	if ( setupJoystick() == -1 ) {
		// error condition (joystick not connected)
	}
	
	setupDisplay();

	// pinMode(BUTTON_A_PIN, INPUT_PULLUP);
	// pinMode(BUTTON_C_PIN, INPUT_PULLUP);
	// byte btnC = digitalRead(BUTTON_C_PIN);
	// if (btnC == 1) {
	// 	powerDown();
	// }
	m5.update();
	if (m5.BtnC.isPressed() == false) {
		powerDown();
	}

	pushTextToMiddleOfSprite(&img_middle, "READY!", /*x*/0, /*y*/(240/2) - (img_middle.height()/2));

	int deadzone = 5;
	bool displayedMessage = false;
	while ( readJoystickOk() == false || esk8.controllerPacket.throttle > 127+deadzone || esk8.controllerPacket.throttle < 127-deadzone ) {
		if ( !displayedMessage ) {
			pushTextToMiddleOfSprite(&img_middle, "Zero throttle!", /*x*/0, /*y*/(240/2) - (img_middle.height()/2));
			debug.print(STARTUP, "Displayed message\n");
			displayedMessage = true;
		}
		vTaskDelay( 500 );
	}
	debug.print(STARTUP, "esk8.controllerPacket.throttle == %d\n", esk8.controllerPacket.throttle);

	while ( m5.BtnC.wasReleased() == false ){
		m5.update();
	}

	runner.startNow();
	runner.addTask(tFlashLeds);
	runner.addTask(tSendControllerValues);
	tSendControllerValues.enable();
	
	int core = xPortGetCoreID();

	xTaskCreatePinnedToCore (
		codeForEncoderTask,	// function
		"Task_Encoder",		// name
		10000,			// stack
		NULL,			// parameter
		2,				// priority
		&FastLEDshowTaskHandle,	// handle
		0
	);				// port	

	// has to be here after creating the above task
	ledsUpdate(pixels.Color(0, 120, 0));
}
/**************************************************************
					LOOP
**************************************************************/
long timeLastReadJoystick = 0;
long nowMs = 0;

void loop() {

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

	// if ( digitalRead(BUTTON_A_PIN) == 0 && digitalRead(BUTTON_C_PIN) == 0 ) {
	// 	powerDown();
	// }
	
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
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		// -- Do the show (synchronously)
		pixels.show();

		// -- Notify the calling task
		xTaskNotifyGive(userTaskHandle);
		//vTaskDelay( 10 );

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
			debug.print(STARTUP, "WARNING: control device is not connected!");
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
	joystickMiddle = middle;
	joystickMax = max;		
	joystickDeadZone = deadZoneSize;
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

	img_topLeft.setColorDepth(8); // Optionally set depth to 8 to halve RAM use
	img_topLeft.createSprite(SPRITE_SMALL_WIDTH, SPRITE_SMALL_HEIGHT);

	img_topRight.setColorDepth(8); // Optionally set depth to 8 to halve RAM use
	img_topRight.createSprite(SPRITE_SMALL_WIDTH, SPRITE_SMALL_HEIGHT);

	img_middle.setColorDepth(16); // Optionally set depth to 8 to halve RAM use
	img_middle.setFreeFont(FF21);                 // Select the font
	img_middle.createSprite(SPRITE_MED_WIDTH, SPRITE_MED_HEIGHT);

	img_bottomLeft.setColorDepth(8); // Optionally set depth to 8 to halve RAM use
	img_bottomLeft.createSprite(SPRITE_SMALL_WIDTH, SPRITE_SMALL_HEIGHT);

	img_bottomRight.setColorDepth(8); // Optionally set depth to 8 to halve RAM use
	img_bottomRight.createSprite(SPRITE_SMALL_WIDTH, SPRITE_SMALL_HEIGHT);
}

//--------------------------------------------------------------
// #define WIDGET_SMALL    6
// #define WIDGET_MEDIUM   12
// #define WIDGET_POS_TOP_LEFT 1
// #define WIDGET_POS_TOP_RIGHT 2
// #define WIDGET_POS_MIDDLE 3
// #define WIDGET_POS_BOTTOM_LEFT 4
// #define WIDGET_POS_BOTTOM_RIGHT 5

void updateDisplay() {
	// commsStats
	char value[5];	// xx.x\0

	char topleft[] = "123";
	char topRight[] = "456";
	char bottomleft[] = "789";
	char bottomright[] = "012";

	// populateWidget( &img_topLeft, WIDGET_SMALL, topleft);
	// img_topLeft.pushSprite(0, 0);

	// populateWidget( &img_topRight, WIDGET_SMALL, topRight);
	// img_topRight.pushSprite(320-(img_topRight.width()), 0);

	if ( millis()/1000 % 2 == 0 ) {
		bool warning = currentFailRatio > 0.1;
		int decimalRatio = currentFailRatio*100;
		sprintf( value, "%d", decimalRatio);
		// Serial.printf("%s\n", stats);
		populateMediumWidget( &img_middle, WIDGET_MEDIUM, value, "FAIL RATIO (%)", /*warning*/ warning);
	}
	else {
	// else if ( esk8.boardPacket.vescOnline ) {
		sprintf( value, "%.1f", esk8.boardPacket.batteryVoltage );
		populateMediumWidget( &img_middle, WIDGET_MEDIUM, value, "BATTERY VOLTS", /*warning*/ false);
	}
	// else {
	// 	pushTextToMiddleOfSprite(&img_middle, "VESC Offline!", /*x*/0, /*y*/(240/2) - (img_middle.height()/2));
	// }
	img_middle.pushSprite(0, (240/2) - (img_middle.height()/2));

	
	// populateWidget( &img_bottomLeft, WIDGET_SMALL, bottomleft);
	// img_bottomLeft.pushSprite(0, 240-img_bottomLeft.height());
	
	// populateWidget( &img_bottomRight, WIDGET_SMALL, bottomright);
	// img_bottomRight.pushSprite(320-(img_topRight.width()), 240-img_bottomLeft.height());
	
	// img.fillSprite(TFT_BLUE);
	// img_topLeft.setFreeFont(FF18);                 // Select the font
 //  	img_topLeft.setTextDatum(MC_DATUM);
	// img_topLeft.setTextColor(TFT_YELLOW, TFT_BLACK);
}
//--------------------------------------------------------------
void ledsUpdate(uint32_t color) {
	for (int i = 0; i < NUMPIXELS; i++){
		pixels.setPixelColor(i, color);
		vTaskDelay( 1 );
	}
	NeoPixelsShowESP32();
}
//--------------------------------------------------------------
void powerDown() {
	pushTextToMiddleOfSprite(&img_middle, "POWERING DOWN!", /*x*/0, /*y*/(240/2) - (img_middle.height()/2));
	img_middle.pushSprite(0, (240/2) - (img_middle.height()/2));
	// radio
	debug.print(STARTUP, "Powering down!\n");

	radio.stopListening();
	radio.powerDown();
	// leds
	ledsUpdate(pixels.Color(0, 0, 0));
	dacWrite(25, 0);

	delay(800);
    M5.powerOFF();
}
//--------------------------------------------------------------