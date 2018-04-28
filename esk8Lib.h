#ifndef Esk8Lib_h
#define Esk8Lib_h

#include <Arduino.h>
#include <ArduinoJson.h>

//--------------------------------------------------------------------------------


// typedef struct VESC_DATA {
// 	float batteryVoltage;
// 	long t;
// };

// typedef struct CONTROLLER_DATA { 
// 	int zButton;
// 	int cButton;
// 	int joyY;
// };

// #define MASTER_ROLE 1
// #define SLAVE_ROLE	0



struct MasterStruct{
	// int32_t rpm;
	float batteryVoltage;
};


struct SlaveStruct {
	int throttle;
};

class esk8Lib
{
	public:
		esk8Lib();
		void begin();
		float getSendDataChecksum(MasterStruct *data);
		float getSlaveChecksum();

		void updateSlavePacket(int newValue);
		void updateMasterPacket(int32_t newValue);

		void parseBoardPacket(String &msg);
		void parseControllerPacket(String &msg);
		String encodeControllerPacket();
		String encodeBoardPacket();

		void serviceEvents();

		MasterStruct masterPacket;
		SlaveStruct slavePacket;

		// VESC_DATA vescdata;

	private:
};

#endif
