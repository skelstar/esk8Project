//************************************************************
// Based on "startHere.ino" in painlessMesh library
//************************************************************
#include <painlessMesh.h>
#include <debugHelper.h>
#include <Rotary.h>

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

#define ENCODER_BUTTON_PIN		34
#define ENCODER_PIN_A		4
#define ENCODER_PIN_B		16

debugHelper debug;

Rotary rotary = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);

// lower number = more coarse
#define ENCODER_COUNTER_MIN	-18 	// decceleration (ie -20 divides 0-127 into 20)
#define ENCODER_COUNTER_MAX	12 		// acceleration (ie 15 divides 127-255 into 15)

#define DEADMAN_SWITCH_USED		1

int encoderCounter = 0;
bool encoderChanged = false;
volatile bool packetReadyToBeSent = false;

void encoderInterruptHandler() {
	unsigned char result = rotary.process();

	bool canAccelerate = true;	// deadmanSwitch.isPressed() || DEADMAN_SWITCH_USED == 0;

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

//--------------------------------------------------------------

// Prototypes
void sendMessage(); 

bool calc_delay = true;
SimpleList<uint32_t> nodes;
float packetData = 0.1;

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

	packetData += 0.1;

	if (calc_delay) {
		mesh.startDelayMeas(otherNode);
	}

	debug.print(d_COMMUNICATION, "Sending message: '%s'\n", msg.c_str());

	taskSendMessage.setInterval( sendIntervalMs );
}
//--------------------------------------------------------------
void receivedCallback(uint32_t from, String & msg) {
	otherNode = from;
	Serial.printf("startHere: Received from %u msg=%s since: %ums\n", otherNode, msg.c_str(), millis()-lastRxMillis);
	lastRxMillis = millis();
}
//--------------------------------------------------------------
void delayReceivedCallback(uint32_t from, int32_t delay) {
	Serial.printf("Delay to node %u is %d us\n", from, delay);
}
//--------------------------------------------------------------

void setup() {

	debug.init(d_DEBUG | d_STARTUP | d_COMMUNICATION);

	Serial.begin(9600);

	//mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
	//mesh.setDebugMsgTypes(ERROR | DEBUG | CONNECTION | COMMUNICATION);  // set before init() so that you can see startup messages
	mesh.setDebugMsgTypes( ERROR | DEBUG | CONNECTION );  // set before init() so that you can see startup messages

	mesh.init(MESH_SSID, MESH_PASSWORD, &userScheduler, MESH_PORT);
	mesh.onReceive(&receivedCallback);
	mesh.onNodeDelayReceived(&delayReceivedCallback);

	// encoder
	setupEncoder();
}

void loop() {
	userScheduler.execute(); // it will run mesh scheduler as well
	mesh.update();

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
