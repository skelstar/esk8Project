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

	controllerPacket.throttle = 127;
	controllerPacket.id = 0;
	boardPacket.id = 0;
	hudReqPacket.id = 0;
}
//---------------------------------------------------------------------------------
void esk8Lib::service() {
}
//---------------------------------------------------------------------------------
bool esk8Lib::sendPacket(byte counter) {

	_radio->stopListening();
	bool sentOk = _radio->write(&counter, sizeof(counter));
	_radio->startListening();

	return sentOk;
}
//---------------------------------------------------------------------------------
void esk8Lib::readPacket() {
	byte pipeNo;

	if ( _radio->available(&pipeNo) ) {
		_radio->read(&rxCounter, sizeof(rxCounter));
		Serial.printf("Read from %d \n", pipeNo);
	}
}
//---------------------------------------------------------------------------------
int esk8Lib::controllerOnline() {
}
//---------------------------------------------------------------------------------
int esk8Lib::boardOnline() {
}
//---------------------------------------------------------------------------------
