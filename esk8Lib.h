#ifndef Esk8Lib_h
#define Esk8Lib_h

#include <Arduino.h>
#include <SPI.h>
#include "RF24.h"

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
	int32_t rpm;
	float batteryVoltage;
};


struct SlaveStruct {
	int throttle;
};

class esk8Lib
{
	public:
		esk8Lib();
		void begin(RF24 *radio, int role, int radioNumber);
		float getSendDataChecksum(MasterStruct *data);
		float getSlaveChecksum();

		int sendPacketToSlave();
		int pollMasterForPacket();
		void updateSlavePacket(int newValue);
		void updateMasterPacket(int32_t newValue);

		MasterStruct masterPacket;
		SlaveStruct slavePacket;

		// VESC_DATA vescdata;

	private:
		RF24 *_radio;
		int _role;
};

#endif
