#if (defined(__AVR__))
    #include <avr\ pgmspace.h> 
#else
    #include <pgmspace.h> 
#endif
//#include <RF24Network.h> 
#include <RF24.h> 
#include <SPI.h>
#include "WiFi.h"
#include <esk8Lib.h>

#define     NODE_BOARD          00
#define     NODE_CONTROLLER     01
#define     NODE_HUD            02
#define     NODE_TEST           03

esk8Lib esk8;

// https://github.com/nRF24/RF24Network/blob/master/examples/Network_Ping/Network_Ping.ino

/***********************************************************************/
/***********************************************************************/
RF24 radio(33, 26); // CE & CS pins to use (Using 7,8 on Uno,Nano)
RF24Network network(radio);

uint16_t this_node; // Our node address

unsigned long last_time_sent;
unsigned long last_fail_time = 0;

unsigned long last_id_received = -1;
unsigned long current_id = 0;

byte addresses[][6] = {"1Node","2Node"};
//--------------------------------------------------------------
void setup() {

    Serial.begin(9600);
    const char filepath[] = __FILE__;
    Serial.printf("\n\r%s\n\r", filepath);

    uint64_t chipid = ESP.getEfuseMac();    //The chip ID is essentially its MAC address(length: 6 bytes).
    Serial.printf("ESP32 Chip ID = %04u \n", (uint16_t)(chipid>>32));//print High 2 bytes

    WiFi.mode( WIFI_OFF );  // WIFI_MODE_NULL
    btStop();   // turn bluetooth module off

    this_node = NODE_BOARD;
    Serial.printf("this_node: 0%o - NODE_BOARD\n\n", this_node);

    SPI.begin(); // Bring up the RF network
    radio.begin();
    radio.setPALevel(RF24_PA_MIN);
    radio.setAutoAck(1);                    // Ensure autoACK is enabled
  	radio.enableAckPayload();               // Allow optional ack payloadsradio.setRetries(0,15);                 // Smallest time between retries, max no. of retries
    radio.printDetails();
    radio.openWritingPipe(addresses[0]);
	radio.openReadingPipe(1,addresses[1]);
	radio.startListening();

    // network.begin( /*channel*/ 100, /*node address*/ this_node);

	esk8.begin(esk8.RF24_BOARD);

	xTaskCreatePinnedToCore (
		codeForEncoderTask,	// function
		"Task_Encoder",		// name
		10000,			// stack
		NULL,			// parameter
		1,				// priority
		NULL,	// handle
		0
	);				// port	
}
//--------------------------------------------------------------

int rxCount = 0;
long nowMs = 0;
#define TX_INTERVAL 500
/**************************************************************
					LOOP
**************************************************************/
void loop() {
	
	vTaskDelay( 10 );
}
/**************************************************************
					TASK 0
**************************************************************/
void codeForEncoderTask( void *parameter ) {

	// setupEncoder();

	long now = millis();

	// then loop forever	
	for (;;) {
		checkForPacketThenAnswer();

	    // if (millis() - nowMs > TX_INTERVAL) {
	    //     nowMs = millis();

	    //     if ( sendToController() == false ) {
	    //     	Serial.print("f");
	    //     	rxCount++;
	    //     }
	    // }
		vTaskDelay( 10 );
	}

	vTaskDelete(NULL);
}
//**************************************************************
//--------------------------------------------------------------
bool checkForPacketThenAnswer() {

	bool packetFound = false;
	byte pipeNo;

	// pong back
	if( radio.available(&pipeNo) ){
		while ( radio.available() ) {
			Serial.print(".");
			rxCount++;
			if (rxCount > 30) {
				Serial.println();
				rxCount = 0;
			}
			radio.read( &esk8.controllerPacket, sizeof(esk8.controllerPacket) );
			radio.writeAckPayload(pipeNo, &esk8.boardPacket, sizeof(esk8.boardPacket));
		}
		packetFound = true;
   	}
    return packetFound;
}
//--------------------------------------------------------------
int send_Multicast() {
    RF24NetworkHeader header(00, /*type*/ 'T' /*Time*/ );

    unsigned long message = 9999;
    uint8_t level = 1;
    return network.multicast(header, &message, sizeof(unsigned long), level);
}
//--------------------------------------------------------------
bool checksumMatches(unsigned long message) {
    return last_id_received + 1 == message;
}
//--------------------------------------------------------------
bool sendToController() {

    // RF24NetworkHeader header( /*to node*/ esk8.RF24_CONTROLLER, /*type*/ 'T' /*Time*/ );
    // esk8.boardPacket.id++;
    // return network.write(header, &esk8.boardPacket, sizeof(esk8.boardPacket));
}
//--------------------------------------------------------------
