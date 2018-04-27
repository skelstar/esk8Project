#include "Arduino.h"
#include "esk8Lib.h"

#define 	ROLE_MASTER		1
#define 	ROLE_SLAVE		0


#define   MESH_SSID       "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

void sendPacketToSlave();

Task taskSendMessage;	//( TASK_MILLISECOND * 1, TASK_FOREVER, &sendPacketToSlave );

volatile long _lastRxMillis;

//--------------------------------------------------------------------------------

esk8Lib::esk8Lib() {}

//--------------------------------------------------------------------------------
void esk8Lib::begin(int role) {

	// _mesh = mesh;
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
