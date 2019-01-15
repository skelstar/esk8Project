#ifndef Esk8Lib_h
#define Esk8Lib_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <RF24Network.h>
#include <RF24.h>

#include <esp_int_wdt.h>
#include <esp_task_wdt.h>

//--------------------------------------------------------------------------------

struct BoardStruct{
	bool vescOnline;
	float batteryVoltage;
	bool areMoving;
	int odometer;
};


struct ControllerStruct {
	byte throttle;
	long id;
	bool buttonC;
};

struct HudStruct {
	byte controllerState;
	byte boardState;
};

typedef void ( *PacketAvailableCallback )( uint16_t from );

class esk8Lib
{
	public:

		enum Role {
			RF24_BOARD		=	0,
			RF24_CONTROLLER	=	1,
			RF24_HUD		=	2
		};

		esk8Lib();
		
		void begin(
			RF24 *radio, 
			RF24Network *network, 
			Role role, 
			PacketAvailableCallback packetAvailableCallback);
		void service();
		bool sendPacketToController();
		bool sendPacketToBoard();
		bool sendPacketToHUD();
		bool numMissedPackets();
		bool controllerOnline(int timeoutMs);
		bool boardOnline(int timeoutMs);

		BoardStruct boardPacket;
		ControllerStruct controllerPacket;
		HudStruct hudPacket;

	private:
		RF24 *_radio;
		RF24Network *_network;
		Role _role;

		long ctime;
		long btime;
		long _oldControllerPacketId;

		uint16_t readPacket();
		bool sendPacket(uint16_t to, unsigned char type, const void *message);

		PacketAvailableCallback _packetAvailableCallback;
};

#endif
