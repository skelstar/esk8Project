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
	DynamicJsonBuffer jsonBuffer(100);
	JsonObject& root = jsonBuffer.parseObject(msg);
	if (root.containsKey("batteryVoltage")) {
		masterPacket.batteryVoltage = root["batteryVoltage"];
		Serial.printf("masterPacket() batteryVoltage: %d\n", masterPacket.batteryVoltage);
	}
	else {
		Serial.printf("parseBoardPacket(): Error parsing %s", msg);
	}
}
//---------------------------------------------------------------------------------
void esk8Lib::parseControllerPacket(String &msg) {
	DynamicJsonBuffer jsonBuffer(100);
	JsonObject& root = jsonBuffer.parseObject(msg);
	if (root.containsKey("throttle")) {
		slavePacket.throttle = root["throttle"];
		Serial.printf("saveBoardPacket() throttle: %d\n", slavePacket.throttle);
	}
	else {
		Serial.printf("parseControllerPacket(): Error parsing %s", msg);
	}
}
//---------------------------------------------------------------------------------
String esk8Lib::encodeControllerPacket() {
	char buff[50];
	sprintf(buff, "throttle:%d", slavePacket.throttle);
	// DynamicJsonBuffer jsonBuffer;
	// JsonObject& msg = jsonBuffer.createObject();
	// msg["throttle"] = slavePacket.throttle;

	// String str;
	// msg.printTo(str);
	return buff;
}
//---------------------------------------------------------------------------------
String esk8Lib::encodeBoardPacket() {
	char buff[50];
	sprintf(buff, "batteryVoltage:%.1f", masterPacket.batteryVoltage);
	// DynamicJsonBuffer jsonBuffer;
	// JsonObject& msg = jsonBuffer.createObject();
	// msg["batteryVoltage"] = masterPacket.batteryVoltage;

	// String str;
	// msg.printTo(str);
	return str;
}
//--------------------------------------------------------------------------------
void esk8Lib::updateMasterPacket(int32_t newValue) {
	// masterPacket.rpm = newValue;
}
//--------------------------------------------------------------------------------
void esk8Lib::updateSlavePacket(int newValue) {
	slavePacket.throttle = newValue;
}
