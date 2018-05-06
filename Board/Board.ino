
#include <SPI.h>
#include "RF24.h"
#include <esk8Lib.h>

#include <ESP8266VESC.h>
#include "VescUart.h"
#include "datatypes.h"
#include <U8g2lib.h>
#include <myPushButton.h>
#include <debugHelper.h>

/*--------------------------------------------------------------------------------*/

const char compile_date[] = __DATE__ " " __TIME__;

//--------------------------------------------------------------------------------

#define 	OLED_GND		12
#define 	OLED_PWR		27
#define 	OLED_SCL		25	// std ESP32
#define 	OLED_SDA		32	// std ESP32
#define 	OLED_ADDR		0x3c

bool radioNumber = 0;
// blank DEV board
// #define 	SPI_CE			22	// white - do we need it?
// #define 	SPI_CS			5	// green
// const char boardSetup[] = "DEV Board";

// WEMOS TTGO
#define 	SPI_CE			33	// white - do we need it?
#define 	SPI_CS			26	// green
const char boardSetup[] = "WEMOS TTGO Board";


RF24 radio(SPI_CE, SPI_CS);	// ce pin, cs pin

esk8Lib esk8;

#define 	ROLE_BOARD 		1
#define 	ROLE_CONTROLLER 0

#define GET_VESC_DATA_INTERVAL	1000
#define CONTROLLER_ONLINE_MS	500

//--------------------------------------------------------------------------------

#define	STARTUP 		1 << 0
#define WARNING 		1 << 1
#define ERROR 			1 << 2
#define DEBUG 			1 << 3
#define COMMUNICATION 	1 << 4

debugHelper debug;

volatile uint32_t otherNode;
volatile long lastControllerPacketTime = 0;
volatile float packetData = 0.1;
bool calc_delay =false;

//--------------------------------------------------------------
void loadPacketForController(bool gotDataFromVesc) {

	if (gotDataFromVesc) {
		debug.print(COMMUNICATION, "VESC online\n");
	}
	else {
		// dummy data
		esk8.boardPacket.batteryVoltage = packetData;
		packetData += 0.1;

		debug.print(DEBUG, "Loaded batteryVoltage: %.1f\n", esk8.boardPacket.batteryVoltage);
	}
}
//--------------------------------------------------------------------------------

#define 	VESC_UART_RX	16	// orange - VESC 5
#define 	VESC_UART_TX	17	// green - VESC 6

HardwareSerial Serial1(2);

ESP8266VESC esp8266VESC = ESP8266VESC(Serial1);

bool vescConnected = false;
bool controllerHasBeenOnline = false;
long intervalStarts = 0;

//--------------------------------------------------------------------------------

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);
#define 	OLED_CONTRAST_HIGH	100		// 256 highest

//--------------------------------------------------------------------------------

void setup()
{

    Serial.begin(9600);

	debug.init();

	debug.addOption(DEBUG, "DEBUG");
	debug.addOption(STARTUP, "STARTUP");
	debug.addOption(COMMUNICATION, "COMMUNICATION");
	debug.addOption(ERROR, "ERROR");
	debug.setFilter(DEBUG | STARTUP | COMMUNICATION | ERROR);

	debug.print(STARTUP, "%s\n", compile_date);
	debug.print(STARTUP, "NOTE: %s\n", boardSetup);

    // Setup serial connection to VESC
    Serial1.begin(9600);

    initOLED();

    radio.begin();

	esk8.begin(&radio, ROLE_BOARD, radioNumber, &debug);
}

