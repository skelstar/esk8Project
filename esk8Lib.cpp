#include "Arduino.h"
#include "esk8Lib.h"

#define 	ADDR_INDEX_LISTENER	2
#define 	ADDR_INDEX_CONTROLLER	1
#define 	ADDR_INDEX_BOARD		0

#define 	CONTROLLER_SEND_INTERVAL	400 + 50
#define 	BOARD_SEND_INTERVAL			4000

const uint16_t node_address_set[10] = { 00, 02, 05, 012, 015, 022, 025, 032, 035, 045 };

//--------------------------------------------------------------------------------

esk8Lib::esk8Lib() {}

//--------------------------------------------------------------------------------
void esk8Lib::begin(RF24 *radio, RF24Network *network, RoleType role) {	//, debugHelper *debug) {

	_radio = radio;
	_network = network;
	_role = role;

	//_radio->setPALevel(RF24_PA_MAX);

	if (role == CONTROLLER) {
		_this_node = node_address_set[ADDR_INDEX_CONTROLLER]; // Which node are we?
		_other_node = node_address_set[ADDR_INDEX_BOARD];
		Serial.printf("this_node: 0%o, other_node: 0%o \n", _this_node, _other_node);
	}
	else if (role == BOARD) {
		_this_node = node_address_set[ADDR_INDEX_BOARD]; // Which node are we?
		_other_node = node_address_set[ADDR_INDEX_CONTROLLER];
		Serial.printf("this_node: 0%o, other_node: 0%o \n", _this_node, _other_node);
	}
	else if (role == LISTENER) {
		_this_node = node_address_set[ADDR_INDEX_LISTENER]; // Which node are we?
		// _other_node = node_address_set[ADDR_INDEX_CONTROLLER];
	}
	else {
		Serial.printf("Unhandled roletype in begin: '%d' \n", role);
	}

	_network->begin( /*channel*/ 100, /*node address*/ _this_node);
	_network->multicastLevel(1);
	
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
				_lastControllerCommsTime = millis();
				handle_Controller_Message(header);
				return true;
			case 'a':	// acknowledge of 'A'
				_lastBoardCommsTime = millis();
				return false;
			case 'B':	// message from Board
				_lastBoardCommsTime = millis();
				handle_Board_Message(header);
				return true;
			case 'b': 	// acknowledge of 'B'
				_lastControllerCommsTime = millis();
				return false;
	        default:
				Serial.printf("*** WARNING *** Unknown message type '%c' \n\r", header.type);
				_network->read(header, 0, 0);
				return false;
        };

		uint32_t fails, success;  
		_network->failures(&fails,&success);  	
		Serial.printf("fails: %d success: %d \n", fails, success);
	}
	return false;
}
//---------------------------------------------------------------------------------
int esk8Lib::send(char messageType) {
	if (_role == CONTROLLER) {
		if (messageType == 'A') {
    		// Serial.printf("%lu: APP Sending &controllerPacket to 0%o... \n\r", millis(), _other_node);
			RF24NetworkHeader header( _other_node, 'A' );
			_lastSentToBoard = millis();
			return _network->multicast( header, &controllerPacket, sizeof(controllerPacket), /* level */ 1 );
		} else {
			Serial.printf("Unhandled messageType in send() %c \n", messageType);
		}
	}
	else if (_role == BOARD) {
		if (messageType == 'B') {
			RF24NetworkHeader header( _other_node, 'B' );
			_lastSentToController = millis();
			return _network->write( header, &boardPacket, sizeof(boardPacket) );
		} else {
			Serial.printf("Unhandled messageType in send() %c \n", messageType);
		}
	}
	else {
		Serial.printf("Unhandled role in send(): %d \n", _role);
	}
}
//---------------------------------------------------------------------------------
void esk8Lib::handle_Controller_Message(RF24NetworkHeader& header) {
    _network->read(header, &controllerPacket, sizeof(controllerPacket));
}
//---------------------------------------------------------------------------------
void esk8Lib::handle_Board_Message(RF24NetworkHeader& header) {
    _network->read(header, &boardPacket, sizeof(boardPacket));
}
//---------------------------------------------------------------------------------
int esk8Lib::controllerOnline() {
	return (millis()-_lastControllerCommsTime) < CONTROLLER_SEND_INTERVAL;
}
//---------------------------------------------------------------------------------
int esk8Lib::boardOnline() {
	return (millis()-_lastBoardCommsTime) < CONTROLLER_SEND_INTERVAL;
}
//---------------------------------------------------------------------------------
long esk8Lib::getSendInterval() {
	if (_role == CONTROLLER) {
		return CONTROLLER_SEND_INTERVAL;
	}
	else if (_role == BOARD) {
		return BOARD_SEND_INTERVAL;
	}
	else {
		Serial.printf("Unhandled getSendInterval() role %d \n", _role);
	}
}