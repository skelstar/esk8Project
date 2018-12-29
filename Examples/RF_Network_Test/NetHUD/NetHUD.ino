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
#define SPI_CE        		15
#define SPI_CS        		13
#define NRF24_POWER_PIN     2
// M5Stack
// #define SPI_CE        5 
// #define SPI_CS        13
// DEV board
// #define SPI_CE        33    	// white/purple
// #define SPI_CS        26  	// green


#define ROLE_MASTER    		0
#define ROLE_BOARD    		1
#define ROLE_HUD    		1

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
    debug.print(STARTUP, "esk8Project/NetHUD.ino \n");

    // nRF24 power
    pinMode(NRF24_POWER_PIN, OUTPUT);
    digitalWrite(NRF24_POWER_PIN, HIGH);

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

void loop() {

	network.update();

	if (network.available()) {
		
		char type = esk8.readPacket();

		if (esk8.state != OK) {
			if (esk8.state == esk8.MISSED_PACKET && millis()/1000 > 0) {
				debug.print(DEBUG, 
					"Missed %d packets from %s at %d minutes \n", 
					esk8.missingPackets, 
					type == 'C' ? "Controller" : "Board",
					millis()/1000/60);
				esk8.missingPackets = 0;
			}
			esk8.state = esk8.OK;
		}

		// if (type == 'C') {
		// 	debug.print(DEBUG, "Rxd from Controller: %d \n", esk8.controllerPacket.id);
		// }
		// else if (type == 'B') {
		// 	debug.print(DEBUG, "Rxd from Board: %d \n", esk8.boardPacket.id);	
		// }
		// else {
		// 	debug.print(DEBUG, "Unknown packet recieved: '%c' \n", type);		
		// }
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
//**************************************************************

void setupRadio() {
	SPI.begin();             
	radio.begin();
	esk8.begin(&radio, &network, esk8.RF24_HUD);
}


