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

uint8_t NODE_ADDRESS = 1; // Use numbers 0 through to select an address from the array

/***********************************************************************/
/***********************************************************************/

RF24 radio(33, 26); // CE & CS pins to use (Using 7,8 on Uno,Nano)
RF24Network network(radio);

uint16_t this_node; // Our node address

const unsigned long interval = 1000; // ms       // Delay manager to send pings regularly.
unsigned long last_time_sent;

const short max_active_nodes = 10; // Array of nodes we are aware of
uint16_t active_nodes[max_active_nodes];
short num_active_nodes = 0;
short next_ping_node_index = 0;

uint8_t _throttle;

bool send_T(uint16_t to); // Prototypes for functions to send & handle messages
bool send_N(uint16_t to);
void handle_T(RF24NetworkHeader & header);
void handle_N(RF24NetworkHeader & header);
void handler_Throttle(RF24NetworkHeader & header);
void add_node(uint16_t node);

void setup() {

    Serial.begin(9600);
    Serial.println("Board: ready...");
    //printf_begin();
    Serial.printf("\n\rRF24Network/examples/meshping/\n\r");

    this_node = node_address_set[NODE_ADDRESS]; // Which node are we?

    Serial.printf("Address: %0o \n", this_node);

    SPI.begin(); // Bring up the RF network
    radio.begin();
    radio.setPALevel(RF24_PA_HIGH);
    network.begin( /*channel*/ 100, /*node address*/ this_node);

}

void loop() {

    network.update(); // Pump the network regularly

    while (network.available()) { // Is there anything ready for us?

        RF24NetworkHeader header; // If so, take a look at it
        network.peek(header);

        switch (header.type) { // Dispatch the message to the correct handler.
	        // case 'T':
	        //     handle_T(header);
	        //     break;
	        // case 'N':
	        //     handle_N(header);
	        //     break;
	        case 'A':
	        	handler_Throttle(header);
	        	if (send_Throttle(00) == true) {
	        		Serial.printf("Send %d to 00 \n", _throttle);
	        	} else {
	        		Serial.printf("Failed to send %d to 00 \n", _throttle);
	        	}
	        	break;
	        default:
	            Serial.printf("*** WARNING *** Unknown message type %c\n\r", header.type);
	            network.read(header, 0, 0);
	            break;
        };
    }
}
//--------------------------------------------------------------
bool send_Throttle(uint16_t to) {

    RF24NetworkHeader header( to, 'A' );

    return network.write(header, &_throttle, sizeof(_throttle));
}
//--------------------------------------------------------------
void handler_Throttle(RF24NetworkHeader & header) {
	
	static uint8_t throttle = 127;
	network.read(header, &throttle, sizeof(throttle));
	_throttle = throttle;
	Serial.printf("Throttle from Controler: %d\n", _throttle);
}
