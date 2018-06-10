#include <SPI.h>
#include "RF24.h"
#include <esk8Lib.h>
#include <debugHelper.h>
/*--------------------------------------------------------------------------------*/

const char compile_date[] = __DATE__ " " __TIME__;


/****************** User Config ***************************/
bool radioNumber = 0;
// DEV Board
#define SPI_CE		33	// white/purple
#define SPI_CS		26 // green
// WEMOS TTGO
// #define SPI_CE		33	// white/purple
// #define SPI_CS		26 	// green

// ++ ESP12e
// #define 	SPI_MOSI		13 	// Blue
// #define 	SPI_MISO		12 	// Orange
// #define 	SPI_CLK			14 	// Yellow
// #define 	SPI_CS			15 		green
// #define 	SPI_CE			2

RF24 radio(SPI_CE, SPI_CS);

#define 	ROLE_SLAVE		1
#define 	ROLE_MASTER		0

esk8Lib esk8;

/**********************************************************/
//                                                                            // Topology
// typedef enum { role_ping_out = 1, role_pong_back } role_e;                 // The various roles supported by this sketch
// const char* role_friendly_name[] = { "invalid", "Ping out", "Pong back"};  // The debug-friendly names of those roles
// role_e role = role_pong_back;                                              // The role of the current running sketch

byte counter = 1;                                                          // A single byte to keep track of the data being sent back and forth
//--------------------------------------------------------------------------------

#define	STARTUP 		1 << 0
#define WARNING 		1 << 1
#define ERROR 			1 << 2
#define DEBUG 			1 << 3
#define COMMUNICATION 	1 << 4

debugHelper debug;

//--------------------------------------------------------------
void setup(){

	Serial.begin(9600);

	debug.init();

	debug.addOption(DEBUG, "DEBUG");
	debug.addOption(STARTUP, "STARTUP");
	debug.addOption(COMMUNICATION, "COMMUNICATION");
	debug.addOption(ERROR, "ERROR");
	debug.setFilter(DEBUG | STARTUP | COMMUNICATION | ERROR);

	debug.print(STARTUP, "%s\n", compile_date);

	radio.begin();

	esk8.begin(&radio, ROLE_SLAVE, radioNumber, &debug);
}
//--------------------------------------------------------------------------------

long startTime = 0;
#define SEND_PERIOD		2000
float packetData = 0.0;
bool reportedOffline = false;

void loop(void) {

	bool haveMasterData = esk8.checkForPacket();
	if (haveMasterData == true) {
		Serial.printf("haveMasterData == true : %.1f\n", esk8.controllerPacket.throttle);
	}

	if (esk8.controllerOnline(1100) == false) {
		if (reportedOffline == false) {
			Serial.printf("Controller OFFLINE! \n");
			reportedOffline = true;
		}
	}
	else {
		reportedOffline = false;	
	}

	// if (millis()-startTime > SEND_PERIOD) {
	// 	startTime = millis();
		
	// 	esk8.boardPacket.batteryVoltage = packetData += 0.1;

	// 	bool haveMasterData = esk8.checkForPacket();
	// 	if (haveMasterData == true) {
	// 		Serial.printf("haveMasterData == true : %.1f\n", esk8.controllerPacket.throttle);
	// 	}
	// 	else {
	// 		Serial.printf("haveMasterData == false \n");		
	// 	}
	// }
}	// loop
