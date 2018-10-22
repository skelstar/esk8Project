#if (defined(__AVR__))
    #include <avr\ pgmspace.h> 
#else
    #include <pgmspace.h> 
#endif
#include <RF24Network.h> 
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
#define SPI_CE        5 //33    // white/purple
#define SPI_CS        13    //26  // green
// #define SPI_CE        33    // white/purple
// #define SPI_CS        26  // green

RF24 radio(SPI_CE, SPI_CS); // CE & CS pins to use (Using 7,8 on Uno,Nano)
RF24Network network(radio);

uint16_t this_node; // Our node address

unsigned long last_time_sent;

unsigned long last_sent_to_board = 50;

unsigned long last_id_received = -1;
unsigned long current_id = 0;

byte addresses[][6] = {"1Node","2Node"};

//--------------------------------------------------------------
void setup() {

    Serial.begin(9600);
    const char filepath[] = __FILE__;
    Serial.printf("\n\r%s\n\r", filepath);

    uint64_t chipid = ESP.getEfuseMac();    //The chip ID is essentially its MAC address(length: 6 bytes).
    Serial.printf("ESP32 Chip ID = %04u \n", (uint16_t)(chipid>>32));   //print High 2 bytes

    WiFi.mode( WIFI_OFF );  // WIFI_MODE_NULL
    btStop();   // turn bluetooth module off

    this_node = NODE_CONTROLLER;
    Serial.printf("this_node: 0%o\n", this_node);

    SPI.begin(); // Bring up the RF network
    radio.begin();
    radio.setPALevel(RF24_PA_MIN);
    radio.setAutoAck(1);                    // Ensure autoACK is enabled
  	radio.enableAckPayload();               // Allow optional ack payloadsradio.setRetries(0,15);                 // Smallest time between retries, max no. of retries
  	radio.openWritingPipe(addresses[1]);
    radio.openReadingPipe(1,addresses[0]);
    radio.printDetails();
    
    //network.begin( /*channel*/ 100, /*node address*/ this_node);

	esk8.begin(esk8.RF24_CONTROLLER);

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
int sendCount = 0;
int hubMs = 0;
int rxCount = 0;
#define TX_INTERVAL 200
/**************************************************************
					LOOP
**************************************************************/
void loop() {

	//checkForPacket();

    if (millis() - last_sent_to_board >= TX_INTERVAL) {
        last_sent_to_board = millis();

        if ( sendToBoard() == false ) {
            Serial.print("f");
         	rxCount++;
       	}
    }
	
	vTaskDelay( 10 );
}
/**************************************************************
					TASK 0
**************************************************************/
void codeForEncoderTask( void *parameter ) {

	// setupEncoder();

	// then loop forever	
	for (;;) {

		vTaskDelay( 10 );
	}

	vTaskDelete(NULL);
}
//**************************************************************
bool checkForPacket() {

	bool packetFound = false;
    return packetFound;
}
//--------------------------------------------------------------
// bool send_Multicast() {
//     RF24NetworkHeader header(00, /*type*/ 'T' /*Time*/ );

//     unsigned long message = 8888;
//     uint8_t level = 1;
//     return network.multicast(header, &message, sizeof(unsigned long), level);
// }
//--------------------------------------------------------------
bool sendToBoard() {

	radio.stopListening();

	bool sentOK = radio.write( &esk8.controllerPacket, sizeof(esk8.controllerPacket) );
	if ( sentOK == false ) {
		Serial.print("f");
	}

	radio.startListening();

	unsigned long started_waiting_at = millis();
    bool timeout = false;                    
    // wait until response has arrived
    if ( !radio.available() ){                             // While nothing is received
		Serial.printf("X");
		rxCount++;
	}
    else {
    	while ( radio.available() ) {
    		Serial.print("r");
    		rxCount++;
			radio.read( &esk8.boardPacket, sizeof(esk8.boardPacket) );
		}
    }
	
	if (rxCount > 30) {
		rxCount = 0;
		Serial.println();
	}

    return sentOK;
}
//--------------------------------------------------------------
