#include "Arduino.h"
#include "esk8Lib.h"

#define 	ROLE_BOARD		1
#define 	ROLE_CONTROLLER	0

byte addresses[][6] = {"1Node","2Node"};

#define 	RADIO_1			1
#define		RADIO_0			0
#define 	PIPE_NUMBER		1

volatile long _lastRxMillis;

//--------------------------------------------------------------------------------

esk8Lib::esk8Lib() {}

//--------------------------------------------------------------------------------
void esk8Lib::begin(RF24 *radio, int role, int radioNumber, debugHelper *debug) {

	Serial.println(F("esk8Lib begin()"));

	_radio = radio;
	_role = role;
	_debug = debug;

	Serial.println(F("esk8Lib begin(RF24)"));
	Serial.print("Radio: "); Serial.println(radioNumber);
	Serial.print("Role: "); Serial.println(role);

	_radio->setPALevel(RF24_PA_MAX);

	_radio->enableAckPayload();                     // Allow optional ack payloads
	_radio->enableDynamicPayloads();                // Ack payloads are dynamic payloads

	// Open a writing and reading pipe on each radio, with opposite addresses
	if( radioNumber == RADIO_1 ){
		_radio->openWritingPipe(addresses[RADIO_1]);
		_radio->openReadingPipe(PIPE_NUMBER, addresses[RADIO_0]);
	} else {
		_radio->openWritingPipe(addresses[RADIO_0]);
		_radio->openReadingPipe(PIPE_NUMBER, addresses[RADIO_1]);
	}

	controllerPacket.throttle = 127;
	
	if (role == ROLE_BOARD) {
		_radio->startListening();
		_radio->writeAckPayload(PIPE_NUMBER, &controllerPacket, sizeof(controllerPacket));
	}

	_radio->printDetails();
}
//---------------------------------------------------------------------------------
// int esk8Lib::sendPacketToBoard() {

// 	_radio->stopListening();

// 	bool sendOk = _radio->write(&controllerPacket, sizeof(controllerPacket));

// 	if (sendOk == false) {
// 		_debug->print(d_ERROR, "sendPacketToBoard(); return (sendOK=false) \n");
// 		return false;
// 	}

// 	bool timedOut = false;
// 	long startedWaiting = millis();
// 	_radio->startListening();
// 	while (_radio->available() == false) {
// 		if (millis()-startedWaiting > 200) {
// 			timedOut = true;
// 			_debug->print(d_COMMUNICATION, "sendPacketToBoard(); timeout \n");
// 			return false;
// 		}
// 	}
// 	// _radio->read(&boardPacket, sizeof(boardPacket));

// 	return true;
// }
// //---------------------------------------------------------------------------------
// int esk8Lib::sendPacketToController() {

// 	_radio->stopListening();

// 	bool sendOk = _radio->write(&boardPacket, sizeof(boardPacket));

// 	if (sendOk == false) {
// 		_debug->print(d_ERROR, "sendPacketToController(); return (sendOK=false) \n");
// 		return false;
// 	}

// 	bool timedOut = false;
// 	long startedWaiting = millis();
// 	_radio->startListening();
// 	while (_radio->available() == false) {
// 		if (millis()-startedWaiting > 200) {
// 			timedOut = true;
// 			_debug->print(d_COMMUNICATION, "sendPacketToController(); timeout \n");
// 			return false;
// 		}
// 	}
// 	// _radio->read(&boardPacket, sizeof(boardPacket));
// 	return true;
// }
// //---------------------------------------------------------------------------------
// int esk8Lib::checkForPacketFromBoard() {

// 	if( _radio->available() ) {
// 		_debug->print(d_COMMUNICATION, "_radio->available() == true \n");
// 		while (_radio->available()) {                          	// While there is data ready
// 			_radio->read( &boardPacket, sizeof(boardPacket) );         	// Get the payload
// 		}

// 		// This can be commented out to send empty payloads.
// 		_radio->writeAckPayload( PIPE_NUMBER, &controllerPacket, sizeof(controllerPacket) );  			

// 		return true;
// 	}
// 	return false;
// }
// //---------------------------------------------------------------------------------
// int esk8Lib::checkForPacketFromController() {

// 	if( _radio->available() ) {
// 		_debug->print(d_COMMUNICATION, "_radio->available() == true \n");
// 		while (_radio->available()) {                          	// While there is data ready
// 			_radio->read( &controllerPacket, sizeof(controllerPacket) );         	// Get the payload
// 		}

// 		// This can be commented out to send empty payloads.
// 		_radio->writeAckPayload( PIPE_NUMBER, &boardPacket, sizeof(boardPacket) );  			

// 		return true;
// 	}
// 	return false;
// }
//---------------------------------------------------------------------------------
int esk8Lib::checkForPacket() {

	if( _radio->available() ) {
		_debug->print(d_COMMUNICATION, "_radio->available() == true \n");
		while ( _radio->available() ) {                          	// While there is data ready
			if (_role == ROLE_BOARD) {
				_radio->read( &controllerPacket, sizeof(controllerPacket) );         	// Get the payload
			}
			else if (_role == ROLE_CONTROLLER) {
				_radio->read( &boardPacket, sizeof(boardPacket) );         	// Get the payload
			}
		}

		// This can be commented out to send empty payloads.
		if (_role == ROLE_BOARD) {
			_radio->writeAckPayload( PIPE_NUMBER, &boardPacket, sizeof(boardPacket) );  			
		}
		else if (_role == ROLE_CONTROLLER) {
			_radio->writeAckPayload( PIPE_NUMBER, &controllerPacket, sizeof(controllerPacket) );  			
		}

		return true;
	}
	return false;
}
//---------------------------------------------------------------------------------
int esk8Lib::sendThenReadPacket() {

	_radio->stopListening();

	bool sendOk;

	if (_role == ROLE_BOARD) {
		sendOk = _radio->write(&boardPacket, sizeof(boardPacket));
	}
	else if (_role == ROLE_CONTROLLER) {
		sendOk = _radio->write(&controllerPacket, sizeof(controllerPacket));
	}

	if (sendOk == false) {
		_debug->print(d_ERROR, "sendThenReadPacket(); return (sendOK=false) \n");
		return false;
	}

	bool timedOut = false;
	long startedWaiting = millis();
	_radio->startListening();
	while (_radio->available() == false) {
		if (millis()-startedWaiting > 200) {
			timedOut = true;
			_debug->print(d_COMMUNICATION, "sendThenReadPacket(); timeout \n");
			return false;
		}
	}

	if (_role == ROLE_BOARD) {
		_radio->read(&controllerPacket, sizeof(controllerPacket));
	}
	else if (_role == ROLE_CONTROLLER) {
		_radio->read(&boardPacket, sizeof(boardPacket));
	}

	return true;
}
