#include "Arduino.h"
#include "esk8Lib.h"

//--------------------------------------------------------------------------------

esk8Lib::esk8Lib() {}

//--------------------------------------------------------------------------------
void esk8Lib::begin(RF24 *radio, Role role) {	

	byte pipes[][6] = { "1Node", "2Node" };              // Radio pipe addresses for the 2 nodes to communicate.

	_role = role;

	_radio = radio;

	_radio->begin();
	_radio->setRetries(0,15);                 // Smallest time between retries, max no. of retries
	_radio->setPayloadSize(1);                // Here we are sending 1-byte payloads to test the call-response speed
	
	switch (_role) {
		case RF24_CONTROLLER:
			_radio->openWritingPipe(pipes[1]);        
			_radio->openReadingPipe(1, pipes[0]);
			break;
		case RF24_HUD:
			_radio->openReadingPipe(2,pipes[0]);	// reads Board packets
		case RF24_BOARD:
			_radio->openWritingPipe(pipes[0]);       // reads Master packets
			_radio->openReadingPipe(1,pipes[1]);
			break;
	}
	_radio->startListening();                 // Start listening
	_radio->printDetails();                   // Dump the configuration of the rf unit for debugging

	controllerPacket.throttle = 126;
	controllerPacket.id = 0;
	boardPacket.id = 0;
	hudReqPacket.id = 0;
}
//---------------------------------------------------------------------------------
void esk8Lib::service() {
}
//---------------------------------------------------------------------------------
bool esk8Lib::sendPacket() {

	bool sentOk = false;
	_radio->stopListening();
	
	if (_role == RF24_CONTROLLER) {
		sentOk = _radio->write(&controllerPacket, sizeof(controllerPacket));
	}
	else if (_role == RF24_BOARD) {
		sentOk = _radio->write(&boardPacket, sizeof(boardPacket));
	}
	else if (_role == RF24_HUD) {
		// exception
	}
	_radio->startListening();

	return sentOk;
}
//---------------------------------------------------------------------------------
int esk8Lib::readPacket() {
	byte pipeNo = -1;

	if (_role == RF24_CONTROLLER) {
		if ( _radio->available(&pipeNo) ) {
			_radio->read(&boardPacket, sizeof(boardPacket));
			Serial.printf("Read from Board (pipe %d) \n", pipeNo);
		}
	}
	else if (_role == RF24_BOARD) {
		if ( _radio->available(&pipeNo) ) {
			_radio->read(&controllerPacket, sizeof(controllerPacket));
			Serial.printf("Read from Controller (pipe %d) \n", pipeNo);
		}
	}
	else if (_role == RF24_HUD) {
		if ( _radio->available(&pipeNo) ) {
			if (pipeNo == 1) {
				_radio->read(&controllerPacket, sizeof(controllerPacket));
				Serial.printf("Read from Controller (pipe %d) \n", pipeNo);
			}
			else if (pipeNo == 2) {
				_radio->read(&boardPacket, sizeof(boardPacket));
				Serial.printf("Read from Board (pipe %d) \n", pipeNo);
			}
		}
	}

	return pipeNo;
}
//---------------------------------------------------------------------------------
int esk8Lib::controllerOnline() {
}
//---------------------------------------------------------------------------------
int esk8Lib::boardOnline() {
}
//---------------------------------------------------------------------------------
