#include <SPI.h>
#include <RF24Network.h>
#include <RF24.h> 

//--------------------------------------------------------------------------------

// TTGO-TQ
// #define SPI_CE        15
// #define SPI_CS        13
// #define NRF24_POWER_PIN        2
// M5Stack
// #define SPI_CE        5 
// #define SPI_CS        13
// DEV board
#define SPI_CE        33    	// white/purple
#define SPI_CS        26  	// green

RF24 radio(SPI_CE, SPI_CS);    // ce pin, cs pin
RF24Network network(radio); 

const uint16_t board_node = 00;
const uint16_t controller_node = 01;
const uint16_t passive_node = 02;

struct payload_t {                 // Structure of our payload
  unsigned long ms;
  unsigned long counter;
};

unsigned long lastCounter = 0;

//--------------------------------------------------------------------------------

/**************************************************************
					SETUP
**************************************************************/
void setup() {

	Serial.begin(9600);
	Serial.printf("\nready\n");

	SPI.begin();
	radio.begin();
	network.begin(/*channel*/ 90, /*node address*/ passive_node);
}
/**************************************************************
					LOOP
**************************************************************/

int numDots = 0;

void loop() {
	network.update();

	while ( network.available() ) {
		RF24NetworkHeader header;        // If so, grab it and print it out
		payload_t payload;
		network.read(header, &payload, sizeof(payload));

		if (lastCounter != 0 && lastCounter != payload.counter - 1) {
			Serial.printf("\nMissed %u packets\n", payload.counter - lastCounter + 1);
		}
		else {
			if (numDots++ < 90) {
				Serial.printf(".");
			}
			else {
				Serial.printf(".\n");
				numDots = 0;
			}
		}
		lastCounter = payload.counter;
	}
}
