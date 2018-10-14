/**
 * Example sketch for writing to and reading from a slave in transactional manner
 *
 * NOTE: You must not use delay() or I2C communications will fail, use tws_delay() instead (or preferably some smarter timing system)
 *
 * On write the first byte received is considered the register addres to modify/read
 * On each byte sent or read the register address is incremented (and it will loop back to 0)
 *
 * You can try this with the Arduino I2C REPL sketch at https://github.com/rambo/I2C/blob/master/examples/i2crepl/i2crepl.ino 
 * If you have bus-pirate remember that the older revisions do not like the slave streching the clock, this leads to all sorts of weird behaviour
 *
 * To read third value (register number 2 since counting starts at 0) send "[ 8 2 [ 9 r ]", value read should be 0xBE
 * If you then send "[ 9 r r r ]" you should get 0xEF 0xDE 0xAD as response (demonstrating the register counter looping back to zero)
 *
 * You need to have at least 8MHz clock on the ATTiny for this to work (and in fact I have so far tested it only on ATTiny85 @8MHz using internal oscillator)
 * Remember to "Burn bootloader" to make sure your chip is in correct mode 
 */


/**
 * Pin notes by Suovula, see also http://hlt.media.mit.edu/?p=1229
 *
 * DIP and SOIC have same pinout, however the SOIC chips are much cheaper, especially if you buy more than 5 at a time
 * For nice breakout boards see https://github.com/rambo/attiny_boards
 *
 * Basically the arduino pin numbers map directly to the PORTB bit numbers.
 *
// I2C
arduino pin 0 = not(OC1A) = PORTB <- _BV(0) = SOIC pin 5 (I2C SDA, PWM)
arduino pin 2 =           = PORTB <- _BV(2) = SOIC pin 7 (I2C SCL, Analog 1)
// Timer1 -> PWM
arduino pin 1 =     OC1A  = PORTB <- _BV(1) = SOIC pin 6 (PWM)
arduino pin 3 = not(OC1B) = PORTB <- _BV(3) = SOIC pin 2 (Analog 3)
arduino pin 4 =     OC1B  = PORTB <- _BV(4) = SOIC pin 3 (Analog 2)
 */

#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <Rotary.h>

#define I2C_SLAVE_ADDRESS 0x4 // the 7-bit address (remember to change this when adapting this example)
// Get this from https://github.com/rambo/TinyWire
// The default buffer size, Can't recall the scope of defines right now
#ifndef TWI_RX_BUFFER_SIZE
#define TWI_RX_BUFFER_SIZE ( 4 )
#endif
//--------------------------------------------------------------

#define ENCODER_BUTTON_PIN	8	// 34	// 36 didn't work
#define ENCODER_PIN_A		6	// 16
#define ENCODER_PIN_B		5	// 4

// lower number = more coarse
#define ENCODER_COUNTER_MIN	-20 	// decceleration (ie -20 divides 0-127 into 20)
#define ENCODER_COUNTER_MAX	 15 	// acceleration (ie 15 divides 127-255 into 15)

#define BUTTON_PIN 	3
#define LED_PIN 	4

#define INBUILT_LED 	13
 


Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, LED_PIN, NEO_GRB + NEO_KHZ800);

Rotary rotary = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);

//--------------------------------------------------------------

// byte i2c_regs[] =
// {
//     0x00, 
//     0x00, 
//     0x00, 
//     0x00, 
// };

// Tracks the current register pointer position
//const byte reg_size = sizeof(i2c_regs);

// http://www.gammon.com.au/i2c

uint8_t rx_data[TWI_RX_BUFFER_SIZE];

volatile byte encoderCounter = 0;
//--------------------------------------------------------------

void encoderInterruptHandler() {

	unsigned char result = rotary.process();

	//bool canAccelerate = deadmanSwitch.isPressed();

	// if (result == DIR_CCW && (canAccelerate || encoderCounter < 0)) {
	if (result == DIR_CW) {
		if (encoderCounter < ENCODER_COUNTER_MAX) {
			encoderCounter++;
			//i2c_regs[1] = encoderCounter;	
			Serial.println(encoderCounter);
		}
	}
	else if (result == DIR_CCW) {
		if (encoderCounter > ENCODER_COUNTER_MIN) {
			encoderCounter--;
			//i2c_regs[1] = encoderCounter;		
			Serial.println(encoderCounter);
		}
	}
}


