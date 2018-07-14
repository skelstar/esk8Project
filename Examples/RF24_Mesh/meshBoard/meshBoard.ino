#if (defined(__AVR__))
#include <avr\pgmspace.h>
#else
#include <pgmspace.h>
#endif
#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>
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

uint8_t NODE_ADDRESS = 1;  // Use numbers 0 through to select an address from the array

/***********************************************************************/
/***********************************************************************/


RF24 radio(33, 26);                              // CE & CS pins to use (Using 7,8 on Uno,Nano)
RF24Network network(radio); 

uint16_t this_node;                           // Our node address

const unsigned long interval = 1000; // ms       // Delay manager to send pings regularly.
unsigned long last_time_sent;


const short max_active_nodes = 10;            // Array of nodes we are aware of
uint16_t active_nodes[max_active_nodes];
short num_active_nodes = 0;
short next_ping_node_index = 0;


bool send_T(uint16_t to);                      // Prototypes for functions to send & handle messages
bool send_N(uint16_t to);
void handle_T(RF24NetworkHeader& header);
void handle_N(RF24NetworkHeader& header);
void add_node(uint16_t node);


void setup(){
  
  Serial.begin(9600);
  Serial.println("Board: ready...");
  //printf_begin();
  Serial.printf("\n\rRF24Network/examples/meshping/\n\r");

  this_node = node_address_set[NODE_ADDRESS];            // Which node are we?
  
  SPI.begin();                                           // Bring up the RF network
  radio.begin();
  radio.setPALevel(RF24_PA_HIGH);
  network.begin(/*channel*/ 100, /*node address*/ this_node );

}

void loop(){
    
  network.update();                                      // Pump the network regularly

   while ( network.available() )  {                      // Is there anything ready for us?
     
		RF24NetworkHeader header;                            // If so, take a look at it
		network.peek(header);
    
		switch (header.type){                              // Dispatch the message to the correct handler.
			case 'T': 
				handle_T(header); 
				break;
			case 'N': 
				handle_N(header); 
				break;
			default:  
				Serial.printf("*** WARNING *** Unknown message type %c\n\r", header.type);
				network.read(header,0,0);
				break;
		};
    }
  
	unsigned long now = millis();                         // Send a ping to the next node every 'interval' ms
	if ( now - last_time_sent >= interval ) {
		last_time_sent = now;

		uint16_t to = 00;                                   // Who should we send to? By default, send to base


		if ( num_active_nodes ){                            // Or if we have active nodes,
		    to = active_nodes[next_ping_node_index++];      // Send to the next active node
		    if ( next_ping_node_index > num_active_nodes ){ // Have we rolled over?
				next_ping_node_index = 0;                   // Next time start at the beginning
				to = 00;                                    // This time, send to node 00.
		    }
		}

		bool ok;

		if ( this_node > 00 || to == 00 ){                    // Normal nodes send a 'T' ping
			if (this_node != to) {
				if ( send_T(to) == true ) {
					Serial.printf("%lu: APP Send ok\n\r", millis());
				} else {
		    	    Serial.printf("%lu: APP Send failed\n\r", millis());
			        last_time_sent -= 100;                            // Try sending at a different time next time
				}
			} else {

			}
		} else {                                                // Base node sends the current active nodes out
		    if ( send_N(to) == true ) {
		        Serial.printf("%lu: APP Send ok\n\r", millis());
		    } else {
		        Serial.printf("%lu: APP Send failed\n\r", millis());
		        last_time_sent -= 100;                            // Try sending at a different time next time
		    }
		}
	}
}

/**
 * Send a 'T' message, the current time
 */
bool send_T(uint16_t to)
{
  RF24NetworkHeader header(/*to node*/ to, /*type*/ 'T' /*Time*/);
  
  // The 'T' message that we send is just a ulong, containing the time
  unsigned long message = millis();
  Serial.printf("---------------------------------\n\r");

  Serial.printf("%lu: APP Sending %lu to 0%o...\n\r", millis(),message,to);
  return network.write(header,&message,sizeof(unsigned long));
}

/**
 * Send an 'N' message, the active node list
 */
bool send_N(uint16_t to)
{
  RF24NetworkHeader header(/*to node*/ to, /*type*/ 'N' /*Time*/);
  
  Serial.printf("---------------------------------\n\r");
  Serial.printf("%lu: APP Sending active nodes to 0%o...\n\r", millis(),to);
  return network.write(header,active_nodes,sizeof(active_nodes));
}

/**
 * Handle a 'T' message
 * Add the node to the list of active nodes
 */
void handle_T(RF24NetworkHeader& header){

  unsigned long message;                                                                      // The 'T' message is just a ulong, containing the time
  network.read(header,&message,sizeof(unsigned long));
  Serial.printf("%lu: APP Received %lu from 0%o\n\r", millis(),message,header.from_node);


  if ( header.from_node != this_node || header.from_node > 00 )                                // If this message is from ourselves or the base, don't bother adding it to the active nodes.
    add_node(header.from_node);
}

/**
 * Handle an 'N' message, the active node list
 */
void handle_N(RF24NetworkHeader& header)
{
  static uint16_t incoming_nodes[max_active_nodes];

  network.read(header,&incoming_nodes,sizeof(incoming_nodes));
  Serial.printf("%lu: APP Received nodes from 0%o\n\r", millis(),header.from_node);

  int i = 0;
  while ( i < max_active_nodes && incoming_nodes[i] > 00 )
    add_node(incoming_nodes[i++]);
}

/**
 * Add a particular node to the current list of active nodes
 */
void add_node(uint16_t node){
  
  short i = num_active_nodes;                                    // Do we already know about this node?
  while (i--)  {
    if ( active_nodes[i] == node )
        break;
  }
  
  if ( i == -1 && num_active_nodes < max_active_nodes ){         // If not, add it to the table
      active_nodes[num_active_nodes++] = node; 
      Serial.printf("%lu: APP Added 0%o to list of active nodes.\n\r", millis(),node);
  }
}