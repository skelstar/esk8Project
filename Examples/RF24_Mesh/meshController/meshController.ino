#if (defined(__AVR__))
    #include <avr\ pgmspace.h> 
#else
    #include <pgmspace.h> 
#endif
#include <RF24Network.h> 
#include <RF24.h> 
#include <SPI.h>
#include "WiFi.h"

#define     NODE_BOARD          00
#define     NODE_CONTROLLER     01
#define     NODE_HUD            02
#define     NODE_TEST           03

// https://github.com/nRF24/RF24Network/blob/master/examples/Network_Ping/Network_Ping.ino

/***********************************************************************/
/***********************************************************************/
#define SPI_CE        5 //33    // white/purple
#define SPI_CS        13    //26  // green
// #define SPI_CE        33    // white/purple
// #define SPI_CS        26  // green

RF24 radio(SPI_CE, SPI_CS); // CE & CS pins to use (Using 7,8 on Uno,Nano)
RF24Network network(radio);

uint16_t this_node; // Our node address

const unsigned long interval = 200; // ms       // Delay manager to send pings regularly.
unsigned long last_time_sent;

unsigned long last_sent_to_board = 50;

unsigned long last_id_received = -1;
unsigned long current_id = 0;
//--------------------------------------------------------------
void setup() {

    Serial.begin(9600);
    const char filepath[] = __FILE__;
    Serial.printf("\n\r%s\n\r", filepath);

    uint64_t chipid = ESP.getEfuseMac();    //The chip ID is essentially its MAC address(length: 6 bytes).
    Serial.printf("ESP32 Chip ID = %04u \n", (uint16_t)(chipid>>32));   //print High 2 bytes

    WiFi.mode( WIFI_OFF );  // WIFI_MODE_NULL
    btStop();   // turn bluetooth module off

    this_node = NODE_CONTROLLER;
    Serial.printf("this_node: 0%o\n", this_node);
    Serial.printf("Sending every: %ums\n", interval);

    SPI.begin(); // Bring up the RF network
    radio.begin();
    radio.setPALevel(RF24_PA_MIN);
    radio.printDetails();
    
    network.begin( /*channel*/ 100, /*node address*/ this_node);
}
//--------------------------------------------------------------
int sendCount = 0;
int hubMs = 0;

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


    if (millis() - last_sent_to_board >= interval) {
        last_sent_to_board = millis();

        uint16_t to = NODE_BOARD;

        if ( send_T(to) == true ) { // Notify us of the result
            Serial.print("`");
        } else {
            Serial.print("x");
        }

        if (sendCount++ % 60 == 0) {
            Serial.println();
        }
    }

    if (millis() - last_time_sent >= interval) {
        last_time_sent = millis();

        uint16_t to = NODE_HUD;

        if ( send_T(to) == true ) { // Notify us of the result
            Serial.print(".");
            //Serial.printf(" %lu: APP Send ok \n\r", millis());
        } else {
            Serial.print("X");
            // Serial.printf(" %lu: APP Send to 0%o failed \n\r", to, millis());
        }

        if (sendCount++ % 60 == 0) {
            Serial.println();
        }
    }
}
//--------------------------------------------------------------
bool send_T(uint16_t to) {

    RF24NetworkHeader header( /*to node*/ to, /*type*/ 'T' /*Time*/ );

    // Serial.printf("---------------------------------\n\r");
    // Serial.printf("APP Sending %lu to 0%o...", current_id, to);
    unsigned long message = current_id++;
    return network.write(header, &message, sizeof(unsigned long));
}
//--------------------------------------------------------------
void handle_T(RF24NetworkHeader & header) {
    unsigned long message; // The 'T' message is just a ulong, containing the time
    network.read(header, &message, sizeof(unsigned long));
    Serial.printf("-----> Id Received %lu from 0%o\n\r", message, header.from_node);
}
//--------------------------------------------------------------
