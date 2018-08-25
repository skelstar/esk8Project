#if (defined(__AVR__))
    #include <avr\ pgmspace.h> 
#else
    #include <pgmspace.h> 
#endif
#include <RF24Network.h> 
#include <RF24.h> 
#include <SPI.h>


#define     NODE_BOARD          00
#define     NODE_CONTROLLER     01
#define     NODE_HUD            02
#define     NODE_TEST           03

// https://github.com/nRF24/RF24Network/blob/master/examples/Network_Ping/Network_Ping.ino

/***********************************************************************/
/***********************************************************************/
RF24 radio(33, 26); // CE & CS pins to use (Using 7,8 on Uno,Nano)
RF24Network network(radio);

uint16_t this_node; // Our node address

const unsigned long interval = 3000; // ms       // Delay manager to send pings regularly.
unsigned long last_time_sent;
unsigned long last_fail_time = 0;

unsigned long last_id_received = -1;
unsigned long current_id = 0;
//--------------------------------------------------------------
void setup() {

    Serial.begin(9600);
    const char filepath[] = __FILE__;
    Serial.printf("\n\r%s\n\r", filepath);

    uint64_t chipid = ESP.getEfuseMac();    //The chip ID is essentially its MAC address(length: 6 bytes).
    Serial.printf("ESP32 Chip ID = %04u \n", (uint16_t)(chipid>>32));//print High 2 bytes

    this_node = NODE_BOARD;
    Serial.printf("this_node: 0%o\n\n", this_node);

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
	        case 'T':
	            handle_T(header);
	            break;
	        default:
	            Serial.printf("*** WARNING *** Unknown message type %c\n\r", header.type);
	            network.read(header, 0, 0);
	            break;
        };
    }

    // if (millis() - last_time_sent >= interval) {
    //     last_time_sent = millis();

    //     uint16_t to = NODE_CONTROLLER;

    //     bool ok = send_T(to);

    //     if (ok) { // Notify us of the result
    //         // Serial.printf(" %lu: APP Send ok \n\r", millis());
    //     } else {
    //         Serial.printf("ERROR: Send to NODE_CONTROLLER (%u) failed \n\r", to);
    //     }
    // }
}
//--------------------------------------------------------------
bool send_T(uint16_t to) {
    RF24NetworkHeader header( /*to node*/ to, /*type*/ 'T' /*Time*/ );

    // The 'T' message that we send is just a ulong, containing the time
    unsigned long message = millis();
    // Serial.printf("---------------------------------\n\r");
    // Serial.printf("APP Sending %lu to 0%o...", current_id, to);
    return network.write(header, &current_id, sizeof(unsigned long));
}
//--------------------------------------------------------------
void handle_T(RF24NetworkHeader & header) {
    unsigned long message;
    network.read(header, & message, sizeof(unsigned long));

    if (header.from_node == NODE_CONTROLLER) {
    	if (last_id_received + 1 != message) {
    		Serial.printf("Sync Error! (missed %d packets from 02) - %u seconds\n", 
    			message - last_id_received, 
    			(millis()-last_fail_time)/1000);
    		last_fail_time = millis();
    	}
    	last_id_received = message;
    	if (last_id_received % 10 == 0) {
    		// if ( send_T(NODE_HUD) == false ) {
    		// 	Serial.printf("Failed sending to HUD \n");
    		// }
    		// if ( send_T(NODE_TEST) == false ) {
    		// 	Serial.printf("Failed sending to NODE_TEST (0%o) \n", NODE_TEST);
    		// }
    	}
    }
    else {
	    Serial.printf("-----> Id received %lu from 0%o\n\r", message, header.from_node);
    }
}
//--------------------------------------------------------------