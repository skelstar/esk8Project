#include <SPI.h>
#include "RF24.h"
#include <esk8Lib.h>

// ESP32
bool radioNumber = 1;
#define SPI_CE		33	// white/purple
#define SPI_CS		26	// green

/*--------------------------------------------------------------------------------*/

const char compile_date[] = __DATE__ " " __TIME__;

//--------------------------------------------------------------------------------

esk8Lib esk8;

#define 	ROLE_MASTER		0
#define 	ROLE_SLAVE		1

const char boardSetup[] = "WEMOS TTGO Board";
// #define SPI_MOSI			23 // Blue
// #define SPI_MISO			19 // Orange
// #define SPI_CLK				18 // Yellow
// #define SPI_CE				33	// white/purple
// #define SPI_CS				26  // green

RF24 radio(SPI_CE, SPI_CS);

//--------------------------------------------------------------------------------

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

//--------------------------------------------------------------------------------
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

	esk8.begin(&radio, ROLE_MASTER, radioNumber, &debug);

	esk8.controllerPacket.throttle = 0.1;
}
//--------------------------------------------------------------------------------
void loop(void) {
  
	if (esk8.sendThenReadPacket() == true) {
		Serial.println("sendThenReadPacket() == true");
	}
	else {
		Serial.println("sendThenReadPacket() == false");
	}

	esk8.controllerPacket.throttle += 0.1;
    
    delay(1000);  // Try again later
}