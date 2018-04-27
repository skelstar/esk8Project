#include "Arduino.h"
#include "esk8Lib.h"

byte addresses[][6] = {"1Node","2Node"};

#define 	RADIO_1			1
#define		RADIO_0			0
#define 	PIPE_NUMBER		1

#define 	ROLE_MASTER		1
#define 	ROLE_SLAVE		0

//--------------------------------------------------------------------------------

esk8Lib::esk8Lib() {}

//--------------------------------------------------------------------------------
void esk8Lib::begin(RF24 *radio, int role, int radioNumber) {

	_radio = radio;
	_role = role;

	Serial.println(F("esk8Lib begin()"));
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

	if (role == ROLE_SLAVE) {

		slavePacket.throttle = 127;

		_radio->startListening();
		_radio->writeAckPayload(PIPE_NUMBER, &slavePacket, sizeof(slavePacket));
	}

	_radio->printDetails();
}
//--------------------------------------------------------------------------------
int esk8Lib::sendPacketToSlave() {

	_radio->stopListening();
	
	Serial.println("Sending...");

	bool sendOk = _radio->write(&masterPacket, sizeof(masterPacket));
	if (sendOk == false) {
		return false;
	}

	bool timedOut = false;
	long startedWaiting = millis();
	_radio->startListening();
	while (_radio->available() == false) {
		if (millis()-startedWaiting > 200) {
			timedOut = true;
			return false;
		}
	}

	_radio->read(&slavePacket, sizeof(slavePacket));
	
	return true;
}
//--------------------------------------------------------------------------------
int esk8Lib::pollMasterForPacket() {

	byte rxData;

	#define READ_TIMEOUT	1000

	if( _radio->available() ) {
		while (_radio->available()) {                          	// While there is data ready
			_radio->read( &masterPacket, sizeof(masterPacket) );         	// Get the payload
		}

		// This can be commented out to send empty payloads.
		_radio->writeAckPayload( PIPE_NUMBER, &slavePacket, sizeof(slavePacket) );  			

		return true;
	}
	return false;
}
//--------------------------------------------------------------------------------
void esk8Lib::updateMasterPacket(int32_t newValue) {
	masterPacket.rpm = newValue;
}
//--------------------------------------------------------------------------------
void esk8Lib::updateSlavePacket(int newValue) {
	slavePacket.throttle = newValue;
}
