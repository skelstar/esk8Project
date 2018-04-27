//************************************************************
// Based on "startHere.ino" in painlessMesh library
//************************************************************
#include <painlessMesh.h>

#define   MESH_SSID       "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

// Prototypes
void sendMessage(); 

bool calc_delay = true;
SimpleList<uint32_t> nodes;

//--------------------------------------------------------------

painlessMesh  mesh;

Scheduler     userScheduler; // to control your personal task

// void sendMessage() ; // Prototype
Task taskSendMessage( TASK_SECOND * 1, TASK_FOREVER, &sendMessage ); // start with a one second interval

volatile uint32_t otherNode;

//--------------------------------------------------------------
void sendMessage() {
	String msg = "Hello from node ";
	msg += mesh.getNodeId();
	mesh.sendBroadcast(msg);

	if (calc_delay) {
		// SimpleList<uint32_t>::iterator node = nodes.begin();
		// while (node != nodes.end()) {
			mesh.startDelayMeas(otherNode);
			// node++;
		// }
		//calc_delay = false;
	}

	Serial.printf("Sending message: %s\n", msg.c_str());

	taskSendMessage.setInterval( random(TASK_SECOND * 1, TASK_SECOND * 5));  // between 1 and 5 seconds
}
//--------------------------------------------------------------
void receivedCallback(uint32_t from, String & msg) {
	otherNode = from;
	Serial.printf("startHere: Received from %u msg=%s\n", otherNode, msg.c_str());
}
//--------------------------------------------------------------
void newConnectionCallback(uint32_t nodeId) {
	// Reset blink task
	Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
}
//--------------------------------------------------------------
void changedConnectionCallback() {
	// Serial.printf("Changed connections %s\n", mesh.subConnectionJson().c_str());
	// // Reset blink task
 
	// nodes = mesh.getNodeList();

	// Serial.printf("Num nodes: %d\n", nodes.size());
	// Serial.printf("Connection list:");

	// SimpleList<uint32_t>::iterator node = nodes.begin();
	// while (node != nodes.end()) {
	// 	Serial.printf(" %u", *node);
	// 	node++;
	// }
	// Serial.println();
	// // calc_delay = true;
}
//--------------------------------------------------------------
void nodeTimeAdjustedCallback(int32_t offset) {
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
	mesh.onNewConnection(&newConnectionCallback);
	mesh.onChangedConnections(&changedConnectionCallback);
	// mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
	mesh.onNodeDelayReceived(&delayReceivedCallback);

	userScheduler.addTask( taskSendMessage );
	taskSendMessage.enable();
}

void loop() {
	userScheduler.execute(); // it will run mesh scheduler as well
	mesh.update();
}
