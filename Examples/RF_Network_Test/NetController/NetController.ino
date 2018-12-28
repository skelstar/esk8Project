#include <SPI.h>
#include <RF24.h> 

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

// TTGO-TQ
// #define SPI_CE        15
// #define SPI_CS        13
// #define NRF24_POWER_PIN        2
// M5Stack
#define SPI_CE        5 
#define SPI_CS        13
// DEV board
// #define SPI_CE        33    	// white/purple
// #define SPI_CS        26  	// green

#define ROLE_MASTER    		0
#define ROLE_BOARD    		1
#define ROLE_HUD    		2

RF24 radio(SPI_CE, SPI_CS);    // ce pin, cs pin

int radioNumber = ROLE_MASTER;

byte pipes[][6] = { "1Node", "2Node" };              // Radio pipe addresses for the 2 nodes to communicate.

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

	setupRadio();

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

void setupRadio() {
	SPI.begin();                                           // Bring up the RF network
	radio.begin();
	radio.setAutoAck(1);                    // Ensure autoACK is enabled
	radio.enableAckPayload();               // Allow optional ack payloads
	radio.setRetries(0,15);                 // Smallest time between retries, max no. of retries
	radio.setPayloadSize(1);                // Here we are sending 1-byte payloads to test the call-response speed
	radio.openWritingPipe(pipes[1]);        // Both radios listen on the same pipes by default, and switch when writing
	radio.openReadingPipe(1,pipes[0]);
	radio.startListening();                 // Start listening
	radio.printDetails();                   // Dump the configuration of the rf unit for debugging
}

bool sendPacket() {
	
	byte gotByte;

	radio.stopListening();

	bool sentOk = radio.write(&counter, ROLE_BOARD);

	if (sentOk) { 
		if ( radio.available() == false ) {
			debug.print(DEBUG, "Blank response \n");
		}
		else {
			while ( radio.available() ) {
				radio.read(&gotByte, 1);
				debug.print(DEBUG, "Response: %d \n", gotByte);
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


