//************************************************************
// Based on "startHere.ino" in painlessMesh library
//************************************************************
#include <painlessMesh.h>

#define   MESH_SSID       "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

//--------------------------------------------------------------

#define ROLE_MASTER		1
#define ROLE_SLAVE		0

int role = ROLE_MASTER;
//int role = ROLE_SLAVE;

int sendIntervalMs = 200;

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
	String msg = "Payload = ";
	msg +=  packetData;
	mesh.sendBroadcast(msg);

	packetData += 0.1;

	if (calc_delay) {
		mesh.startDelayMeas(otherNode);
	}

	Serial.printf("Sending message: %s\n", msg.c_str());

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
	Serial.begin(9600);

	//mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
	//mesh.setDebugMsgTypes(ERROR | DEBUG | CONNECTION | COMMUNICATION);  // set before init() so that you can see startup messages
	mesh.setDebugMsgTypes( ERROR | DEBUG | CONNECTION );  // set before init() so that you can see startup messages

	mesh.init(MESH_SSID, MESH_PASSWORD, &userScheduler, MESH_PORT);
	mesh.onReceive(&receivedCallback);
	mesh.onNodeDelayReceived(&delayReceivedCallback);

	userScheduler.addTask( taskSendMessage );

	if (role == ROLE_MASTER) {
		taskSendMessage.enable();
	}
}

void loop() {
	userScheduler.execute(); // it will run mesh scheduler as well
	mesh.update();
}
