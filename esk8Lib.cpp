#include "Arduino.h"
#include "esk8Lib.h"

#define 	ROLE_BOARD		1
#define 	ROLE_CONTROLLER	0

volatile long _lastRxMillis;

//--------------------------------------------------------------------------------

esk8Lib::esk8Lib() {}

//--------------------------------------------------------------------------------
void esk8Lib::begin() {

	Serial.println(F("esk8Lib begin()"));

	slavePacket.throttle = 127;
}
//---------------------------------------------------------------------------------
void esk8Lib::serviceEvents() {
}
//---------------------------------------------------------------------------------
void esk8Lib::parseBoardPacket(String &msg) {
	if (strstr(msg.c_str(), "batteryVoltage") == NULL) {
		Serial.printf("parseBoardPacket(): Error parsing %s \n", msg);
	}
	else {
		char* pch;
		pch = strtok((char*)msg.c_str(), ":");
		masterPacket.batteryVoltage = atof(strtok(NULL, ":"));
	}
}
//---------------------------------------------------------------------------------
void esk8Lib::parseControllerPacket(String &msg) {
	if (strstr(msg.c_str(), "throttle") == NULL) {
		Serial.printf("parseControllerPacket(): Error parsing %s \n", msg);
	}
	else {
		char* pch;
		pch = strtok((char*)msg.c_str(), ":");
		slavePacket.throttle = atoi(strtok(NULL, ":"));
	}
}
//---------------------------------------------------------------------------------
String esk8Lib::encodeControllerPacket() {
	char buff[50];
	sprintf(buff, "throttle:%d", slavePacket.throttle);
	return buff;
}
//---------------------------------------------------------------------------------
String esk8Lib::encodeBoardPacket() {
	char buff[50];
	sprintf(buff, "batteryVoltage:%.1f", masterPacket.batteryVoltage);
	return buff;
}
//--------------------------------------------------------------------------------
void esk8Lib::updateMasterPacket(int32_t newValue) {
	// masterPacket.rpm = newValue;
}
//--------------------------------------------------------------------------------
void esk8Lib::updateSlavePacket(int newValue) {
	slavePacket.throttle = newValue;
}
