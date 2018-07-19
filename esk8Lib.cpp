#include "Arduino.h"
#include "esk8Lib.h"

#define 	ROLE_LISTENER	2
#define 	ROLE_CONTROLLER	1
#define 	ROLE_BOARD		0

#define 	CONTROLLER_SEND_INTERVAL	400 + 50
#define 	BOARD_SEND_INTERVAL			4000

const uint16_t node_address_set[10] = { 00, 02, 05, 012, 015, 022, 025, 032, 035, 045 };

//--------------------------------------------------------------------------------

esk8Lib::esk8Lib() {}

//--------------------------------------------------------------------------------
void esk8Lib::begin(RF24 *radio, RF24Network *network, int role) {	//, debugHelper *debug) {

	_radio = radio;
	_network = network;
	_role = role;

	//_radio->setPALevel(RF24_PA_MAX);

	this_node = node_address_set[role]; // Which node are we?

	_network->begin( /*channel*/ 100, /*node address*/ this_node);
	
	_radio->printDetails();

	controllerPacket.throttle = 127;
}
//---------------------------------------------------------------------------------
int esk8Lib::available() {
	
	_network->update();
	
	while (_network->available()) {
	
		RF24NetworkHeader header; // If so, take a look at it
        _network->peek(header);

        switch (header.type) { // Dispatch the message to the correct handler.
			case 'A':	// message from Controller
				_lastControllerPacketReadTime = millis();
				handle_Controller_Message(header);
				return true;
			case 'B':	// message from Board
				_lastBoardPacketReadTime = millis();
				handle_Board_Message(header);
				return true;
	        default:
				Serial.printf("*** WARNING *** Unknown message type %c\n\r", header.type);
				network.read(header, 0, 0);
				return false;
        };
	}
	return false;
}
//---------------------------------------------------------------------------------
void esk8Lib::handle_Controller_Message(RF24NetworkHeader& header) {
    network.read(header, &throttle, sizeof(throttle));
    Serial.printf("%lu: APP Received message from CONTROLLER \n\r ", millis());
}
//---------------------------------------------------------------------------------
void esk8Lib::handle_Board_Message(RF24NetworkHeader& header) {
    network.read(header, &throttle, sizeof(throttle));
    Serial.printf("%lu: APP Received message from BOARD\n\r ", millis());
}
//---------------------------------------------------------------------------------
int esk8Lib::controllerOnline() {
	return (millis()-_lastControllerPacketReadTime) < CONTROLLER_SEND_INTERVAL;
}
//---------------------------------------------------------------------------------
int esk8Lib::boardOnline() {
	return (millis()-_lastBoardPacketReadTime) < CONTROLLER_SEND_INTERVAL;
}
//---------------------------------------------------------------------------------