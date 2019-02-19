#include "Arduino.h"
#include "esk8Lib.h"

//--------------------------------------------------------------------------------

esk8Lib::esk8Lib() {}

//--------------------------------------------------------------------------------
void esk8Lib::begin(
	RF24 *radio, 
	Role role, 
	PacketAvailableCallback packetAvailableCallback) {	

	byte pipes[][6] = { "1Node", "2Node" };              // Radio pipe addresses for the 2 nodes to communicate.

	_role = role;
	_radio = radio;
	_packetAvailableCallback = packetAvailableCallback;

	_radio->begin();
	_radio->setPALevel(RF24_PA_MAX);
	_radio->setAutoAck( true );
	_radio->setRetries(0, 15);
	_radio->enableAckPayload();
	if ( _role == RF24_BOARD ) {
		_radio->openWritingPipe( pipes[1] );
		_radio->openReadingPipe( 1, pipes[0] );
	} 
	else if ( _role == RF24_CONTROLLER ) {
		_radio->openWritingPipe( pipes[0] );
		_radio->openReadingPipe( 1, pipes[1] );
	}
	_radio->startListening();
	_radio->printDetails();                   // Dump the configuration of the rf unit for debugging

	controllerPacket.throttle = 127;
	controllerPacket.id = 0;

	ctime = millis();
	btime = millis();

	boardPacket.batteryVoltage = 0.0;
	boardPacket.vescOnline = false;
	
	hudPacket.controllerState = 0;
	hudPacket.boardState = 0;
}
//---------------------------------------------------------------------------------
void esk8Lib::service() {
	if ( _radio->available() ) {
		Serial.printf("_radio->available()\n");
		switch (_role) {
			case RF24_BOARD:
				if ( readPacket() ) {
					_packetAvailableCallback(RF24_CONTROLLER);
				}
				break;
			case RF24_CONTROLLER:
				if ( readPacket() ) {
					_packetAvailableCallback(RF24_BOARD);
				}
				break;
		}
	}
}
//---------------------------------------------------------------------------------
bool esk8Lib::sendPacket() {
	bool sendOk = false;
	_radio->stopListening();
	switch (_role) {
		case RF24_BOARD:
			Serial.printf("Sending to RF24_CONTROLLER\n");
			sendOk = _radio->write( &boardPacket, sizeof(boardPacket) );    
			while ( sendOk == true && _radio->available() ) {
				_radio->read( &controllerPacket, sizeof(controllerPacket) );
			}
			return sendOk;
		case RF24_CONTROLLER:
			Serial.printf("Sending to RF24_BOARD\n");
			sendOk = _radio->write( &controllerPacket, sizeof(controllerPacket)) ;
			while ( sendOk == true && _radio->available() ) {
				_radio->read( &boardPacket, sizeof(boardPacket) );
			}
			return sendOk;
	}
	return sendOk;
}
//---------------------------------------------------------------------------------
bool esk8Lib::sendPacketToController() {
	return sendPacket();
}
//---------------------------------------------------------------------------------
bool esk8Lib::sendPacketToBoard() {
	return sendPacket();
}
//---------------------------------------------------------------------------------
bool esk8Lib::sendPacketToHUD() {
	// RF24NetworkHeader header( RF24_HUD );
	// if (_role == RF24_CONTROLLER || _role == RF24_BOARD) {
	// 	return _network->write(header, &hudPacket, sizeof(HudStruct));
	// }
	// else {
	// 	// exception
	// 	Serial.printf("ERROR CONDITION!!! sendPacketToHUD (role: %d) (23) \n", _role);
	// }
	return true;
}
//---------------------------------------------------------------------------------
bool esk8Lib::readPacket() {
	byte pipeNo;
	bool readSomething = false;
	while ( _radio->available(&pipeNo) ) {
		Serial.printf("readSomething == true\n");
		switch (_role) {
			case RF24_BOARD:
				_radio->read( &controllerPacket, sizeof(controllerPacket) );
				_radio->writeAckPayload( pipeNo, &boardPacket, sizeof(boardPacket) );
				readSomething = true;
				break;
			case RF24_CONTROLLER:
				_radio->read( &boardPacket, sizeof(boardPacket) );
				readSomething = true;
				break;
		}
	}
	return readSomething;
}
//---------------------------------------------------------------------------------
bool esk8Lib::numMissedPackets() {
	if ( _role == RF24_BOARD ) {
		return controllerPacket.id - _oldControllerPacketId - 1;
	}
	else {
		Serial.printf("DO NOT USE THIS!!! ONLY WITH BOARD\n");
	}
	return 0;
}

bool esk8Lib::controllerOnline(int timeoutMs) {
	return millis() - ctime > timeoutMs;
}
//---------------------------------------------------------------------------------
bool esk8Lib::boardOnline(int timeoutMs) {
	return millis() - btime > timeoutMs;
}
//---------------------------------------------------------------------------------
