#include <SPI.h>
#include "RF24.h"
#include <esk8Lib.h>

/****************** User Config ***************************/
// ESP12e
bool radioNumber = 1;
#define SPI_CE		33	// white/purple
#define SPI_CS		26 	// green

// RADIO_HUZZAH;
// bool radioNumber = 1;
// #define SPI_CE		32//21	// white/purple
// #define SPI_CS		14//4 	// green

// ++ ESP12e
// #define 	SPI_MOSI		13 	// Blue
// #define 	SPI_MISO		12 	// Orange
// #define 	SPI_CLK			14 	// Yellow
// #define 	SPI_CS			15 		green
// #define 	SPI_CE			2

/* Hardware configuration: Set up nRF24L01 radio on SPI bus plus pins 7 & 8 */
RF24 radio(SPI_CE, SPI_CS);

#define 	ROLE_MASTER		1
#define 	ROLE_SLAVE		0

esk8Lib esk8;

/**********************************************************/
                                                                           // Topology
typedef enum { role_ping_out = 1, role_pong_back } role_e;                 // The various roles supported by this sketch
const char* role_friendly_name[] = { "invalid", "Ping out", "Pong back"};  // The debug-friendly names of those roles
role_e role = role_pong_back;                                              // The role of the current running sketch

byte counter = 1;                                                          // A single byte to keep track of the data being sent back and forth
//--------------------------------------------------------------------------------
void setup(){

	Serial.begin(9600);

	radio.begin();

	esk8.begin(&radio, ROLE_SLAVE, radioNumber);
}
//--------------------------------------------------------------------------------
void loop(void) {

	int result = esk8.pollMasterForPacket();
	if (result == true) {
		// success
		Serial.printf("Success!: %.1f\n", esk8.masterPacket.batteryVoltage);
	}

}	// loop
