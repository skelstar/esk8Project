#include <SPI.h>
#include <RF24Network.h>
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
RF24Network network(radio); 

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

long now = 0;
byte boardCounter = 0;

void loop() {

	network.update();

	while (network.available()) {
		esk8.readPacket();
		debug.print(DEBUG, "Rx from Controller: %d \n", esk8.controllerPacket.id);
	}

	bool timeToTx = millis()-now > 2000;
	if ( timeToTx ) {
		now = millis();
		esk8.sendPacket();
		debug.print(DEBUG, "Sent: %d \n", esk8.boardPacket.id);
		esk8.boardPacket.id++;
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
	SPI.begin();        
	radio.begin();
	esk8.begin(&radio, &network, esk8.RF24_BOARD);
}
