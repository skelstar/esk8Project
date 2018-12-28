#include <SPI.h>
//#include <RF24Network.h>
#include <RF24.h> 
#include "WiFi.h"

#include <TaskScheduler.h>

#include <debugHelper.h>
#include <esk8Lib.h>

//--------------------------------------------------------------------------------

#define	STARTUP 		1 << 0
#define DEBUG 			1 << 1
#define COMMUNICATION 	1 << 2
#define HARDWARE		1 << 3
#define ONLINE_STATUS	1 << 5
#define TIMING			1 << 6

debugHelper debug;

esk8Lib esk8;

//--------------------------------------------------------------

portMUX_TYPE mmux = portMUX_INITIALIZER_UNLOCKED;

//--------------------------------------------------------------

#define SPI_CE        5 	// 33 dev    // white/purple
#define SPI_CS        13	// 26 dev   // green

#define ROLE_MASTER    		0
#define ROLE_BOARD    		1
#define ROLE_HUD    		2

RF24 radio(SPI_CE, SPI_CS);    // ce pin, cs pin

int radioNumber = ROLE_MASTER;

byte addresses[][6] = { "1Node", "2Node" };              // Radio pipe addresses for the 2 nodes to communicate.

byte counter = 0;

uint16_t  this_node = ROLE_MASTER;

//--------------------------------------------------------------------------------

const char compile_date[] = __DATE__ " " __TIME__;

//--------------------------------------------------------------

#define COMMS_TIMEOUT_PERIOD 	1000

/**************************************************************
					SETUP
**************************************************************/
void setup() {

	Serial.begin(9600);

	debug.init();
	debug.addOption(STARTUP, "STARTUP");
	debug.addOption(DEBUG, "DEBUG");
	debug.addOption(HARDWARE, "HARDWARE");
	debug.addOption(COMMUNICATION, "COMMUNICATION");
	debug.addOption(ONLINE_STATUS, "ONLINE_STATUS");
	debug.addOption(TIMING, "TIMING");
	//debug.setFilter( STARTUP | COMMUNICATION | ONLINE_STATUS | TIMING );
	debug.setFilter( STARTUP | DEBUG | ONLINE_STATUS );
	//debug.setFilter( STARTUP );

	debug.print(STARTUP, "%s \n", compile_date);
    debug.print(STARTUP, "esk8Project/NetController.ino \n");

	SPI.begin();                                           // Bring up the RF network
	radio.begin();
	radio.setPALevel(RF24_PA_HIGH);

	radio.enableAckPayload();                     // Allow optional ack payloads
	radio.enableDynamicPayloads();                // Ack payloads are dynamic payloads

	if (this_node == ROLE_MASTER) {
		radio.openWritingPipe(addresses[1]);
		radio.openReadingPipe(1, addresses[0]);
	}
	else {
		radio.openWritingPipe(addresses[0]);
		radio.openReadingPipe(1, addresses[1]);
	}
	radio.startListening();
	radio.writeAckPayload(1, &counter, 1);          // Pre-load an ack-paylod into the FIFO buffer for pipe 1
	radio.printDetails();

	// network.begin(channel 100, /*node address*/ this_node );

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
/**************************************************************
					LOOP
**************************************************************/

int millisUntilSendPacket = 500;
long now = 0;

void loop() {

	if (millis() - now > millisUntilSendPacket) {
		now = millis();
		sendPacket();
		// debug.print(DEBUG, "Sending: %s \n", sendBroadcastPacket(ROLE_BOARD) ? "OK!" : "FAILED");
	}

	vTaskDelay( 10 );
}
/**************************************************************
					TASK 0
**************************************************************/
void codeForEncoderTask( void *parameter ) {

	#define TX_INTERVAL 200
	long nowMs = 0;
	
	for (;;) {


		vTaskDelay( 10 );
	}

	vTaskDelete(NULL);
}
//**************************************************************

bool sendPacket() {
	
	byte rxByte;

	radio.stopListening();

	bool sentOk = radio.write(&counter, 1);

	if (sentOk) { 
		if (!radio.available()) {
			debug.print(DEBUG, "Blank response \n");
		}
		else {
			while (radio.available()) {
				radio.read(&rxByte, 1);
				debug.print(DEBUG, "Response: %d", rxByte);
			}
		}
	}

	counter++;
	return sentOk;
}

bool sendBroadcastPacket(uint16_t to) {
	// RF24NetworkHeader header(/*to node*/ to, /*type*/ 'T' /*Time*/);

	// unsigned long message = millis();
	
	// return network.multicast(header, &message, sizeof(unsigned long), 1);
}

void readPacket() {
//  	RF24NetworkHeader header;                            // If so, take a look at it
//     network.peek(header);

// 	unsigned long message;                                                                      // The 'T' message is just a ulong, containing the time
// 	network.read(header, &message, sizeof(unsigned long));
// 	printf_P(PSTR("%lu: APP Received %lu from 0%o\n\r"), millis(), message, header.from_node);
}


