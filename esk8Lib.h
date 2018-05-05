#ifndef Esk8Lib_h
#define Esk8Lib_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <RF24.h>
#include <debugHelper.h>

//--------------------------------------------------------------------------------

struct BoardStruct{
	// int32_t rpm;
	float batteryVoltage;
};


struct ControllerStruct {
	int throttle;
};

class esk8Lib
{
	public:
		esk8Lib();
		void begin(RF24 *radio, int role, int radioNumber, debugHelper *debug);

		int sendPacketToBoard();

		int checkForPacket();
		int packetChanged();
		int sendThenReadPacket();
		int controllerOnline();
		int boardOnline();
		int getSendInterval();

		BoardStruct boardPacket;
		ControllerStruct controllerPacket;

		// VESC_DATA vescdata;



	private:
		RF24 *_radio;
		int _role;
		long _lastPacketReadTime;
		debugHelper *_debug;
		ControllerStruct _oldControllerPacket;

};

#endif