void loop() {

	if (millis() - intervalStarts > esk8.getSendInterval()) {
		intervalStarts = millis();
		// update controller
		bool success = getVescValues();
		if (success == false) {
			debug.print(COMMUNICATION, "VESC not responding\n");
		}
		loadPacketForController(success);
	}

	bool controllerOnline = esk8.controllerOnline();

	if (controllerHasBeenOnline == false && controllerOnline) {
		controllerHasBeenOnline = true;
	}

	bool haveControllerData = esk8.checkForPacket();
	bool controllerDataChanged = esk8.packetChanged();

	if (haveControllerData && controllerDataChanged) {
		debug.print(COMMUNICATION, "sendDataToVesc(); Throttle: %d \n", esk8.controllerPacket.throttle);
		sendDataToVesc(controllerOnline, controllerHasBeenOnline);
	}

	updateOLED(controllerOnline);
}
//--------------------------------------------------------------------------------
void sendDataToVesc(bool controllerOnline, bool controllerHasBeenOnline) {
	debug.print(COMMUNICATION, "Sending data to VESC \n");
	if (controllerOnline) {
		esp8266VESC.setNunchukValues(127, esk8.controllerPacket.throttle, 0, 0);
	}
	else if (controllerHasBeenOnline) {
		// in case controller has been connected but then drops for some reason
		esp8266VESC.setNunchukValues(127, 127, 0, 0);
	}
	else {
		// don't send (in case something else is controlling the VESC)
	}
}
//--------------------------------------------------------------------------------
bool getVescValues() {

	// struct VESCValues
	// {
	//		float temperatureMosfet1 = 0.0f;
	//		float temperatureMosfet2 = 0.0f;
	//		float temperatureMosfet3 = 0.0f;
	//		float temperatureMosfet4 = 0.0f;
	//		float temperatureMosfet5 = 0.0f;
	//		float temperatureMosfet6 = 0.0f;
	//		float temperaturePCB = 0.0f;
	//		float avgMotorCurrent = 0.0f;
	//		float avgInputCurrent = 0.0f;
	//		float dutyCycleNow = 0.0f;
	//		int32_t rpm = 0;
	//		float inputVoltage = 0.0f;
	//		float ampHours = 0.0f;
	//		float ampHoursCharged = 0.0f;
	//		float wattHours = 0.0f;
	//		float wattHoursCharged = 0.0f;
	//		int32_t tachometer = 0;
	//		int32_t tachometerAbs = 0;
	//		mc_fault_code faultCode = FAULT_CODE_NONE;
	// };


    VESCValues vescValues;

	if (esp8266VESC.getVESCValues(vescValues) == true) {
		vescConnected = true;
		esk8.boardPacket.batteryVoltage = vescValues.inputVoltage;
		// Serial.println("Average motor current = " + String(vescValues.avgMotorCurrent) + "A");
		// Serial.println("Average battery current = " + String(vescValues.avgInputCurrent) + "A");
		// Serial.println("Duty cycle = " + String(vescValues.dutyCycleNow) + "%");
	    
		//Serial.println("rpm = " + String(vescValues.rpm) + "rpm");

		Serial.println("Battery voltage = " + String(vescValues.inputVoltage) + "V");

		// Serial.println("Drawn energy (mAh) = " + String(vescValues.ampHours) + "mAh");
		// Serial.println("Charged energy (mAh) = " + String(vescValues.ampHoursCharged) + "mAh");

		// Serial.println("Drawn energy (Wh) = " + String(vescValues.wattHours) + "Wh");
		// Serial.println("Charged energy (Wh) = " + String(vescValues.wattHoursCharged) + "Wh");

		// String(vescValues.ampHours).toCharArray(myVescValues.ampHours, 7);
		// String(vescValues.rpm).toCharArray(myVescValues.rpm, 7);
			// String(vescValues.inputVoltage).toCharArray(myVescValues.battery, 7);
		// myVescValues.battery = vescValues.inputVoltage;
		// String(vescValues.avgMotorCurrent).toCharArray(myVescValues.avgMotorCurrent, 7);
		// String(vescValues.dutyCycleNow).toCharArray(myVescValues.dutyCycleNow, 7);
	}
	else
	{
		vescConnected = false;
		debug.print(COMMUNICATION, "The VESC values could not be read!\n");
	}
	return vescConnected;
}
//--------------------------------------------------------------------------------
void initOLED() {

    // OLED
    pinMode(OLED_GND, OUTPUT); digitalWrite(OLED_GND, LOW);
    pinMode(OLED_PWR, OUTPUT); digitalWrite(OLED_PWR, HIGH);

	// u8g2.setI2CAddress(0x3C);
	u8g2.begin();
	u8g2.setContrast(OLED_CONTRAST_HIGH);

	u8g2.clearBuffer();
	u8g2.setFont(u8g2_font_logisoso26_tf);	// u8g2_font_logisoso46_tf
	int width = u8g2.getStrWidth("ready!");
	u8g2.drawStr((128/2)-(width/2), (64/2) + (26/2),"ready!");
	u8g2.sendBuffer();
}
//--------------------------------------------------------------------------------
void updateOLED(bool controllerOnline) {

	int y = 0;

	u8g2.clearBuffer();
	// throttle
	u8g2.setFont(u8g2_font_logisoso26_tf);	// u8g2_font_logisoso46_tf
	char buff[5];
	itoa(esk8.controllerPacket.throttle, buff, 10);
	u8g2.drawStr(0, 26, buff);

	// vesc connected
	y = 64/2+12;
	u8g2.setFont(u8g2_font_courB12_tf);
	if (vescConnected) {
		u8g2.setCursor(0, y);
		u8g2.print("VESC: ");
		u8g2.print(esk8.boardPacket.batteryVoltage, 1);
		//u8g2.drawStr(0, y, "VESC: Connected");
	}
	else {
		u8g2.drawStr(0, y, "VESC: -");
	}

	y = 64/2+12+2+12;
	if (controllerOnline) {
		u8g2.drawStr(0, y, "CTRL: Connected");
	}
	else {
		u8g2.drawStr(0, y, "CTRL: -");
	}
	u8g2.sendBuffer();
}
//--------------------------------------------------------------------------------
