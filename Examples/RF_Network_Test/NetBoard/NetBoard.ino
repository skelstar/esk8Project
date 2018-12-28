#include <SPI.h>
#include <RF24Network.h>
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

#define SPI_CE        33    // white/purple
#define SPI_CS        26  // green

#define ROLE_MASTER    		0
#define ROLE_BOARD    		2
#define ROLE_HUD    		3

RF24 radio(SPI_CE, SPI_CS);    // ce pin, cs pin
RF24Network network(radio); 

uint16_t  this_node = ROLE_BOARD;

int offlineCount = 0;
int encoderOfflineCount = 0;
bool encoderOnline = true;

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
    debug.print(STARTUP, "esk8Project/NetBoard.ino \n");

	SPI.begin();                                           // Bring up the RF network
	radio.begin();
	radio.setPALevel(RF24_PA_HIGH);
    radio.printDetails();
	network.begin(/*channel*/ 100, /*node address*/ this_node );

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

void loop() {

	network.update();

	if (network.available()) {
		readPacket();
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

bool sendPacket(uint16_t to) {
	
	RF24NetworkHeader header(/*to node*/ to, /*type*/ 'T' /*Time*/);

	unsigned long message = millis();
	printf_P(PSTR("---------------------------------\n\r"));
	printf_P(PSTR("%lu: APP Sending %lu to 0%o...\n\r"), millis(), message, to);	
	
	return network.write(header, &message, sizeof(unsigned long));
}

void readPacket() {
 	RF24NetworkHeader header;                            // If so, take a look at it
    network.peek(header);

	unsigned long message;                                                                      // The 'T' message is just a ulong, containing the time
	network.read(header, &message, sizeof(unsigned long));
	printf_P(PSTR("%lu: APP Received %lu from 0%o\n\r"), millis(), message, header.from_node);
}

