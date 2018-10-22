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
void esk8Lib::begin(Role role) {	

	_this_node = role; // Which node are we?
	switch (role) {
		case RF24_CONTROLLER:
			_other_node = RF24_BOARD;
			break;
		case RF24_BOARD:
			_other_node = RF24_CONTROLLER;
			break;
		default:
			break;
	}


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
