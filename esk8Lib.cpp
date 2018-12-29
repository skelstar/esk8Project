#include "Arduino.h"
#include "esk8Lib.h"

//--------------------------------------------------------------------------------

esk8Lib::esk8Lib() {}

//--------------------------------------------------------------------------------
void esk8Lib::begin(RF24 *radio, RF24Network *network, Role role) {	

	byte pipes[][6] = { "1Node", "2Node" };              // Radio pipe addresses for the 2 nodes to communicate.

	_role = role;

	_radio = radio;
	_network = network;

	_radio->begin();
	_radio->setPALevel(RF24_PA_LOW);

	switch (_role) {
		case RF24_BOARD:
			_network->begin(/*channel*/ 100, /*node address*/ 00 );
			_network->multicastLevel(0);
			break;
		case RF24_CONTROLLER:
			_network->begin(/*channel*/ 100, /*node address*/ 01 );
			_network->multicastLevel(1);
			break;
		case RF24_HUD:
			_network->begin(/*channel*/ 100, /*node address*/ 02 );
			_network->multicastLevel(1);
			break;
	}

	_radio->printDetails();                   // Dump the configuration of the rf unit for debugging

	controllerPacket.throttle = 126;
	controllerPacket.id = 0;

	missingPackets = 0;
	state = OK;

	boardPacket.id = 0;
	hudReqPacket.id = 0;
}
//---------------------------------------------------------------------------------
void esk8Lib::service() {
}
//---------------------------------------------------------------------------------
bool esk8Lib::sendPacket() {

	if (_role == RF24_CONTROLLER) {
		RF24NetworkHeader header(/*to node*/ RF24_BOARD, /*type*/ 'C' /*Controller*/);
		return _network->write(header, &controllerPacket, sizeof(ControllerStruct));
	}
	else if (_role == RF24_BOARD) {
		RF24NetworkHeader header(/*to node*/ RF24_HUD, /*type*/ 'B' /*Board*/);
		return _network->multicast(header, &boardPacket, sizeof(BoardStruct), 1);
	}
	else if (_role == RF24_HUD) {
		// exception
	}

	return false;
}
//---------------------------------------------------------------------------------
char esk8Lib::readPacket() {

	RF24NetworkHeader header;                            // If so, take a look at it
    _network->peek(header);

	if ( header.type == 'C' ) {
		_network->read(header, &controllerPacket, sizeof(ControllerStruct));
		int idDifference = controllerPacket.id - _lastControllerId;
		_lastControllerId = controllerPacket.id;
		if ( idDifference > 1 ) {
			missingPackets = idDifference;
			state = MISSED_PACKET;
		}
	}
	else if ( header.type == 'B' ) {
		_network->read(header, &boardPacket, sizeof(BoardStruct));
		int idDifference = boardPacket.id - _lastBoardId;
		_lastBoardId = boardPacket.id;
		if ( idDifference > 1 ) {
			missingPackets = idDifference;
			state = MISSED_PACKET;
		}
	}
	else if ( header.type == 'H' ) {
		//network->read(header, &controllerPacket, sizeof(ControllerStruct));
	}
	return header.type;
}
//---------------------------------------------------------------------------------
int esk8Lib::controllerOnline() {
}
//---------------------------------------------------------------------------------
int esk8Lib::boardOnline() {
}
//---------------------------------------------------------------------------------
