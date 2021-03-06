#if (defined(__AVR__))
    #include <avr\ pgmspace.h> 
#else
    #include <pgmspace.h> 
#endif
#include <RF24.h> 
#include <SPI.h>
#include "WiFi.h"
#include <esk8Lib.h>

#define     NODE_BOARD          00
#define     NODE_CONTROLLER     01
#define     NODE_HUD            02
#define     NODE_TEST           03

esk8Lib esk8;

/***********************************************************************/
/***********************************************************************/
RF24 radio(33, 26); // CE & CS pins to use (Using 7,8 on Uno,Nano)

uint16_t this_node; // Our node address

unsigned long last_time_sent;
unsigned long last_fail_time = 0;

unsigned long last_id_received = -1;
unsigned long current_id = 0;

// byte addresses[][6] = {"1Node","2Node"};
//--------------------------------------------------------------
void setup() {

    Serial.begin(9600);
    const char filepath[] = __FILE__;
    Serial.printf("\n\r%s\n\r", filepath);

    uint64_t chipid = ESP.getEfuseMac();    //The chip ID is essentially its MAC address(length: 6 bytes).
    Serial.printf("ESP32 Chip ID = %04u \n", (uint16_t)(chipid>>32));//print High 2 bytes

    WiFi.mode( WIFI_OFF );  // WIFI_MODE_NULL
    btStop();   // turn bluetooth module off

    SPI.begin(); // Bring up the RF network
    radio.begin();
    radio.setPALevel(RF24_PA_MIN);
    radio.setAutoAck(1);                    // Ensure autoACK is enabled
  	radio.enableAckPayload();               // Allow optional ack payloadsradio.setRetries(0,15);                 // Smallest time between retries, max no. of retries
    radio.printDetails();

  	radio.openWritingPipe( esk8.talking_pipes[esk8.RF24_BOARD] );
  	radio.openReadingPipe( 1, esk8.listening_pipes[esk8.RF24_BOARD] );

	radio.startListening();

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
#define TX_HUD_INTERVAL 2000
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
		checkForControllerPacketThenAnswer();

	    // if (millis() - nowMs > TX_HUD_INTERVAL) {
	    //     nowMs = millis();

     //     	Serial.print( sendToHUD() ? "-" : "f" );
	    //     rxCount++;
	    // }
		vTaskDelay( 10 );
	}

	vTaskDelete(NULL);
}
/**************************************************************
		
**************************************************************/
bool checkForControllerPacketThenAnswer() {

	bool packetFound = false;
	byte pipeNo;

  	radio.openWritingPipe( esk8.talking_pipes[esk8.RF24_BOARD] );
  	radio.openReadingPipe( 1, esk8.listening_pipes[esk8.RF24_BOARD] );

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
			vTaskDelay( 1 );
		}
		packetFound = true;
   	}
    return packetFound;
}
//--------------------------------------------------------------
bool checksumMatches(unsigned long message) {
    return last_id_received + 1 == message;
}
//--------------------------------------------------------------
