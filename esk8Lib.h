#ifndef Esk8Lib_h
#define Esk8Lib_h

#include <Arduino.h>
#include <ArduinoJson.h>
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

struct HudReqStruct {
	int id;
};

typedef void ( *PacketAvailableCallback )( int test );

// #define	STARTUP 		1 << 0
// #define WARNING 		1 << 1
// #define ERROR 			1 << 2
// #define DEBUG 			1 << 3
// #define COMMUNICATION 	1 << 4
// #define VESC_COMMS		1 << 5

class esk8Lib
{
	public:

		enum Role {
			RF24_BOARD		=	0,
			RF24_CONTROLLER	=	1,
			RF24_HUD		=	2
		};

		enum RoleType {
			CONTROLLER,
			BOARD,
			HUD
		};

		esk8Lib();
		
		void begin(RF24 *radio, Role role);
		void service();
		bool sendPacket(byte counter);
		void readPacket();
		int controllerOnline();
		int boardOnline();

		int rxCounter;

		BoardStruct boardPacket;
		ControllerStruct controllerPacket;
		HudReqStruct hudReqPacket;

	private:
		RF24 *_radio;
		Role _role;

		uint16_t _this_node;
		uint16_t _other_node;

		long _lastSentToController;
		long _lastSentToBoard;

		long _lastPacketReadTime;
		long _lastControllerCommsTime;
		long _lastBoardCommsTime;
		bool _debugMessages = false;

		PacketAvailableCallback _packetAvailableCallback;
};

#endif
