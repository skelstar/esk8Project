#if (defined(__AVR__))
	#include <avr\ pgmspace.h > 
#else
	#include <pgmspace.h > 
#endif
#include <RF24.h > 
#include <RF24Network.h > 
#include <SPI.h >
// #include "//printf.h"

/***********************************************************************
************* Set the Node Address *************************************
/***********************************************************************/

// These are the Octal addresses that will be assigned
const uint16_t node_address_set[10] = { 00, 02, 05, 012, 015, 022, 025, 032, 035, 045 };

// 0 = Master
// 1-2 (02,05)   = Children of Master(00)
// 3,5 (012,022) = Children of (02)
// 4,6 (015,025) = Children of (05)
// 7   (032)     = Child of (02)
// 8,9 (035,045) = Children of (05)

uint8_t NODE_ADDRESS = 0; // Use numbers 0 through to select an address from the array
#define BOARD_ADDRESS_INDEX	1
#define HUD_ADDRESS_INDEX	2	

/***********************************************************************/
/***********************************************************************/

RF24 radio(33, 26); // CE & CS pins to use (Using 7,8 on Uno,Nano)
RF24Network network(radio);

uint16_t this_node; // Our node address

const unsigned long interval = 1000; // ms       // Delay manager to send pings regularly.
unsigned long last_time_sent;

long _lastSentToBoard;

const short max_active_nodes = 10; // Array of nodes we are aware of
uint16_t active_nodes[max_active_nodes];
short num_active_nodes = 0;
short next_ping_node_index = 0;


//--------------------------------------------------------------

void setup() {
    Serial.begin(9600);
    Serial.println("Controller: ready...");
    Serial.printf("\n\rRF24Network/examples/meshping/\n\r");

    this_node = node_address_set[NODE_ADDRESS]; // Which node are we?

    Serial.printf("Address: %0o \n", this_node);

    SPI.begin(); // Bring up the RF network
    
    radio.begin();
    radio.setPALevel(RF24_PA_HIGH);
    
    network.begin( /*channel*/ 100, /*node address*/ this_node);
}

//--------------------------------------------------------------

void loop() {
    network.update(); // Pump the network regularly

    while (network.available()) { // Is there anything ready for us?

        RF24NetworkHeader header; // If so, take a look at it
        network.peek(header);

        switch (header.type) { // Dispatch the message to the correct handler.
			case 'A':
				handle_Throttle(header);
				break;
	        default:
				Serial.printf("*** WARNING *** Unknown message type %c\n\r", header.type);
				network.read(header, 0, 0);
				break;
        };
    }

    if (millis() - last_time_sent >= interval) {
    	last_time_sent = millis();

    	uint16_t to = node_address_set[BOARD_ADDRESS_INDEX];
    	_lastSentToBoard = millis();

    	bool ok = send_Throttle(to);
    	if (ok) {
			Serial.printf("Throttle sent ok\n");
    	}
    	else {
			Serial.printf("Throttle sent FAILED\n");
    	}
	}
}
//--------------------------------------------------------------
bool send_Throttle(uint16_t to) {
	static uint8_t throttle = 132;
	RF24NetworkHeader header( to, 'A' );

    Serial.printf("---------------------------------\n\r");
    Serial.printf("%lu: APP Sending Throttle to 0%o...\n\r", millis(), to);
    _lastSentToBoard = millis();
    //return network.write(header, &throttle, sizeof(throttle));	
    return network.multicast( header, &throttle, sizeof(throttle), /* level */ 1 );
}
//--------------------------------------------------------------
void handle_Throttle(RF24NetworkHeader & header) {
	static uint8_t throttle = 132;

	long timeTaken = millis() - _lastSentToBoard;

    network.read(header, &throttle, sizeof(throttle));
    Serial.printf("%lu: APP Received throttle from 0%o, time taken: %lu \n\r ", 
    	millis(), header.from_node, timeTaken);
}
//--------------------------------------------------------------
