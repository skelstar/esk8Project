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
	int encoderButton;
};

#define	STARTUP 		1 << 0
#define WARNING 		1 << 1
#define ERROR 			1 << 2
#define DEBUG 			1 << 3
#define COMMUNICATION 	1 << 4
#define VESC_COMMS		1 << 5

class esk8Lib
{
	public:
		enum ReturnCode {
			CODE_SUCCESS,
			ERR_NOT_SEND_OK,
			ERR_TIMEOUT
		};

		esk8Lib();
		void begin(RF24 *radio, int role, int radioNumber, debugHelper *debug);

		int checkForPacket();
		int packetChanged();
		int sendThenReadACK();
		int controllerOnline();
		int controllerOnline(int period);
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
		// ControllerStruct _oldControllerPacket;

};

#endif
