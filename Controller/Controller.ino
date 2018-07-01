// #include <SPI.h>
// #include "RF24.h"
#include <Rotary.h>
// #include <FastLED.h>

#include <TaskScheduler.h>

#include <myPushButton.h>
#include <debugHelper.h>
// #include <esk8Lib.h>

#include "BLEDevice.h"
#include <ESP8266VESC.h>
#include "VescUart.h"

//--------------------------------------------------------------

#define	STARTUP 		1 << 0
#define DEBUG 			1 << 1
#define COMMUNICATION 	1 << 2
#define HARDWARE		1 << 3
#define BLE 			1 << 4
#define ONLINE_STATUS	1 << 5

debugHelper debug;

//--------------------------------------------------------------

ESP8266VESC esp8266VESC = ESP8266VESC();

static BLEUUID serviceUUID("0000ffe0-0000-1000-8000-00805f9b34fb");
static BLEUUID    charUUID("0000ffe1-0000-1000-8000-00805f9b34fb");

static BLEAddress *pServerAddress;
static boolean doConnect = false;
static boolean connected = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
//--------------------------------------------------------------
static void notifyCallback(
	BLERemoteCharacteristic* pBLERemoteCharacteristic,
	uint8_t* pData,
	size_t length,
	bool isNotify) {
	    debug.print(BLE, "----------- N O T I F Y -----------\n");
		esp8266VESC.receivePacket(pData, length);
}
//--------------------------------------------------------------
bool connectToServer(BLEAddress pAddress) {
	debug.print(BLE, "Connecting...\n");
    BLEClient *pClient  = BLEDevice::createClient();
    pClient->connect(pAddress);
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
		// Serial.print("Failed to find our service UUID: ");
		return false;
    }
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
		// Serial.print("Failed to find our characteristic UUID: ");
		return false;
    }
    pRemoteCharacteristic->registerForNotify(notifyCallback);
}
//--------------------------------------------------------------
/*Scan for BLE servers and find the first one that advertises the service we are looking for. */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
	/* Called for each advertising BLE server. */
  	void onResult(BLEAdvertisedDevice advertisedDevice) {
		if (advertisedDevice.haveServiceUUID() && advertisedDevice.getServiceUUID().equals(serviceUUID)) {
			debug.print(BLE, "Found VESC...\n");
			advertisedDevice.getScan()->stop();

			pServerAddress = new BLEAddress(advertisedDevice.getAddress());
			doConnect = true;
		} // Found our server
  	}
};
//--------------------------------------------------------------------------------

const char compile_date[] = __DATE__ " " __TIME__;

int getThrottleValue(int raw);
void setupEncoder();
// void setPixels(CRGB c) ;
void zeroThrottleReadyToSend();

//--------------------------------------------------------------------------------

#define ENCODER_BUTTON_PIN	34	// 36 didn't work
#define ENCODER_PIN_A		16
#define ENCODER_PIN_B		4

#define DEADMAN_SWITCH_PIN	25

#define	PIXEL_PIN			5

//--------------------------------------------------------------

int sendIntervalMs = 200;

int throttle = 127;

//--------------------------------------------------------------

Rotary rotary = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);

//--------------------------------------------------------------

#define 	NUM_PIXELS 		8

// CRGB leds[NUM_PIXELS];

// #define BRIGHTNESS	20

// CRGB COLOUR_OFF = CRGB::Black;
// CRGB COLOUR_RED = CRGB::Red;
// CRGB COLOUR_GREEN = CRGB::Green;
// CRGB COLOUR_BLUE = CRGB::Blue;
// CRGB COLOUR_PINK = CRGB::Pink;

// CRGB COLOUR_ACCELERATING = CRGB::Navy;
// CRGB COLOUR_BRAKING = CRGB::Crimson;
// CRGB COLOUR_THROTTLE_IDLE = CRGB::Green;

// CRGB COLOUR_WHITE = CRGB::White;

//--------------------------------------------------------------
#define	TN_ONLINE 	1
#define	ST_ONLINE 	2
#define	TN_OFFLINE  3
#define	ST_OFFLINE  4

class OnlineStatus 
{
	private:
		uint8_t state = ST_ONLINE;
		uint8_t oldstate = ST_ONLINE;

	public:

		bool serviceState(bool online) {
			switch (state) {
				case TN_ONLINE:
					debug.print(ONLINE_STATUS, "-> TN_ONLINE \n");
					state = online ? ST_ONLINE : TN_OFFLINE;
					break;
				case ST_ONLINE:
					// lastControllerOnlineTime = millis();
					state = online ? ST_ONLINE : TN_OFFLINE;
					break;
				case TN_OFFLINE:
					debug.print(ONLINE_STATUS, "-> TN_OFFLINE \n");
					state = online ? TN_ONLINE : ST_OFFLINE;
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

		bool getState() {
			return state;
		}
};

OnlineStatus vescCommsStatus;

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
			if (throttle > 127) {
			 	zeroThrottleReadyToSend();
			}
			// //debug.print(HARDWARE, "EV_BUTTON_RELEASED (DEADMAN) \n");
			break;
	}
}

void listener_encoderButton( int eventCode, int eventPin, int eventParam );
myPushButton encoderButton(ENCODER_BUTTON_PIN, PULLUP, OFF_STATE_HIGH, listener_encoderButton);
void listener_encoderButton( int eventCode, int eventPin, int eventParam ) {

	switch (eventCode) {

		case encoderButton.EV_BUTTON_PRESSED:
			break;

		case encoderButton.EV_RELEASED:
            zeroThrottleReadyToSend();
			break;

		case encoderButton.EV_HELD_SECONDS:
			break;
	}
}
//--------------------------------------------------------------
// lower number = more coarse
#define ENCODER_COUNTER_MIN	-18 	// decceleration (ie -20 divides 0-127 into 20)
#define ENCODER_COUNTER_MAX	10 		// acceleration (ie 15 divides 127-255 into 15)

int encoderCounter = 0;

//--------------------------------------------------------------------------------

Scheduler runner;

#define FAST_FLASH_DURATION     300

void tFastFlash_callback();
Task tFastFlash(FAST_FLASH_DURATION, 2, &tFastFlash_callback);
void tFastFlash_callback() {
    if (tFastFlash.isLastIteration()) {
        //setPixels(COLOUR_OFF);
        tFastFlash.disable();
    }
}

// void fastFlashLed(CRGB c) {
// 	setPixels(c);
//     tFastFlash.setIterations(2);
//     tFastFlash.enable();
// }

// void fastFlashLed() {
//     tFastFlash.setIterations(2);
//     tFastFlash.enable();
// }

//------------------------------------------------------------------------------
bool tFlashLeds_onEnable();
void tFlashLedsOn_callback();
void tFlashLedsOff_callback();
// CRGB tFlashLedsColour = COLOUR_RED;

Task tFlashLeds(500, TASK_FOREVER, &tFlashLedsOff_callback);

bool tFlashLeds_onEnable() {
	// setPixels(tFlashLedsColour);
	tFlashLeds.enable();
    return true;
}
void tFlashLedsOn_callback() {
	tFlashLeds.setCallback(&tFlashLedsOff_callback);
	// setPixels(tFlashLedsColour);
}
void tFlashLedsOff_callback() {
	tFlashLeds.setCallback(&tFlashLedsOn_callback);
	// setPixels(COLOUR_OFF);
}
// //--------------------------------------------------------------
// void tSendControllerValues_callback() {
// 	sendThrottleToVESC();
// }
// Task tSendControllerValues(SEND_TO_BOARD_INTERVAL_MS, TASK_FOREVER, &tSendControllerValues_callback);
//------------------------------------------------------------------------------

void encoderInterruptHandler() {
	unsigned char result = rotary.process();

	bool canAccelerate = deadmanSwitch.isPressed();

	// if (result == DIR_CW) {
	// 	debug.print(HARDWARE, "DIR_CW %d \n", encoderCounter);		
	// }
	// else if (result == DIR_CCW) {
	// 	debug.print(HARDWARE, "DIR_CCW %d \n", encoderCounter);
	// }

	if (result == DIR_CW && (canAccelerate || encoderCounter < 0)) {
		if (encoderCounter < ENCODER_COUNTER_MAX) {
			encoderCounter++;
			throttle = getThrottleValue(encoderCounter);
			debug.print(HARDWARE, "encoderCounter: %d, throttle: %d \n", encoderCounter, throttle);
		}
	}
	else if (result == DIR_CCW) {
		if (encoderCounter > ENCODER_COUNTER_MIN) {
			encoderCounter--;
			throttle = getThrottleValue(encoderCounter);
			debug.print(HARDWARE, "encoderCounter: %d, throttle: %d \n", encoderCounter, throttle);
		}
	}
}

//--------------------------------------------------------------

volatile uint32_t otherNode;
volatile long lastRxMillis = 0;
#define COMMS_TIMEOUT_PERIOD 	1000

//--------------------------------------------------------------
void sendThrottleToVESC() {
}
//--------------------------------------------------------------------------------

