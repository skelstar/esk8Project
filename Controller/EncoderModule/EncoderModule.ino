#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <Rotary.h>
#include <myPushButton.h>

#define I2C_SLAVE_ADDRESS 0x4 

/* ---------------------------------------------
	pinouts
----------------------------------------------*/

#define ENCODER_BUTTON_PIN	8	
#define ENCODER_PIN_A		5	
#define ENCODER_PIN_B		6	


#define DEADMAN_SW_PIN 		3
#define LED_PIN 			4

#define INBUILT_LED 		13
 

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, LED_PIN, NEO_GRB + NEO_KHZ800);

Rotary rotary = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);

//--------------------------------------------------------------

#define CMD_ENCODER_MODULE_LED	1
#define CMD_ENCODER_LIMITS_SET	2
#define CMD_LED_BRIGHTNESS		3

int ledBrightness = 30;
#define ENCODER_MODULE_LED_COLOUR_BLACK	0
#define ENCODER_MODULE_LED_COLOUR_RED	1
#define ENCODER_MODULE_LED_COLOUR_BLUE	2
#define ENCODER_MODULE_LED_COLOUR_GREEN	3

#define TWI_RX_BUFFER_SIZE	3

uint8_t tx_data[TWI_RX_BUFFER_SIZE];

// lower number = more coarse
// 0 for safety
#define ENCODER_COUNTER_MIN		0 	// decceleration (ie -20 divides 0-127 into 20)
#define ENCODER_COUNTER_MAX	 	0 	// acceleration (ie 15 divides 127-255 into 15)

volatile int encoderCounterMin = ENCODER_COUNTER_MIN;
volatile int encoderCounterMax = ENCODER_COUNTER_MAX;

volatile int encoderCounter = 0;
volatile byte deadmanSwitch = 0;
//--------------------------------------------------------------

void encoderService() {

	unsigned char result = rotary.process();

	bool canAccelerate = deadmanSwitch == 0;

	if (deadmanSwitch == 1 && encoderCounter > 0) {
		encoderCounter = 0;
		// Serial.print("encoderCounter: "); // Serial.println(encoderCounter);
	}
	
	bool isBraking = encoderCounter < 0;

	if (result == DIR_CW) {
		if (encoderCounter < encoderCounterMax) {
			if (canAccelerate || isBraking) {
				encoderCounter++;
				// Serial.print("encoderCounter: "); // Serial.println(encoderCounter);
			}
			else {
				// Serial.print("Braking: "); // Serial.print(isBraking); // Serial.print(" CanAccel: "); // Serial.println(canAccelerate); 
			}
		}
	}
	else if (result == DIR_CCW) {
		if (encoderCounter > encoderCounterMin) {
			encoderCounter--;
			// Serial.print("encoderCounter: "); // Serial.println(encoderCounter);
		}
	}
}
//--------------------------------------------------------------
void encoderButtonCallback(int eventCode, int eventPin, int eventParam);

#define PULLUP		true
#define OFFSTATE	LOW

myPushButton encoderButton(ENCODER_BUTTON_PIN, PULLUP, LOW, encoderButtonCallback);

void encoderButtonCallback(int eventCode, int eventPin, int eventParam) {
    
	switch (eventCode) {
		case encoderButton.EV_BUTTON_PRESSED:
			encoderCounter = 0;
			// Serial.print("encoderCounter: "); // Serial.println(encoderCounter);
			break;
		// case button.EV_RELEASED:
		// 	break;
		// case button.EV_DOUBLETAP:
		// 	break;
		// case button.EV_HELD_SECONDS:
		// 	break;
    }
}
//--------------------------------------------------------------
void setEncoderLimits(byte min, byte max) {
	// a byte can't be negative
	if (min > max && min > 200 && max < 100) {
		// 200 = -55 (the minimum min)
		encoderCounterMin = 0 - (255 - min);
		encoderCounterMax = max;
	}
	else {
		setPixelColour(ENCODER_MODULE_LED_COLOUR_RED);
		pixels.show();
		delay(200);
		setPixelColour(ENCODER_MODULE_LED_COLOUR_BLACK);
		pixels.show();
	}
}
//--------------------------------------------------------------
/**
 * Don't try and send more than once
 */
void requestEvent()
{  
	Wire.write(encoderCounter);
	// Serial.print("encoderCounter: "); // Serial.println(encoderCounter);
}
//--------------------------------------------------------------
void receiveEvent(int numBytes)
{
    int command = Wire.read();

    if (command ==  CMD_ENCODER_MODULE_LED) {
		int colour = Wire.read();
		setPixelColour(colour);
	}
	else if (command == CMD_ENCODER_LIMITS_SET) {
		byte min = Wire.read();
		byte max = Wire.read();
		setEncoderLimits(min, max);
		// Serial.print("Setting limits: "); // Serial.print(min); // Serial.print("|"); // Serial.println(max); 
	}
	else if (command == CMD_LED_BRIGHTNESS) {
		int brightness = Wire.read();
		if (brightness > 0 && brightness < 255) {
			ledBrightness = brightness;
		}
		else {
			setPixelColour(ENCODER_MODULE_LED_COLOUR_RED);
			pixels.show();
		}
    }
}
//--------------------------------------------------------------

void setup()
{
	// Serial.begin(9600);
	// Serial.println("Ready");

	ledBrightness = 50;

    pixels.begin();
	pixels.setBrightness(ledBrightness); // 1/3 brightness
	setPixelColour(ENCODER_MODULE_LED_COLOUR_GREEN);
	pixels.show();
	delay(200);
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
	
	encoderButton.serviceEvents();

    encoderService();
	
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
			pixels.setPixelColor(0, pixels.Color(0, 0, ledBrightness));
			break;
		case ENCODER_MODULE_LED_COLOUR_GREEN:
			pixels.setPixelColor(0, pixels.Color(0, ledBrightness, 0));
			break;
	}
	pixels.show();
}
//--------------------------------------------------------------
void setupHardware() {

	pinMode(ENCODER_PIN_A, INPUT_PULLUP);
	pinMode(ENCODER_PIN_B, INPUT_PULLUP);

    pinMode(DEADMAN_SW_PIN, INPUT_PULLUP);
    // pinMode(ENCODER_BUTTON_PIN, INPUT_PULLUP);
    pinMode(INBUILT_LED, OUTPUT);
}
//--------------------------------------------------------------