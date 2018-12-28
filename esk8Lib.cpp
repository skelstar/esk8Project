#include "Arduino.h"
#include "esk8Lib.h"

//--------------------------------------------------------------------------------

esk8Lib::esk8Lib() {}

//--------------------------------------------------------------------------------
void esk8Lib::begin(RF24 *radio, bool isController) {	

	byte pipes[][6] = { "1Node", "2Node" };              // Radio pipe addresses for the 2 nodes to communicate.

	_radio = radio;

	_radio->begin();
	_radio->setRetries(0,15);                 // Smallest time between retries, max no. of retries
	_radio->setPayloadSize(1);                // Here we are sending 1-byte payloads to test the call-response speed
	
	if ( isController ) {
		_radio->openWritingPipe(pipes[1]);        // Both radios listen on the same pipes by default, and switch when writing
		_radio->openReadingPipe(1, pipes[0]);
	}
	else {
		_radio->openWritingPipe(pipes[0]);        // Both radios listen on the same pipes by default, and switch when writing
		_radio->openReadingPipe(1,pipes[1]);
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
	}
}
//---------------------------------------------------------------------------------
int esk8Lib::controllerOnline() {
}
//---------------------------------------------------------------------------------
int esk8Lib::boardOnline() {
}
//---------------------------------------------------------------------------------
