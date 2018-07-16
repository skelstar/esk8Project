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

uint8_t NODE_ADDRESS = 2; // Use numbers 0 through to select an address from the array

/***********************************************************************/
/***********************************************************************/

RF24 radio(22, 5); // CE & CS pins to use (Using 7,8 on Uno,Nano)
RF24Network network(radio);

uint16_t this_node; // Our node address

const unsigned long interval = 1000; // ms       // Delay manager to send pings regularly.
unsigned long last_time_sent;

const short max_active_nodes = 10; // Array of nodes we are aware of
uint16_t active_nodes[max_active_nodes];
short num_active_nodes = 0;
short next_ping_node_index = 0;

bool send_T(uint16_t to); // Prototypes for functions to send & handle messages
bool send_N(uint16_t to);
void handle_T(RF24NetworkHeader & header);
void handle_N(RF24NetworkHeader & header);
void add_node(uint16_t node);

uint8_t _throttle;

//--------------------------------------------------------------
void setup() {

    Serial.begin(9600);
    Serial.println("HUD: ready...");
    //printf_begin();
    Serial.printf("\n\rRF24Network/examples/meshping/\n\r");

    this_node = node_address_set[NODE_ADDRESS]; // Which node are we?

    SPI.begin(); // Bring up the RF network
    radio.begin();
    radio.setPALevel(RF24_PA_HIGH);
    network.begin( /*channel*/ 100, /*node address*/ this_node);

}
//--------------------------------------------------------------
void loop() {

    network.update(); 

    while (network.available()) { 

        RF24NetworkHeader header;
        network.peek(header);

        switch (header.type) { 
			case 'A':
				handle_Throttle(header);
				break;
	        default:
				Serial.printf("*** WARNING *** Unknown message type %c\n\r", header.type);
				network.read(header, 0, 0);
				break;
        };
    }
}
//--------------------------------------------------------------
void handle_Throttle(RF24NetworkHeader & header) {
	
	static uint8_t throttle = 127;
	network.read(header, &throttle, sizeof(throttle));
	_throttle = throttle;
	Serial.printf("Throttle from Controler: %d\n", _throttle);
}
//--------------------------------------------------------------
