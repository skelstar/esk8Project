/*
 Copyright (C) 2011 James Coliz, Jr. <maniacbug@ymail.com>
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */
/**
 * Example: Network topology, and pinging across a tree/mesh network
 *
 * Using this sketch, each node will send a ping to every other node in the network every few seconds. 
 * The RF24Network library will route the message across the mesh to the correct node.
 *
 * This sketch is greatly complicated by the fact that at startup time, each
 * node (including the base) has no clue what nodes are alive.  So,
 * each node builds an array of nodes it has heard about.  The base
 * periodically sends out its whole known list of nodes to everyone.
 *
 * To see the underlying frames being relayed, compile RF24Network with
 * #define SERIAL_DEBUG.
 *
 * Update: The logical node address of each node is set below, and are grouped in twos for demonstration.
 * Number 0 is the master node. Numbers 1-2 represent the 2nd layer in the tree (02,05).
 * Number 3 (012) is the first child of number 1 (02). Number 4 (015) is the first child of number 2.
 * Below that are children 5 (022) and 6 (025), and so on as shown below 
 * The tree below represents the possible network topology with the addresses defined lower down
 *
 *     Addresses/Topology                            Node Numbers  (To simplify address assignment in this demonstration)
 *             00                  - Master Node         ( 0 )
 *           02  05                - 1st Level children ( 1,2 )
 *    32 22 12    15 25 35 45    - 2nd Level children (7,5,3-4,6,8)
 *
 * eg:
 * For node 4 (Address 015) to contact node 1 (address 02), it will send through node 2 (address 05) which relays the payload
 * through the master (00), which sends it through to node 1 (02). This seems complicated, however, node 4 (015) can be a very
 * long way away from node 1 (02), with node 2 (05) bridging the gap between it and the master node.
 *
 * To use the sketch, upload it to two or more units and set the NODE_ADDRESS below. If configuring only a few
 * units, set the addresses to 0,1,3,5... to configure all nodes as children to each other. If using many nodes,
 * it is easiest just to increment the NODE_ADDRESS by 1 as the sketch is uploaded to each device.
 */
#if (defined(__AVR__))
	#include <avr\ pgmspace.h> 
#else
	#include <pgmspace.h> 
#endif
#include <RF24Network.h> 
#include <RF24.h> 
#include <SPI.h>
#include <debugHelper.h>

#define SERIAL_DEBUG

//--------------------------------------------------------------

#define	STARTUP 			1 << 0	// 0
#define WARNING 			1 << 1	// 2
#define ERROR 				1 << 2	// 4
#define PRINT_OK 			1 << 3	// 8
#define PRINT_RX 			1 << 4	// 16
#define PRINT_TX			1 << 5	// 32

debugHelper debug;

/***********************************************************************
************* Set the Node Address *************************************
/***********************************************************************/

// https://github.com/nRF24/RF24Network/blob/master/examples/Network_Ping/Network_Ping.ino

/***********************************************************************/
/***********************************************************************/
RF24 radio(33, 26); // CE & CS pins to use (Using 7,8 on Uno,Nano)
RF24Network network(radio);

uint16_t this_node; // Our node address

unsigned long interval = 1000; // ms       // Delay manager to send pings regularly.
unsigned long last_time_sent;

const short max_active_nodes = 10; // Array of nodes we are aware of
uint16_t active_nodes[max_active_nodes];
bool other_nodes[max_active_nodes];
int num_fails_before_remove = 5;
int fail_count[max_active_nodes];

short num_active_nodes = 0;
short next_ping_node_index = 0;

bool send_T(uint16_t to); // Prototypes for functions to send & handle messages
bool send_N(uint16_t to);
void handle_T(RF24NetworkHeader & header);
void handle_N(RF24NetworkHeader & header);
void add_node(uint16_t node);

