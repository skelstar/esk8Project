//************************************************************
// Based on "startHere.ino" in painlessMesh library
//************************************************************
#include <painlessMesh.h>

#include <Rotary.h>
#include <FastLED.h>

#include <myPushButton.h>
#include <debugHelper.h>
#include <myPowerButtonManager.h>


#define   MESH_SSID       "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

//--------------------------------------------------------------

#define ROLE_MASTER		1
#define ROLE_SLAVE		0

//int role = ROLE_MASTER;
int role = ROLE_SLAVE;

int sendIntervalMs = 200;

//--------------------------------------------------------------

#define ENCODER_BUTTON_PIN	34
#define ENCODER_PIN_A		4
#define ENCODER_PIN_B		16
	
#define DEADMAN_SWITCH_PIN	25

#define	PIXEL_PIN			5

//--------------------------------------------------------------

debugHelper debug;

Rotary rotary = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);

//--------------------------------------------------------------------------------

#define 	OFF_STATE_HIGH		HIGH
#define 	OFF_STATE_LOW       0
#define 	PULLUP              true

void listener_deadmanSwitch( int eventCode, int eventPin, int eventParam );
myPushButton deadmanSwitch(DEADMAN_SWITCH_PIN, PULLUP, OFF_STATE_HIGH, listener_deadmanSwitch);
void listener_deadmanSwitch( int eventCode, int eventPin, int eventParam ) {

	switch (eventCode) {

		case deadmanSwitch.EV_BUTTON_PRESSED:
			// if (esk8.slavePacket.throttle > 127) {
			// 	//zeroThrottle();
			// }
			debug.print(d_DEBUG, "EV_BUTTON_PRESSED (DEADMAN) \n");
			break;
		
		case deadmanSwitch.EV_RELEASED:
			// if (esk8.slavePacket.throttle > 127) {
			// 	//zeroThrottle();
			// }
			debug.print(d_DEBUG, "EV_BUTTON_RELEASED (DEADMAN) \n");
			break;
		
		case deadmanSwitch.EV_HELD_SECONDS:
			//Serial.printf("EV_BUTTON_HELD (DEADMAN): %d \n", eventParam);
			break;
	}
}

// lower number = more coarse
#define ENCODER_COUNTER_MIN	-18 	// decceleration (ie -20 divides 0-127 into 20)
#define ENCODER_COUNTER_MAX	12 		// acceleration (ie 15 divides 127-255 into 15)

#define DEADMAN_SWITCH_USED		1

//--------------------------------------------------------------

int encoderCounter = 0;
bool encoderChanged = false;
volatile bool packetReadyToBeSent = false;

void encoderInterruptHandler() {
	unsigned char result = rotary.process();

	bool canAccelerate = deadmanSwitch.isPressed();	// || DEADMAN_SWITCH_USED == 0;

	if (result == DIR_CW && (canAccelerate || encoderCounter < 0)) {
		if (encoderCounter < ENCODER_COUNTER_MAX) {

			encoderCounter++;
			//int throttleValue = getThrottleValue();
			//esk8.updateSlavePacket(throttleValue);
			packetReadyToBeSent = true;
			debug.print(d_DEBUG, "encoderCounter: %d \n", encoderCounter);
		}
	}
	else if (result == DIR_CCW) {
		if (encoderCounter > ENCODER_COUNTER_MIN) {
			encoderCounter--;
			//int throttleValue = getThrottleValue();
			//esk8.updateSlavePacket(throttleValue);
			packetReadyToBeSent = true;
			debug.print(d_DEBUG, "encoderCounter: %d \n", encoderCounter);
		}
	}
}

//--------------------------------------------------------------------------------

#define 	NUM_PIXELS 		8

CRGB leds[NUM_PIXELS];

#define BRIGHTNESS	20

CRGB COLOUR_OFF = CRGB::Black;
CRGB COLOUR_RED = CRGB::Red;
CRGB COLOUR_GREEN = CRGB::Green;
CRGB COLOUR_BLUE = CRGB::Blue;
CRGB COLOUR_WHITE = CRGB::White;

//--------------------------------------------------------------

// Prototypes
void sendMessage(); 

bool calc_delay = false;

//--------------------------------------------------------------

painlessMesh  mesh;

Scheduler     userScheduler; // to control your personal task

// void sendMessage() ; // Prototype
Task taskSendMessage( TASK_MILLISECOND * 1, TASK_FOREVER, &sendMessage ); // start with a one second interval

volatile uint32_t otherNode;
volatile long lastRxMillis = 0;

