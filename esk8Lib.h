#ifndef Esk8Lib_h
#define Esk8Lib_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <RF24Network.h>
#include <RF24.h>

#include <esp_int_wdt.h>
#include <esp_task_wdt.h>

// #include <debugHelper.h>

//--------------------------------------------------------------------------------

struct BoardStruct{
	// int32_t rpm;
	float batteryVoltage;
	int id;
	int vescOnline;
};


struct ControllerStruct {
	int throttle;
	int encoderButton;
	int id;
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

		// enum RoleType {
		// 	CONTROLLER,
		// 	BOARD,
		// 	HUD
		// };

		enum StateEnum {
			OK,
			MISSED_PACKET
		};

		esk8Lib();
		
		void begin(
			RF24 *radio, 
			RF24Network *network, 
			Role role, 
			PacketAvailableCallback packetAvailableCallback);
		void service();
		bool sendPacket();
		bool sendPacketToHUD();
		bool sendPacket(uint16_t to, char type, const void *message);
		uint16_t readPacket();
		int controllerOnline();
		int boardOnline();

		BoardStruct boardPacket;
		ControllerStruct controllerPacket;
		HudStruct hudPacket;

		int missingPackets;
		StateEnum state;


	private:
		RF24 *_radio;
		RF24Network *_network;
		Role _role;

		PacketAvailableCallback _packetAvailableCallback;

		int _lastControllerId;
		int _lastBoardId;
};

#endif
