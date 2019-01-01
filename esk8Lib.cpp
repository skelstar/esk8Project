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

	ctime = millis();
	btime = millis();

	boardPacket.batteryVoltage = 0.0;
	boardPacket.vescOnline = false;
	
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
	RF24NetworkHeader header( RF24_CONTROLLER );
	return _network->write(header, &boardPacket, sizeof(BoardStruct));
}
//---------------------------------------------------------------------------------
bool esk8Lib::sendPacketToBoard() {
	RF24NetworkHeader header( RF24_BOARD );
	// Serial.printf("Sending %d \n", controllerPacket.throttle);
	return _network->write(header, &controllerPacket, sizeof(ControllerStruct));
}
//---------------------------------------------------------------------------------
bool esk8Lib::sendPacketToHUD() {
	RF24NetworkHeader header( RF24_HUD );
	if (_role == RF24_CONTROLLER || _role == RF24_BOARD) {
		return _network->write(header, &hudPacket, sizeof(HudStruct));
	}
	else {
		// exception
		Serial.printf("ERROR CONDITION!!! sendPacketToHUD (role: %d) (23) \n", _role);
	}
	return false;
}
//---------------------------------------------------------------------------------
uint16_t esk8Lib::readPacket() {

	RF24NetworkHeader header;                            // If so, take a look at it
    _network->peek(header);

	long time = 0;

	if ( header.from_node == RF24_CONTROLLER ) {
		 _network->read(header, &controllerPacket, sizeof(controllerPacket));
		// Serial.printf("%s %d\n", header.toString(), controllerPacket.throttle);
	}
	else if ( header.from_node == RF24_BOARD ) {
		_network->read(header, &boardPacket, sizeof(boardPacket));
		// Serial.printf("%s %.1f \n", header.toString(), boardPacket.batteryVoltage);
	}
	else if ( header.from_node == RF24_HUD ) {
		_network->read(header, &hudPacket, sizeof(hudPacket));
	}
	else {
		Serial.printf("ERROR CONDITION!!! readPacket (from_node: '%d') (23) \n", header.from_node);
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