//--------------------------------------------------------------
void sendMessage() {
	String msg = "throttle:";
	msg +=  encoderCounter;
	mesh.sendBroadcast(msg);

	if (calc_delay) {
		mesh.startDelayMeas(otherNode);
	}

	debug.print(d_COMMUNICATION, "Sending message: '%s'\n", msg.c_str());

	taskSendMessage.setInterval( sendIntervalMs );
}
//--------------------------------------------------------------
void receivedCallback(uint32_t from, String & msg) {
	otherNode = from;
	//debug.print(d_COMMUNICATION, "Received msg=%s, took: %ums\n", otherNode, msg.c_str(), millis()-lastRxMillis);
	//Serial.printf("Received msg=%s, took: %ums\n", otherNode, msg.c_str(), millis()-lastRxMillis);
	debug.print(d_COMMUNICATION, "Received msg=%s, took: %ums\n", msg.c_str(), millis()-lastRxMillis);
	lastRxMillis = millis();
}
//--------------------------------------------------------------
void delayReceivedCallback(uint32_t from, int32_t delay) {
	Serial.printf("Delay to node %u is %d us\n", from, delay);
}
//--------------------------------------------------------------

myPowerButtonManager powerButton(ENCODER_BUTTON_PIN, HIGH, 3000, 3000, powerupEvent);

void powerupEvent(int state) {

	switch (state) {
		case powerButton.TN_TO_POWERING_UP:
			debug.print(d_STARTUP, "TN_TO_POWERING_UP \n");
			//setPixels(COLOUR_GREEN, 0);
			break;
		case powerButton.TN_TO_POWERED_UP_WAIT_RELEASE:
			debug.print(d_STARTUP, "TN_TO_POWERED_UP_WAIT_RELEASE \n");
			// skip this and go straighht to RUNNING
			powerButton.setState(powerButton.TN_TO_RUNNING);
			break;
		case powerButton.TN_TO_RUNNING:
			//setPixels(COLOUR_OFF, 0);
			debug.print(d_STARTUP, "TN_TO_RUNNING\n");
			break;
		case powerButton.TN_TO_POWERING_DOWN:
			debug.print(d_STARTUP, "TN_TO_POWERING_DOWN \n");
			//setPixels(COLOUR_GREEN, 0);
			break;
		case powerButton.TN_TO_POWERING_DOWN_WAIT_RELEASE:
			debug.print(d_STARTUP, "TN_TO_POWERING_DOWN_WAIT_RELEASE \n");
			// u8g2.clearBuffer();
			// u8g2.sendBuffer();	// clear screen
			// setPixels(COLOUR_OFF, 0);
			powerButton.setState(powerButton.TN_TO_POWER_OFF);
			break;
		case powerButton.TN_TO_POWER_OFF:
			debug.print(d_STARTUP, "TN_TO_POWER_OFF \n");
			//setPixels(COLOUR_OFF, 0);
			delay(100);
			esp_deep_sleep_start();
			Serial.println("This will never be printed");
			break;
	}
}

//--------------------------------------------------------------------------------

void setup() {

	debug.init(d_DEBUG | d_STARTUP | d_COMMUNICATION);

	FastLED.addLeds<NEOPIXEL, PIXEL_PIN>(leds, NUM_PIXELS);

	Serial.begin(9600);

	powerButton.begin();

	//mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
	//mesh.setDebugMsgTypes(ERROR | DEBUG | CONNECTION | COMMUNICATION);  // set before init() so that you can see startup messages
	mesh.setDebugMsgTypes( ERROR | STARTUP| DEBUG | CONNECTION );  // set before init() so that you can see startup messages

	mesh.init(MESH_SSID, MESH_PASSWORD, &userScheduler, MESH_PORT, AP_ONLY);
	mesh.onReceive(&receivedCallback);
	mesh.onNodeDelayReceived(&delayReceivedCallback);

	// encoder
	setupEncoder();
}

void loop() {

	userScheduler.execute(); // it will run mesh scheduler as well
	mesh.update();
	deadmanSwitch.serviceEvents();

	powerButton.serviceButton(true);

	if (packetReadyToBeSent) {
		packetReadyToBeSent = false;
		sendMessage();
	}
}
//----------------------------------------------------------------
void setupEncoder() {

	pinMode(ENCODER_PIN_A, INPUT_PULLUP);
	pinMode(ENCODER_PIN_B, INPUT_PULLUP);

	attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), encoderInterruptHandler, CHANGE);
	attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), encoderInterruptHandler, CHANGE);

	// esk8.slavePacket.throttle = getThrottleValue();
}
//--------------------------------------------------------------------------------
void setPixels(CRGB c, uint8_t wait) {
	for (uint16_t i=0; i<NUM_PIXELS; i++) {
		leds[i] = c;
		if (wait > 0) {
			FastLED.show();
			delay(wait);
		}
		mesh.update();
	}
	FastLED.show();
}
//--------------------------------------------------------------------------------
