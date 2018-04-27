#include <SPI.h>
#include "RF24.h"
#include <esk8Lib.h>

// ESP32
bool radioNumber = 0;
#define SPI_CE		33	// white/purple
#define SPI_CS		26	// green

//--------------------------------------------------------------------------------

RF24 radio(SPI_CE, SPI_CS);

esk8Lib esk8;

#define 	ROLE_MASTER		1
#define 	ROLE_SLAVE		0

// #define 	SPI_MOSI		19 	// Blue
// #define 	SPI_MISO		18 	// Orange
// #define 	SPI_CLK			5 	// Yellow
// #define 	SPI_CS			14//4	// (A5)green
// #define 	SPI_CE			32//21	// white/purple

//--------------------------------------------------------------------------------

typedef enum { role_ping_out = 1, role_pong_back } role_e;                 // The various roles supported by this sketch
const char* role_friendly_name[] = { "invalid", "Ping out", "Pong back"};  // The debug-friendly names of those roles
role_e role = role_pong_back;                                              // The role of the current running sketch

byte counter = 1;                                                          // A single byte to keep track of the data being sent back and forth

//--------------------------------------------------------------------------------
void setup(){

	Serial.begin(9600);

	radio.begin();

	esk8.begin(&radio, ROLE_MASTER, radioNumber);

	esk8.masterPacket.batteryVoltage = 32.1;
}
//--------------------------------------------------------------------------------
void loop(void) {
  
	if (esk8.sendPacketToSlave() == true) {
		Serial.println("sendPacket() == true");
	}
	else {
		Serial.println("sendPacket() == false");
	}

	esk8.masterPacket.batteryVoltage += 0.1;
    
    delay(1000);  // Try again later
}