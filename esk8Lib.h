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
	long id;
};


struct ControllerStruct {
	int throttle;
	int encoderButton;
	long id;
};

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

		enum RoleType {
			CONTROLLER,
			BOARD,
			LISTENER
		};

		esk8Lib();
		
		void begin(RF24 *radio, RF24Network *network, RoleType role);
		int available();

		int send(char messageType);

		void handle_Controller_Message(RF24NetworkHeader& header);
		void handle_ACK_Message(RF24NetworkHeader& header);
		void handle_Board_Message(RF24NetworkHeader& header);

		int controllerOnline();
		int boardOnline();

		long getSendInterval();

		BoardStruct boardPacket;
		ControllerStruct controllerPacket;

	private:
		RF24 *_radio;
		RF24Network *_network;
		RoleType _role;
		uint16_t _this_node;
		uint16_t _other_node;

		long _lastSentToController;
		long _lastSentToBoard;

		long _lastPacketReadTime;
		long _lastControllerCommsTime;
		long _lastBoardCommsTime;
};

#endif