/**
 * This is called for each read request we receive, never put more than one byte of data (with TinyWireS.send) to the 
 * send-buffer when using this callback
 */
void requestEvent()
{  
	Serial.println("requestEvent(): ");

	rx_data[0] = encoderCounter;
	rx_data[1] = digitalRead(BUTTON_PIN);
	rx_data[2] = digitalRead(ENCODER_BUTTON_PIN);

	Wire.write(rx_data, sizeof rx_data);

	Serial.print(encoderCounter); Serial.print("|");
	Serial.print((uint8_t)digitalRead(BUTTON_PIN)); Serial.print("|");
	Serial.print((uint8_t)digitalRead(ENCODER_BUTTON_PIN)); Serial.print("|");

	// for (int i = 0; i < reg_size; i++) {
 //    	Wire.write(i2c_regs[i]);	
	// 	Serial.print(i2c_regs[i]); Serial.print("|");
 //    }
	Serial.println();
}

/**
 * The I2C data received -handler
 *
 * This needs to complete before the next incoming transaction (start, data, restart/stop) on the bus does
 * so be quick, set flags for long running tasks to be called from the mainloop instead of running them directly,
 */


#define ENCODER_MODULE_LED_CMD	1
#define ENCODER_MODULE_LED_COLOUR_BLACK	0
#define ENCODER_MODULE_LED_COLOUR_RED	1
#define ENCODER_MODULE_LED_COLOUR_BLUE	2
#define ENCODER_MODULE_LED_COLOUR_GREEN	3

void receiveEvent(int numBytes)
{
    if (numBytes > TWI_RX_BUFFER_SIZE)
    {
        return;
    }

    int command = Wire.read();
    if (command == ENCODER_MODULE_LED_CMD) {
    	int colour = Wire.read();
    	setPixelColour(colour);
    }
}

//--------------------------------------------------------------

void setup()
{
	Serial.begin(9600);
	Serial.println("Ready");

    pixels.begin();
	pixels.setBrightness(50); // 1/3 brightness
	setPixelColour(ENCODER_MODULE_LED_COLOUR_GREEN);
	pixels.show();

	setupEncoder();

    Wire.begin(I2C_SLAVE_ADDRESS);
    Wire.onReceive(receiveEvent);
    Wire.onRequest(requestEvent);

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(ENCODER_BUTTON_PIN, INPUT_PULLUP);
    pinMode(INBUILT_LED, OUTPUT);
}

void loop()
{
    // TinyWireS_stop_check();

    //i2c_regs[0] = digitalRead(BUTTON_PIN) == 1;
    encoderInterruptHandler();
    //pollEncoderPins();
}

//--------------------------------------------------------------
void setPixelColour(int option) {
	switch (option) {
		case ENCODER_MODULE_LED_COLOUR_BLACK:
			pixels.setPixelColor(0, pixels.Color(0, 0, 0));
			break;
		case ENCODER_MODULE_LED_COLOUR_RED:
		pixels.setPixelColor(0, pixels.Color(255, 0, 0));
			break;
		case ENCODER_MODULE_LED_COLOUR_BLUE:
			pixels.setPixelColor(0, pixels.Color(0, 0, 255));
			break;
		case ENCODER_MODULE_LED_COLOUR_GREEN:
			pixels.setPixelColor(0, pixels.Color(0, 255, 0));
			break;
	}
	pixels.show();
}
//--------------------------------------------------------------
void setupEncoder() {

	pinMode(ENCODER_PIN_A, INPUT_PULLUP);
	pinMode(ENCODER_PIN_B, INPUT_PULLUP);

	// attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), encoderInterruptHandler, CHANGE);
	// attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), encoderInterruptHandler, CHANGE);
}
