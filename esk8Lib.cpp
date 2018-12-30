#include "Arduino.h"
#include "esk8Lib.h"

//--------------------------------------------------------------------------------

esk8Lib::esk8Lib() {}

//--------------------------------------------------------------------------------
void esk8Lib::begin(
	RF24 *radio, 
	RF24Network *network, 
	Role role, 
	PacketAvailableCallback packetAvailableCallback) {	

	byte pipes[][6] = { "1Node", "2Node" };              // Radio pipe addresses for the 2 nodes to communicate.

	_role = role;
	_radio = radio;
	_network = network;
	_packetAvailableCallback = packetAvailableCallback;

	_radio->begin();
	_radio->setPALevel(RF24_PA_HIGH);
	_radio->setDataRate(RF24_1MBPS);


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

	controllerPacket.throttle = 127;
	controllerPacket.id = 0;

	boardPacket.id = 0;
	hudPacket.controllerState = 0;
	hudPacket.boardState = 0;
}
//---------------------------------------------------------------------------------
void esk8Lib::service() {
	_network->update();
	if ( _network->available() ) {
		uint16_t from = readPacket();
		_packetAvailableCallback(from);
	}
}
//---------------------------------------------------------------------------------
bool esk8Lib::sendPacketToController() {

	return sendPacket(/*to:*/ RF24_CONTROLLER, /*type:*/ 'B', /*message*/ &boardPacket);
}
//---------------------------------------------------------------------------------
bool esk8Lib::sendPacketToBoard() {

	return sendPacket(/*to:*/ RF24_BOARD, /*type:*/ 'C', /*message*/ &controllerPacket);
}
//---------------------------------------------------------------------------------
bool esk8Lib::sendPacketToHUD() {
	if (_role == RF24_CONTROLLER) {
		return sendPacket(RF24_HUD, 'H', &hudPacket);
	}
	else if (_role == RF24_BOARD) {
		return sendPacket(RF24_HUD, 'H', &hudPacket);
	}
	else {
		// exception
		Serial.printf("ERROR CONDITION!!! sendPacketToHUD (role: %d) (23) \n", _role);
	}
	return false;
}
//---------------------------------------------------------------------------------
bool esk8Lib::sendPacket(uint16_t to, char type, const void *message) {
	RF24NetworkHeader header(to, type);
	return _network->write(header, message, sizeof(message));
}
//---------------------------------------------------------------------------------
uint16_t esk8Lib::readPacket() {

	RF24NetworkHeader header;                            // If so, take a look at it
    _network->peek(header);

	if ( header.type == 'C' ) {
		_network->read(header, &controllerPacket, sizeof(controllerPacket));
	}
	else if ( header.type == 'B' ) {
		_network->read(header, &boardPacket, sizeof(boardPacket));
	}
	else if ( header.type == 'H' ) {
		_network->read(header, &hudPacket, sizeof(hudPacket));
	}
	else {
		Serial.printf("ERROR CONDITION!!! readPacket (from: %d) (23) \n", header.from_node);
	}
	return header.from_node;
}
//---------------------------------------------------------------------------------
int esk8Lib::controllerOnline() {
}
//---------------------------------------------------------------------------------
int esk8Lib::boardOnline() {
}
//---------------------------------------------------------------------------------