/**************************************************************
					SETUP
**************************************************************/
void setup() {

	Serial.begin(9600);

	debug.init();
	debug.addOption(STARTUP, "STARTUP");
	debug.addOption(DEBUG, "DEBUG");
	debug.addOption(HARDWARE, "HARDWARE");
	debug.addOption(BLE, "BLE");
	debug.addOption(ONLINE_STATUS, "ONLINE_STATUS");
    debug.setFilter( STARTUP | HARDWARE | BLE | ONLINE_STATUS );	// DEBUG | STARTUP | COMMUNICATION | ERROR | HARDWARE);

	//debug.print(STARTUP, "%s \n", compile_date);
    //debug.print(STARTUP, "esk8Project/Controller.ino \n");

	// FastLED.addLeds<NEOPIXEL, PIXEL_PIN>(leds, NUM_PIXELS);
	// FastLED.show();
	BLEDevice::init("ESP32 BLE Client");

    //debug.print(STARTUP, "BLE init \n");

	BLEScan* pBLEScan = BLEDevice::getScan();
	pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
	pBLEScan->setActiveScan(true);
	debug.print(BLE, "Scanning... \n");
	pBLEScan->start(30);

	runner.startNow();
    runner.addTask(tFastFlash);
	runner.addTask(tFlashLeds);
    //debug.print(STARTUP, "runner \n");

	xTaskCreatePinnedToCore (
		codeForEncoderTask,	// function
		"Task_Encoder",		// name
		10000,			// stack
		NULL,			// parameter
		1,				// priority
		NULL,	// handle
		0);				// port	
}
/**************************************************************
					LOOP
**************************************************************/
long nowms = 0;

void loop() {

	if (doConnect == true) {
		if (connectToServer(*pServerAddress)) {
			debug.print(BLE, "We are now connected to the BLE Server.");
			connected = true;
		} else {
			debug.print(BLE, "We have failed to connect to the server; there is nothin more we will do.");
		}
		doConnect = false;
	}

	// vescCommsStatus.serviceState(connected);
	// bool vescOnline = vescCommsStatus.getState() == ST_ONLINE;

	if (connected) {

		if (millis() - nowms > 5000) {
			nowms = millis();
			debug.print(BLE, "======================================================\n");
			sendGetValuesRequest();
		}

		// if (accelDecelState == ACCEL_DECEL_STATE_IDLE || accelDecelState == ACCEL_DECEL_STATE_DECEL) {
		// 	if (millis()-accelDEcelMs > 3000) {
		// 		accelDEcelMs = millis();
		// 		accelDecelState = ACCEL_DECEL_STATE_ACCEL;
		// 		sendNunchukValues(127+15);
		// 	}
		// }
		// else {
		// 	if (millis()-accelDEcelMs > 1000) {
		// 		accelDEcelMs = millis();
		// 		accelDecelState = ACCEL_DECEL_STATE_DECEL;
		// 		sendNunchukValues(127);
		// 	}
		// }

	}


	delay(10);
}
/**************************************************************
					TASK 0
**************************************************************/
void codeForEncoderTask( void *parameter ) {

	long task0now = 0;

    // //debug.print(STARTUP, "codeForEncoderTask \n");

	setupEncoder();

	// then loop forever	
	for (;;) {
		encoderButton.serviceEvents();
		deadmanSwitch.serviceEvents();
		delay(10);
	}

	vTaskDelete(NULL);
}
//**************************************************************
void setupEncoder() {

	pinMode(ENCODER_PIN_A, INPUT_PULLUP);
	pinMode(ENCODER_PIN_B, INPUT_PULLUP);

	attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), encoderInterruptHandler, CHANGE);
	attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), encoderInterruptHandler, CHANGE);
}
//--------------------------------------------------------------
void sendGetValuesRequest() {

	esp8266VESC.composeGetValuesRequest();

	uint8_t* payload = esp8266VESC.getCommandPayload();
	uint8_t payloadLength = esp8266VESC.getCommandPayloadLength();

	pRemoteCharacteristic->writeValue(payload, payloadLength);
}
//--------------------------------------------------------------------------------
// void setPixels(CRGB c) {
// 	// for (uint16_t i=0; i<NUM_PIXELS; i++) {
// 	// 	leds[i] = c;
// 	// 	leds[i] /= 10;
// 	// }
// 	// FastLED.show();
// }
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
	throttle = 127;
    //debug.print(HARDWARE, "encoderCounter: %d, throttle: %d [ZERO] \n", encoderCounter, throttle);
}
//--------------------------------------------------------------
#define ONLINE_SYMBOL_WIDTH 	14
