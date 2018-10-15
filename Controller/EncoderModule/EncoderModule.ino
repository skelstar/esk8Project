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


#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <Rotary.h>

#define I2C_SLAVE_ADDRESS 0x4 

/* ---------------------------------------------
	pinouts
----------------------------------------------*/

#define ENCODER_BUTTON_PIN	8	
#define ENCODER_PIN_A		5	
#define ENCODER_PIN_B		6	

// lower number = more coarse
#define ENCODER_COUNTER_MIN	-20 	// decceleration (ie -20 divides 0-127 into 20)
#define ENCODER_COUNTER_MAX	 15 	// acceleration (ie 15 divides 127-255 into 15)

#define DEADMAN_SW_PIN 		3
#define LED_PIN 			4

#define INBUILT_LED 		13
 

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, LED_PIN, NEO_GRB + NEO_KHZ800);

#define ENCODER_MODULE_LED_CMD	1
#define ENCODER_MODULE_LED_COLOUR_BLACK	0
#define ENCODER_MODULE_LED_COLOUR_RED	1
#define ENCODER_MODULE_LED_COLOUR_BLUE	2
#define ENCODER_MODULE_LED_COLOUR_GREEN	3

Rotary rotary = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);


#define ENCODER_IDX		0
#define ENCODER_BTN_IDX	1
#define DEADMAN_SWITCH_IDX	2

#define TWI_RX_BUFFER_SIZE	3

uint8_t tx_data[TWI_RX_BUFFER_SIZE];

volatile int encoderCounter = 0;
volatile byte encoderButton = 0;
volatile byte deadmanSwitch = 0;
//--------------------------------------------------------------

void encoderInterruptHandler() {

	unsigned char result = rotary.process();

	bool canAccelerate = deadmanSwitch == 0;
	
	byte minCount = ENCODER_COUNTER_MIN;
	byte maxCount = ENCODER_COUNTER_MAX;
	bool isBraking = encoderCounter < 0;

	if (result == DIR_CW) {
		if (encoderCounter < ENCODER_COUNTER_MAX) {
			if (canAccelerate || isBraking) {
				encoderCounter++;
			}
			else {
				Serial.print("Braking: "); Serial.print(isBraking); Serial.print(" CanAccel: "); Serial.println(canAccelerate); 
			}
		}
	}
	else if (result == DIR_CCW) {
		if (encoderCounter > ENCODER_COUNTER_MIN) {
			encoderCounter--;
			Serial.println(encoderCounter);
		}
	}
}

//--------------------------------------------------------------
/**
 * Don't try and send more than once
 */
void requestEvent()
{  
	Serial.println("requestEvent(): ");

	tx_data[ENCODER_IDX] = encoderCounter;
	tx_data[DEADMAN_SWITCH_IDX] = deadmanSwitch;
	tx_data[ENCODER_BTN_IDX] = encoderButton;

	Wire.write(tx_data, sizeof tx_data);

	Serial.print(encoderCounter); Serial.print("|");
	Serial.print((uint8_t)digitalRead(DEADMAN_SW_PIN)); Serial.print("|");
	Serial.print((uint8_t)digitalRead(ENCODER_BUTTON_PIN)); Serial.print("|");

	// for (int i = 0; i < reg_size; i++) {
 //    	Wire.write(i2c_regs[i]);	
	// 	Serial.print(i2c_regs[i]); Serial.print("|");
 //    }
	Serial.println();
}

//--------------------------------------------------------------

void receiveEvent(int numBytes)
{
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
	setPixelColour(ENCODER_MODULE_LED_COLOUR_BLACK);
	pixels.show();

	setupHardware();

    Wire.begin(I2C_SLAVE_ADDRESS);
    Wire.onReceive(receiveEvent);
    Wire.onRequest(requestEvent);
}

void loop()
{
    deadmanSwitch = digitalRead(DEADMAN_SW_PIN);
	
	encoderButton = digitalRead(ENCODER_BUTTON_PIN);

    encoderInterruptHandler();
	
	delay(10);
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
void setupHardware() {

	pinMode(ENCODER_PIN_A, INPUT_PULLUP);
	pinMode(ENCODER_PIN_B, INPUT_PULLUP);

    pinMode(DEADMAN_SW_PIN, INPUT_PULLUP);
    pinMode(ENCODER_BUTTON_PIN, INPUT_PULLUP);
    pinMode(INBUILT_LED, OUTPUT);
}
//--------------------------------------------------------------