void setup() {

    Serial.begin(9600);

	debug.init();
	debug.addOption(WARNING, "WARNING");
	debug.addOption(STARTUP, "STARTUP");
	debug.addOption(ERROR, "ERROR");
	debug.addOption(PRINT_OK, "PRINT_OK");
	debug.addOption(PRINT_RX, "");
	debug.addOption(PRINT_TX, "PRINT_TX");

    uint64_t chipid = ESP.getEfuseMac();	//The chip ID is essentially its MAC address(length: 6 bytes).
	switch ((uint32_t) (chipid>>32)) {
		case 36306:
	    	this_node = 0; // 00
			debug.setFilter( STARTUP | ERROR | PRINT_RX );
	    	break;
    	case 39989:
		    this_node = 1;
			debug.setFilter( STARTUP | ERROR | PRINT_RX );
		    break;
	    case 10292:
	    	// multicast
		    this_node = 2;
		    interval = 5000;
			debug.setFilter( STARTUP | ERROR | PRINT_RX );
			break;
	    case 57808:		// antenna
		    this_node = 3;
			debug.setFilter( STARTUP | ERROR | PRINT_RX );
			break;
		case 39045:
		    this_node = 4;
			debug.setFilter( STARTUP | ERROR | PRINT_RX );
			break;
		case 59618:
			this_node = 5;
			debug.setFilter( STARTUP | ERROR | PRINT_RX );
			break;
		default:
			debug.setFilter( STARTUP | ERROR | PRINT_RX );
			debug.print(STARTUP, "NOTE: node not configured! \n\n");
			break;		
	}

    debug.print(STARTUP, "\n\r\\esk8Project\\Examples\\RF24_Mesh\\NetworkGeneric\n\r");

	debug.print(STARTUP, "ESP32 Chip ID = %04u \n", (uint16_t)(chipid>>32));//print High 2 bytes
	debug.print(STARTUP, "%08u\n", (uint32_t)chipid);//print Low 4bytes.

    debug.print(STARTUP, "this_node: 0%d\n", this_node);

    SPI.begin(); // Bring up the RF network
    radio.begin();
    radio.setPALevel(RF24_PA_HIGH);
    radio.printDetails();
    network.begin( /*channel*/ 100, /*node address*/ this_node);

    if ( this_node > 0 ) {
    	network.multicastLevel( 1 );
    }
}

void loop() {

    network.update(); // Pump the network regularly
    while (network.available()) { // Is there anything ready for us?

        RF24NetworkHeader header; // If so, take a look at it
        network.peek(header);

        switch (header.type) { // Dispatch the message to the correct handler.
        case 'T':
            handle_T(header);
            break;
        case 'N':
            handle_N(header);
            break;
        default:
            debug.print(WARNING, "*** WARNING *** Unknown message type %c\n\r", header.type);
            network.read(header, 0, 0);
            break;
        };
    }

    unsigned long now = millis(); // Send a ping to the next node every 'interval' ms
    if (now - last_time_sent >= interval) {
        last_time_sent = now;

        uint16_t to = getNextNodeToSendTo();

        bool ok;

        if (this_node == 2) {
        	if ( send_T(-1) ) { // Notify us of the result
	            fail_count[to] = 0;
	            debug.print(PRINT_OK, "send_T() ---> 0%d... OK! \n\r", to);
        	} else {
            	fail_count[to]++;
            	debug.print(ERROR, "send_T() ---> 0%d... FAILED!!! (count = %d) \n\r", to, fail_count[to]);
            	if (fail_count[to] >= num_fails_before_remove) {
            		remove_other_node(to);
            	}
	            last_time_sent -= 100; // Try sending at a different time next time

		        	// send to root
		        if ( send_T(0) == false ) {
	            	debug.print(ERROR, "send_T() ---> 0... FAILED!!! \n\r");
		        }
	        }
        }
        else if (this_node != to) {

	        if ( this_node > 0 ) { // Normal nodes send a 'T' ping
	            ok = send_T(to);
	        } else { // Base node sends the current active nodes out
        	    debug.print(PRINT_TX, "---------------------------------\n\r");
			    debug.print(PRINT_TX, "(N) Sending active nodes ---> 0%d...", to);
	            ok = send_N(to);
	        }

	        if ( ok ) { // Notify us of the result
	            fail_count[to] = 0;
	            debug.print(PRINT_OK, "send_T() ---> 0%d... OK! \n\r", to);
        	} else {
            	fail_count[to]++;
            	debug.print(ERROR, "send_T() ---> 0%d... FAILED!!! (count = %d) \n\r", to, fail_count[to]);
            	if (fail_count[to] >= num_fails_before_remove) {
            		remove_other_node(to);
            	}
	            last_time_sent -= 100; // Try sending at a different time next time
	        }
	    }
    }
    //  delay(50);                          // Delay to allow completion of any serial printing
    //  if(!network.available()){
    //      network.sleepNode(2,0);         // Sleep this node for 2 seconds or a payload is received (interrupt 0 triggered), whichever comes first
    //  }
}

