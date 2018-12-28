#include "Arduino.h"
#include "esk8Lib.h"

#define 	ADDR_INDEX_LISTENER	2
#define 	ADDR_INDEX_CONTROLLER	1
#define 	ADDR_INDEX_BOARD		0

#define 	CONTROLLER_SEND_INTERVAL	400 + 50
#define 	BOARD_SEND_INTERVAL			4000

#define 	MSG_CONTROLLER_PACKET 		'A'
#define 	MSG_BOARD_PACKET			'B'

//--------------------------------------------------------------------------------

esk8Lib::esk8Lib() {}

//--------------------------------------------------------------------------------
void esk8Lib::begin(RF24 *radio, bool isMaster) {	

	_radio = radio;

	// _radio.begin();
	// // radio.setAutoAck(1);                    // Ensure autoACK is enabled
	// // radio.enableAckPayload();               // Allow optional ack payloads
	// _radio.setRetries(0,15);                 // Smallest time between retries, max no. of retries
	// _radio.setPayloadSize(1);                // Here we are sending 1-byte payloads to test the call-response speed
	// _radio.openWritingPipe(pipes[0]);        // Both radios listen on the same pipes by default, and switch when writing
	// _radio.openReadingPipe(1,pipes[1]);
	// _radio.startListening();                 // Start listening
	// _radio.printDetails();                   // Dump the configuration of the rf unit for debugging

	Serial.printf("this_node: 0%o, _other_node: 0%o \n", _this_node, _other_node);

	controllerPacket.throttle = 127;
	controllerPacket.id = 0;
	boardPacket.id = 0;
	hudReqPacket.id = 0;
}
//---------------------------------------------------------------------------------
void esk8Lib::service() {
}
//---------------------------------------------------------------------------------
int esk8Lib::send() {
}
//---------------------------------------------------------------------------------
void esk8Lib::handle_Controller_Message(RF24NetworkHeader& header) {
}
//---------------------------------------------------------------------------------
void esk8Lib::handle_Board_Message(RF24NetworkHeader& header) {
}
//---------------------------------------------------------------------------------
int esk8Lib::controllerOnline() {
}
//---------------------------------------------------------------------------------
int esk8Lib::boardOnline() {
}
//---------------------------------------------------------------------------------
void esk8Lib::enableDebug() {
}
//---------------------------------------------------------------------------------
