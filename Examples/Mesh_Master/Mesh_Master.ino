//************************************************************
// Based on "startHere.ino" in painlessMesh library
//************************************************************
#include <esk8Lib.h>

// #define   MESH_SSID       "whateverYouLike"
// #define   MESH_PASSWORD   "somethingSneaky"
// #define   MESH_PORT       5555

//--------------------------------------------------------------

#define ROLE_MASTER		1
#define ROLE_SLAVE		0

//--------------------------------------------------------------

esk8Lib esk8;

// Scheduler     userScheduler; // to control your personal task

//--------------------------------------------------------------

void setup() {
	Serial.begin(9600);

	//mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
	//mesh.setDebugMsgTypes(ERROR | DEBUG | CONNECTION | COMMUNICATION);  // set before init() so that you can see startup messages
	mesh.setDebugMsgTypes( ERROR | DEBUG | CONNECTION );  // set before init() so that you can see startup messages

	mesh.init(MESH_SSID, MESH_PASSWORD, &userScheduler, MESH_PORT);
	mesh.onReceive(&receivedCallback);
	mesh.onNodeDelayReceived(&delayReceivedCallback);

	esk8.begin(ROLE_MASTER);

}

void loop() {
	esk8.serviceEvents();
	// userScheduler.execute(); // it will run mesh scheduler as well
	// mesh.update();
}
