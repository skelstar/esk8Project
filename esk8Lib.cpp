#include "Arduino.h"
#include "esk8Lib.h"

#define 	ROLE_MASTER		1
#define 	ROLE_SLAVE		0

volatile long _lastRxMillis;

//--------------------------------------------------------------------------------

esk8Lib::esk8Lib() {}

//--------------------------------------------------------------------------------
void esk8Lib::begin(int role) {

	_role = role;

	Serial.println(F("esk8Lib begin()"));
	Serial.print("Role: "); Serial.println(role);

	if (role == ROLE_SLAVE) {

		slavePacket.throttle = 127;

	}
}
//---------------------------------------------------------------------------------
void esk8Lib::serviceEvents() {
}
//---------------------------------------------------------------------------------
void esk8Lib::saveBoardPacket(String msg) {
	Serial.println("saveBoardPacket();");
}
//--------------------------------------------------------------
void sendPacketToSlave() {

}
//--------------------------------------------------------------------------------
int esk8Lib::pollMasterForPacket() {

	byte rxData;

	return true;
}
//--------------------------------------------------------------------------------
void esk8Lib::updateMasterPacket(int32_t newValue) {
	// masterPacket.rpm = newValue;
}
//--------------------------------------------------------------------------------
void esk8Lib::updateSlavePacket(int newValue) {
	slavePacket.throttle = newValue;
}
