#include "Arduino.h"
#include "esk8Lib.h"

#define 	ROLE_BOARD		1
#define 	ROLE_CONTROLLER	0

#define 	ROLE_SLAVE		1
#define 	ROLE_MASTER		0

byte addresses[][6] = {"1Node","2Node"};

#define 	RADIO_1			1
#define		RADIO_0			0
#define 	PIPE_NUMBER		1

#define 	CONTROLLER_SEND_INTERVAL	200 + 50
#define 	BOARD_SEND_INTERVAL			4000

volatile long _lastRxMillis;

// #define	STARTUP 		1 << 0
// #define WARNING 		1 << 1
// #define ERROR 			1 << 2
// #define DEBUG 			1 << 3
// #define COMMUNICATION 	1 << 4

//--------------------------------------------------------------------------------

esk8Lib::esk8Lib() {}

//--------------------------------------------------------------------------------
void esk8Lib::begin(RF24 *radio, int role, int radioNumber, debugHelper *debug) {

	_radio = radio;
	_role = role;

	_debug = debug;

	_debug->print(STARTUP, "esk8Lib begin(RF24) \n");
	_debug->print(STARTUP, "Radio: %d \n", radioNumber);
	_debug->print(STARTUP, "Role: %s \n ", role == ROLE_CONTROLLER ? "MASTER" : "SLAVE");

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
		_radio->writeAckPayload(PIPE_NUMBER, &boardPacket, sizeof(boardPacket));
	}

	_radio->printDetails();
}
//---------------------------------------------------------------------------------
int esk8Lib::checkForPacket() {

	if( _radio->available() ) {
		while ( _radio->available() ) {                          	// While there is data ready
			if (_role == ROLE_BOARD) {
				// save current throttle data
				_radio->read( &controllerPacket, sizeof(controllerPacket) );         	// Get the payload
				_lastPacketReadTime = millis();
			}
			else if (_role == ROLE_CONTROLLER) {
				Serial.printf("ERROR: unhandled role type (Controller). \n");
				// _radio->read( &boardPacket, sizeof(boardPacket) );         	// Get the payload
				// _lastPacketReadTime = millis();
			}
		}

		// This can be commented out to send empty payloads.
		if (_role == ROLE_BOARD) {
			_radio->writeAckPayload( PIPE_NUMBER, &boardPacket, sizeof(boardPacket) );  			
			// _debug->print(COMMUNICATION, "checkForPacket():_radio->available() == true throttle=%d \n", controllerPacket.throttle);
		}
		else if (_role == ROLE_CONTROLLER) {
			Serial.printf("ERROR: unhandled role type (Controller). \n");
			//_radio->writeAckPayload( PIPE_NUMBER, &controllerPacket, sizeof(controllerPacket) );  			
		}

		return true;
	}
	return false;
}
//---------------------------------------------------------------------------------
// int esk8Lib::packetChanged() {

// 	bool changed = false;
	
// 	switch (_role) {
// 		case ROLE_BOARD:
// 			changed = _oldControllerPacket.throttle != controllerPacket.throttle;
// 			break;
// 		default:
// 			changed = false;
// 	}
// 	return changed;
// }
//---------------------------------------------------------------------------------
int esk8Lib::sendThenReadPacket() {

	_radio->stopListening();

	bool sendOk;
	uint8_t returnCode = CODE_SUCCESS;

	if (_role == ROLE_BOARD) {
		sendOk = _radio->write(&boardPacket, sizeof(boardPacket)) == true;
	}
	else if (_role == ROLE_CONTROLLER) {
		sendOk = _radio->write(&controllerPacket, sizeof(controllerPacket)) == true;
	}

	if (sendOk == false) {
		return ERR_NOT_SEND_OK;
	}

	long startedWaiting = millis();
	_radio->startListening();
	while (_radio->available() == false) {
		Serial.print(".");
		if (millis()-startedWaiting > 50) {
			// timeout
			return ERR_TIMEOUT;
		}
	}

	if (_role == ROLE_BOARD) {
		_radio->read(&controllerPacket, sizeof(controllerPacket));
		_lastPacketReadTime = millis();
	}
	else if (_role == ROLE_CONTROLLER) {
		_radio->read(&boardPacket, sizeof(boardPacket));
		_lastPacketReadTime = millis();
	}

	return CODE_SUCCESS;
}
//---------------------------------------------------------------------------------
int esk8Lib::getSendInterval() {
	if (_role == ROLE_CONTROLLER) {
		return CONTROLLER_SEND_INTERVAL;
	}
	else if (_role == ROLE_BOARD) {
		return BOARD_SEND_INTERVAL;
	}
}
//---------------------------------------------------------------------------------
int esk8Lib::controllerOnline() {
	return (millis()-_lastPacketReadTime) < CONTROLLER_SEND_INTERVAL;
}

int esk8Lib::controllerOnline(int period) {
	return (millis()-_lastPacketReadTime) < period;
}

//---------------------------------------------------------------------------------
int esk8Lib::boardOnline() {
	return (millis()-_lastPacketReadTime) < CONTROLLER_SEND_INTERVAL;
}
//---------------------------------------------------------------------------------
