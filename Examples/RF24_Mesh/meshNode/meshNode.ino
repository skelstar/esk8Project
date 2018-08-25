
/** RF24Mesh_Example.ino by TMRh20

   This example sketch shows how to manually configure a node via RF24Mesh, and send data to the
   master node.
   The nodes will refresh their network address as soon as a single write fails. This allows the
   nodes to change position in relation to each other and the master node.
*/


#include "RF24.h"
#include "RF24Network.h"
#include "RF24Mesh.h"
#include <SPI.h>
//#include <printf.h>


/**** Configure the nrf24l01 CE and CS pins ****/
RF24 radio(26, 33);
RF24Network network(radio);
RF24Mesh mesh(radio, network);

/**
   User Configuration: nodeID - A unique identifier for each radio. Allows addressing
   to change dynamically with physical changes to the mesh.

   In this example, configuration takes place below, prior to uploading the sketch to the device
   A unique value from 1-255 must be configured for each node.
   This will be stored in EEPROM on AVR devices, so remains persistent between further uploads, loss of power, etc.

 **/
#define nodeID 1


uint32_t displayTimer = 0;

struct payload_t {
  unsigned long ms;
  unsigned long counter;
};

void setup() {

	Serial.begin(9600);
	//printf_begin();
	// Set the nodeID manually

	uint64_t chipid = ESP.getEfuseMac();  //The chip ID is essentially its MAC address(length: 6 bytes).
	Serial.printf("ESP32 Chip ID = %04u ", (uint16_t)(chipid>>32));//print High 2 bytes
	// Serial.printf("%08X\n", (uint32_t)chipid);//print Low 4bytes.
	Serial.printf("%08u\n", (uint32_t)chipid);//print Low 4bytes.

	int this_node = 255;

	switch ((uint32_t) (chipid>>32)) {
		// case 36306:
		// 	this_node = 1; // Which node are we?
		// 	break;
		case 39989:
			this_node = 2; // Which node are we?
			break;
		case 10292:
			this_node = 3; // Which node are we?
			break;
		case 57808:
			this_node = 4; // Which node are we?
			break;
		default:
			Serial.printf("NOTE: node not configured! \n\n");
			break;    
	}

	Serial.printf("this_node: 0%o\n", this_node);

	mesh.setNodeID(nodeID);
	// Connect to the mesh
	Serial.println(F("Connecting to the mesh..."));
	mesh.begin();
}



void loop() {

	mesh.update();

	// Send to the master node every second
	if (millis() - displayTimer >= 1000) {
		displayTimer = millis();
		Serial.printf("Sending to master.. \n");

		// Send an 'M' type message containing the current millis()
		if (!mesh.write(&displayTimer, 'M', sizeof(displayTimer))) {

			// If a write fails, check connectivity to the mesh network
			if ( ! mesh.checkConnection() ) {
				//refresh the network address
				Serial.println("Renewing Address");
				mesh.renewAddress();
			} else {
				Serial.println("Send fail, Test OK");
			}
		} else {
			Serial.print("Send OK: "); Serial.println(displayTimer);
		}
	}

	while (network.available()) {
		
		RF24NetworkHeader header;
		
		payload_t payload;
		network.read(header, &payload, sizeof(payload));
		Serial.print("Received packet #");
		Serial.print(payload.counter);
		Serial.print(" at ");
		Serial.println(payload.ms);
	}
}





