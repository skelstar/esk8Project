#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>

// TTGO-TQ
// #define SPI_CE        15
// #define SPI_CS        13
// #define NRF24_POWER_PIN        2
// M5Stack
// #define SPI_CE        5 
// #define SPI_CS        13
// DEV board
#define SPI_CE        33    	// white/purple
#define SPI_CS        26  	// green

RF24 radio(SPI_CE, SPI_CS);    // ce pin, cs pin
RF24Network network(radio); 

const uint16_t board_node = 00;
const uint16_t controller_node = 01;
const uint16_t passive_node = 02;

const unsigned long interval = 200; //ms  // How often to send 'hello world to the other unit

unsigned long last_sent;             // When did we last send?
unsigned long packets_sent;          // How many have we sent already

struct payload_t {                  // Structure of our payload
  unsigned long ms;
  unsigned long counter;
};


void setup(void)
{
  Serial.begin(9600);
  Serial.println("\nready\n");
 
  SPI.begin();
  radio.begin();
  network.begin(/*channel*/ 90, /*node address*/ controller_node);
}
void loop() {
  
  network.update();                          // Check the network regularly
  
  unsigned long now = millis();              // If it's time to send a message, send it!
  if ( now - last_sent >= interval  )
  {
    last_sent = now;
    Serial.print(".");
    payload_t payload = { millis(), packets_sent++ };
    RF24NetworkHeader header(/*to node*/ board_node);
    bool ok = network.write(header,&payload,sizeof(payload));
    if (!ok) {
      Serial.printf("failed. (%u)\n", payload.counter);
	}
  }
}