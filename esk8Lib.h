#ifndef Esk8Lib_h
#define Esk8Lib_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <RF24.h>
#include <RF24Network.h > 

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
		enum ReturnCode {
			CODE_SUCCESS,
			ERR_NOT_SEND_OK,
			ERR_TIMEOUT
		};

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
		
		void begin(Role role);
		void service();
		int send();
		int controllerOnline();
		int boardOnline();
		void enableDebug();

		BoardStruct boardPacket;
		ControllerStruct controllerPacket;
		HudReqStruct hudReqPacket;

		const uint64_t talking_pipes[5] = { 0xF0F0F0F0D2LL, 0xF0F0F0F0C3LL, 0xF0F0F0F0B4LL, 0xF0F0F0F0A5LL, 0xF0F0F0F096LL };
		const uint64_t listening_pipes[5] = { 0x3A3A3A3AD2LL, 0x3A3A3A3AC3LL, 0x3A3A3A3AB4LL, 0x3A3A3A3AA5LL, 0x3A3A3A3A96LL };

	private:
		RF24 *_radio;
		RF24Network *_network;
		uint16_t _this_node;
		uint16_t _other_node;

		long _lastSentToController;
		long _lastSentToBoard;

		long _lastPacketReadTime;
		long _lastControllerCommsTime;
		long _lastBoardCommsTime;
		bool _debugMessages = false;

		void handle_Controller_Message(RF24NetworkHeader& header);
		void handle_Board_Message(RF24NetworkHeader& header);

		PacketAvailableCallback _packetAvailableCallback;

};

#endif
