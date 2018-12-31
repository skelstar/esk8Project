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

	controllerPacket.id = 0;

	ctime = millis();
	btime = millis();

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
	RF24NetworkHeader header(RF24_CONTROLLER, BOARD_TYPE);
	return _network->write(header, &boardPacket, sizeof(boardPacket));
}
//---------------------------------------------------------------------------------
bool esk8Lib::sendPacketToBoard() {
	RF24NetworkHeader header(RF24_BOARD, CONTROLLER_TYPE);
	return _network->write(header, &controllerPacket, sizeof(controllerPacket));
}
//---------------------------------------------------------------------------------
bool esk8Lib::sendPacketToHUD() {
	if (_role == RF24_CONTROLLER || _role == RF24_BOARD) {
		return sendPacket(RF24_HUD, HUD_TYPE, &hudPacket);
	}
	else {
		// exception
		Serial.printf("ERROR CONDITION!!! sendPacketToHUD (role: %d) (23) \n", _role);
	}
	return false;
}
//---------------------------------------------------------------------------------
bool esk8Lib::sendPacket(uint16_t to, unsigned char type, const void *message) {
	RF24NetworkHeader header(to, type);
	return _network->write(header, message, sizeof(message));
}
//---------------------------------------------------------------------------------
uint16_t esk8Lib::readPacket() {

	RF24NetworkHeader header;                            // If so, take a look at it
    _network->peek(header);

    // Serial.printf("%s\n", header.toString());

	if ( header.type == CONTROLLER_TYPE ) {
		_network->read(header, &controllerPacket, sizeof(controllerPacket));
	}
	else if ( header.type == BOARD_TYPE ) {
		_network->read(header, &boardPacket, sizeof(boardPacket));
	}
	else if ( header.type == HUD_TYPE ) {
		_network->read(header, &hudPacket, sizeof(hudPacket));
	}
	else {
		Serial.printf("ERROR CONDITION!!! readPacket (type: '%d') (23) \n", header.type);
	}
	return header.from_node;
}
//---------------------------------------------------------------------------------
bool esk8Lib::controllerOnline(int timeoutMs) {
	return millis() - ctime > timeoutMs;
}
//---------------------------------------------------------------------------------
bool esk8Lib::boardOnline(int timeoutMs) {
	return millis() - btime > timeoutMs;
}
//---------------------------------------------------------------------------------
