
#include <SPI.h>
#include "RF24.h"
#include <esk8Lib.h>

#include <ESP8266VESC.h>
#include "VescUart.h"
#include "datatypes.h"
#include <U8g2lib.h>
#include <myPushButton.h>
#include <debugHelper.h>

#include <TaskScheduler.h>

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

#define 	GET_VESC_DATA_INTERVAL	1000
#define 	CONTROLLER_ONLINE_MS	500

//--------------------------------------------------------------------------------
#define	STARTUP 		1 << 0
#define WARNING 		1 << 1
#define ERROR 			1 << 2
#define DEBUG 			1 << 3
#define CONTROLLER_COMMS 	1 << 4
#define HARDWARE		1 << 5
#define VESC_COMMS		1 << 6

debugHelper debug;

volatile uint32_t otherNode;
volatile long lastControllerPacketTime = 0;
volatile float packetData = 0.1;

//--------------------------------------------------------------
void loadPacketForController(bool gotDataFromVesc) {

	if (gotDataFromVesc) {
		debug.print(VESC_COMMS, "VESC online\n");
	}
	else {
		// dummy data
		esk8.boardPacket.batteryVoltage = packetData;
		packetData += 0.1;

		debug.print(VESC_COMMS, "VESC OFFLINE: mock data: %.1f\n", esk8.boardPacket.batteryVoltage);
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
bool haveControllerData;

#define	TN_ONLINE 	1
#define	ST_ONLINE 	2
#define	TN_OFFLINE  3
#define	ST_OFFLINE  4

uint8_t controllerCommsState;

uint8_t serviceCommsState(uint8_t commsState, bool online) {
	switch (commsState) {
		case TN_ONLINE:
			debug.print(CONTROLLER_COMMS, "-> TN_ONLINE \n");
			return online ? ST_ONLINE : TN_OFFLINE;
		case ST_ONLINE:
			return  online ? ST_ONLINE : TN_OFFLINE;
		case TN_OFFLINE:
			debug.print(CONTROLLER_COMMS, "-> TN_OFFLINE \n");
			return online ? TN_ONLINE : ST_OFFLINE;
		case ST_OFFLINE:
			return online ? TN_ONLINE : ST_OFFLINE;
		default:
			return online ? TN_ONLINE : TN_OFFLINE;
	}
}

//--------------------------------------------------------------------------------

Scheduler runner;

void tSendToVESC_callback();
Task tSendToVESC(300, TASK_FOREVER, &tSendToVESC_callback);
void tSendToVESC_callback() {

	int throttle = 127;

	if (esk8.controllerOnline()) {
		debug.print(VESC_COMMS, "tSendToVESC: throttle=%d \n", esk8.controllerPacket.throttle);
		throttle = esk8.controllerPacket.throttle;
	}
	else {
		debug.print(VESC_COMMS, "tSendToVESC: Controller OFFLINE (127) \n");
	}
	esp8266VESC.setNunchukValues(127, throttle, 0, 0);
}

//--------------------------------------------------------------------------------

// U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);
// #define 	OLED_CONTRAST_HIGH	100		// 256 highest

TaskHandle_t RF24CommsRxTask;

//--------------------------------------------------------------------------------

void setup()
{

    Serial.begin(9600);

	debug.init();

	debug.addOption(DEBUG, "DEBUG");
	debug.addOption(STARTUP, "STARTUP");
	debug.addOption(CONTROLLER_COMMS, "CONTROLLER_COMMS");
	debug.addOption(VESC_COMMS, "VESC_COMMS");
	debug.addOption(ERROR, "ERROR");
	debug.setFilter(DEBUG | STARTUP | CONTROLLER_COMMS | ERROR);

	debug.print(STARTUP, "%s\n", compile_date);
	debug.print(STARTUP, "NOTE: %s\n", boardSetup);

    // Setup serial connection to VESC
    Serial1.begin(9600);

    // initOLED();

    radio.begin();

	esk8.begin(&radio, ROLE_BOARD, radioNumber, &debug);

	runner.startNow();
	runner.addTask(tSendToVESC);
	tSendToVESC.enable();

	xTaskCreatePinnedToCore (
		codeForRF24CommsRxTask,	// function
		"RF24 Comms Rx Task",		// name
		10000,			// stack
		NULL,			// parameter
		1,				// priority
		&RF24CommsRxTask,	// handle
		0);				// port	

	debug.print(STARTUP, "loop() core: %d \n", xPortGetCoreID());
}
//*************************************************************
void loop() {

	bool timeToUpdateController = millis() - intervalStarts > esk8.getSendInterval();

	if (timeToUpdateController) {
		intervalStarts = millis();
		// update controller
		bool success = getVescValues();
		if (success == false) {
			debug.print(VESC_COMMS, "VESC not responding\n");
		}
		//update with data if VESC offline
		loadPacketForController(success);
	}

	runner.execute();
}
//*************************************************************
void codeForRF24CommsRxTask( void *parameter ) {
	debug.print(STARTUP, "codeForReceiverTask() core: %d \n", xPortGetCoreID());

	for (;;) {
		haveControllerData = esk8.checkForPacket();

		bool controllerOnline = esk8.controllerOnline();

		controllerCommsState = (uint8_t)serviceCommsState(controllerCommsState, controllerOnline);

		delay(10);
	}
	vTaskDelete(NULL);
}
//*************************************************************
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
		debug.print(VESC_COMMS, "The VESC values could not be read!\n");
	}
	return vescConnected;
}
//--------------------------------------------------------------------------------
// void initOLED() {

//     // OLED
//     pinMode(OLED_GND, OUTPUT); digitalWrite(OLED_GND, LOW);
//     pinMode(OLED_PWR, OUTPUT); digitalWrite(OLED_PWR, HIGH);

// 	// u8g2.setI2CAddress(0x3C);
// 	u8g2.begin();
// 	u8g2.setContrast(OLED_CONTRAST_HIGH);

// 	u8g2.clearBuffer();
// 	u8g2.setFont(u8g2_font_logisoso26_tf);	// u8g2_font_logisoso46_tf
// 	int width = u8g2.getStrWidth("ready!");
// 	u8g2.drawStr((128/2)-(width/2), (64/2) + (26/2),"ready!");
// 	u8g2.sendBuffer();
// }
// //--------------------------------------------------------------------------------
// void updateOLED(bool controllerOnline) {

// 	int y = 0;

// 	u8g2.clearBuffer();
// 	// throttle
// 	u8g2.setFont(u8g2_font_logisoso26_tf);	// u8g2_font_logisoso46_tf
// 	char buff[5];
// 	itoa(esk8.controllerPacket.throttle, buff, 10);
// 	u8g2.drawStr(0, 26, buff);

// 	// vesc connected
// 	y = 64/2+12;
// 	u8g2.setFont(u8g2_font_courB12_tf);
// 	if (vescConnected) {
// 		u8g2.setCursor(0, y);
// 		u8g2.print("VESC: ");
// 		u8g2.print(esk8.boardPacket.batteryVoltage, 1);
// 		//u8g2.drawStr(0, y, "VESC: Connected");
// 	}
// 	else {
// 		u8g2.drawStr(0, y, "VESC: -");
// 	}

// 	y = 64/2+12+2+12;
// 	if (controllerOnline) {
// 		u8g2.drawStr(0, y, "CTRL: Connected");
// 	}
// 	else {
// 		u8g2.drawStr(0, y, "CTRL: -");
// 	}
// 	u8g2.sendBuffer();
// }
//--------------------------------------------------------------------------------