/**

 */
int16_t getNextNodeToSendTo() {
	uint16_t _to = 0; // Who should we send to? By default, send to base

	if (num_active_nodes) { // Or if we have active nodes,
		_to = active_nodes[next_ping_node_index++]; // Send to the next active node
		if (next_ping_node_index > num_active_nodes) { // Have we rolled over?
			next_ping_node_index = 0; // Next time start at the beginning
			_to = 0; // This time, send to node 0.
		}
	}
	return _to;
}
/**
 * Send a 'T' message, the current time
 */
bool send_T(uint16_t to) {
    RF24NetworkHeader header( /*to node*/ to, /*type*/ 'T' /*Time*/ );

    // The 'T' message that we send is just a ulong, containing the time
    unsigned long message = millis();

    if (this_node == 2) {
    	return network.multicast(header, &message, sizeof(unsigned long), 1);
    }
    return network.write(header, &message, sizeof(unsigned long));
}
/**
 * Send an 'N' message, the active node list
 */
bool send_N(uint16_t to) {
    RF24NetworkHeader header( /*to node*/ to, /*type*/ 'N' /*Time*/ );
    return network.write(header, active_nodes, sizeof(active_nodes));
}
/**
 * Handle a 'T' message
 * Add the node to the list of active nodes
 */
void handle_T(RF24NetworkHeader & header) {
    unsigned long message; // The 'T' message is just a ulong, containing the time
    network.read(header, & message, sizeof(unsigned long));
    if ( header.to_node == 64 ) {
	    debug.print(PRINT_RX, "-----> Received %lu from 0%d (MULTICAST) \n\r", message, header.from_node);
    }
    else {
	    debug.print(PRINT_RX, "-----> Received %lu from 0%d \n\r", message, header.from_node);
	}
    if (header.from_node != this_node || header.from_node > 0) {
        add_node(header.from_node);
    }
}
/**
 * Handle an 'N' message, the active node list
 */
void handle_N(RF24NetworkHeader & header) {
    
    static uint16_t incoming_nodes[max_active_nodes];
    network.read(header, &incoming_nodes, sizeof(incoming_nodes));

    if (debug.hasOption(PRINT_RX)) {
        Serial.printf("---------------------------------------------\n");
	    Serial.printf("-----> Received nodes from 0%d: ", header.from_node);
	    int i = 0;
	    while (i < max_active_nodes && incoming_nodes[i] > 0) {
	    	if (incoming_nodes[i] == this_node) {
		    	Serial.printf("*%d ", incoming_nodes[i]);
	    	}
	    	else {
		    	Serial.printf("%d ", incoming_nodes[i]);
	    	}
	        add_node(incoming_nodes[i++]);
	    }
        Serial.printf("\n---------------------------------------------\n");
	}
}
/**
 * Add a particular node to the current list of active nodes
 */
void add_node(uint16_t node) {

	if (other_nodes[node] == false) {
		other_nodes[node] = true;
		fail_count[node] = 0;
	}

    short i = num_active_nodes; // Do we already know about this node?
    while (i--) {
        if (active_nodes[i] == node)
            break;
    }

    if (i == -1 && num_active_nodes < max_active_nodes) { // If not, add it to the table
        active_nodes[num_active_nodes++] = node;
        debug.print(PRINT_RX, "%lu: +++ Added 0%d to list of active nodes.\n\r", millis(), node);
    }
}

void remove_other_node(uint16_t otherNode) {
	other_nodes[otherNode] = false;
	debug.print(WARNING, "Removed node from list -> %d \n", otherNode);
}