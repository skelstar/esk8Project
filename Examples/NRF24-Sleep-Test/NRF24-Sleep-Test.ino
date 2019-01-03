#include <SPI.h>
#include <RF24Network.h>
#include <RF24.h> 

#include <esk8Lib.h>

//--------------------------------------------------------------

esk8Lib esk8;

#define SPI_CE        33    	// white/purple
#define SPI_CS        26  	// green

RF24 radio(SPI_CE, SPI_CS);    // ce pin, cs pin
RF24Network network(radio); 

void packetAvailableCallback( uint16_t from ) {

	Serial.printf("%c", 'r');
}

//--------------------------------------------------------------

void setup() {
	Serial.begin(9600);

	SPI.begin();     
	#ifdef NRF24_POWER_PIN
	pinMode(NRF24_POWER_PIN, OUTPUT);
	digitalWrite(NRF24_POWER_PIN, HIGH);   
	#endif
	radio.begin();
	radio.setAutoAck(true);
	esk8.begin(&radio, &network, esk8.RF24_BOARD, packetAvailableCallback);
}

bool sleepDone = false;

void loop() {

	esk8.service();

	if (millis() > 5000 && sleepDone == false) {
		Serial.printf("Powering down!\n");
		powerDownRF24();
		sleepDone = true;
	}
}

void powerDownRF24() {
	radio.stopListening();
	radio.powerDown();
}


