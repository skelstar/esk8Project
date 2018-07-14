//************************************************************
// this is a simple example that uses the painlessMesh library
//
// 1. sends a silly message to every node on the mesh at a random time between 1 and 5 seconds
// 2. prints anything it receives to Serial.print
//
//
//************************************************************
#include "painlessMesh.h"

#define   MESH_PREFIX     "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

Scheduler userScheduler; // to control your personal task
painlessMesh  mesh;

// User stub
void sendMessage() ; // Prototype so PlatformIO doesn't complain

Task taskSendMessage( TASK_SECOND * 1 , TASK_FOREVER, &sendMessage );

int counter = 0;

void sendMessage() {
	String msg = "Controller sending: ";
	msg += counter;
	mesh.sendBroadcast( msg );
	Serial.printf("Sending message... %d \n", counter++);
	taskSendMessage.setInterval( TASK_SECOND * 1 );
}

// Needed for painless library
void receivedCallback( uint32_t from, String &msg ) {
	Serial.printf("receivedCallback: Received from %u msg=%s\n", from, msg.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
    Serial.printf("newConnectionCallback: New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
    Serial.printf("changedConnectionCallback: Changed connections %s\n",mesh.subConnectionJson().c_str());
}

void nodeTimeAdjustedCallback(int32_t offset) {
    // Serial.printf("nodeTimeAdjustedCallback: Adjusted time %u. Offset = %d\n", mesh.getNodeTime(),offset);
}

void nodeDelayReceivedCallback(uint32_t nodeId, int32_t delay) {
	Serial.printf("onNodeDelayReceived: nodeIdL %u delay %d \n", nodeId, delay);
}

void setup() {
	Serial.begin(9600);

	//mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
	mesh.setDebugMsgTypes( ERROR | STARTUP );  // set before init() so that you can see startup messages

	mesh.init( MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT );
	mesh.onReceive(&receivedCallback);
	mesh.onNewConnection(&newConnectionCallback);
	mesh.onChangedConnections(&changedConnectionCallback);
	mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
	mesh.onNodeDelayReceived(&nodeDelayReceivedCallback);

	userScheduler.addTask( taskSendMessage );
	taskSendMessage.enable();
}

void loop() {
	userScheduler.execute(); // it will run mesh scheduler as well
	mesh.update();
}