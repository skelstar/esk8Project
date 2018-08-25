#include "Arduino.h"
#include "esk8Lib.h"

#define 	ADDR_INDEX_LISTENER	2
#define 	ADDR_INDEX_CONTROLLER	1
#define 	ADDR_INDEX_BOARD		0

#define 	CONTROLLER_SEND_INTERVAL	400 + 50
#define 	BOARD_SEND_INTERVAL			4000

#define 	MSG_CONTROLLER_PACKET 		'A'
#define 	MSG_CONTROLLER_PACKET_ACK	'a'
#define 	MSG_BOARD_PACKET			'B'
#define 	MSG_BOARD_PACKET_ACK		'b'

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
	controllerPacket.id = 0;
}
//---------------------------------------------------------------------------------
int esk8Lib::available() {
	
	_network->update();
	
	while (_network->available()) {

		esp_task_wdt_feed();

		RF24NetworkHeader header; // If so, take a look at it
        _network->peek(header);

    	Serial.printf("Message handled %c \n", header.type);

        switch (header.type) {
			case MSG_CONTROLLER_PACKET:	{ 
				_lastControllerCommsTime = millis();
				handle_Controller_Message(header);
				return true;
			}
			case MSG_CONTROLLER_PACKET_ACK:	{
				handle_ACK_Message(header);
				_lastBoardCommsTime = millis();
				return false;
			}
			case MSG_BOARD_PACKET: { // message from Board
				_lastBoardCommsTime = millis();
				handle_Board_Message(header);
				return true;
			}
			case MSG_BOARD_PACKET_ACK: { 	
				_lastControllerCommsTime = millis();
				return false;
			}
	        default: {
				Serial.printf("*** WARNING *** Unknown message type '%c' \n\r", header.type);
				_network->read(header, 0, 0);
				return false;
			}
        }
	}
	return false;
}
//---------------------------------------------------------------------------------
int esk8Lib::send(char messageType) {

	if ( messageType == MSG_CONTROLLER_PACKET ) {
		Serial.printf("Sending MSG_CONTROLLER_PACKET\n");
		RF24NetworkHeader header( _other_node, messageType );
		_lastControllerCommsTime = millis();
		controllerPacket.id ++;
		return _network->multicast( header, &controllerPacket, sizeof(controllerPacket), /* level */ 1 );
	}
	else if ( messageType == MSG_CONTROLLER_PACKET_ACK ) {
		RF24NetworkHeader header( _other_node, messageType );
		_lastBoardCommsTime = millis();
		return _network->write( header, &controllerPacket.id, sizeof(controllerPacket.id) );
	}
	else if ( messageType == MSG_BOARD_PACKET ) {
		RF24NetworkHeader header( _other_node, messageType );
		_lastBoardCommsTime = millis();
		return _network->write( header, &boardPacket, sizeof(boardPacket) );
	}
	else {
		Serial.printf("Unhandled messageType in send() %c \n", messageType);
		return false;
	}
}
//---------------------------------------------------------------------------------
void esk8Lib::handle_Controller_Message(RF24NetworkHeader& header) {
	Serial.printf("Controller message handled \n");
    _network->read(header, &controllerPacket, sizeof(controllerPacket));
    // send ACK
    send(MSG_CONTROLLER_PACKET_ACK);
}
//---------------------------------------------------------------------------------
void esk8Lib::handle_ACK_Message(RF24NetworkHeader& header) {
	_network->read(header, 0, 0);
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
	return (millis()-_lastBoardCommsTime) < (CONTROLLER_SEND_INTERVAL * 1.1);	// 10% threshold
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