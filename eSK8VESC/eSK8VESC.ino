
#include <SPI.h>
#include "RF24.h"
#include <esk8Lib.h>

#include <ESP8266VESC.h>
#include <myWifiHelper.h>
#include "VescUart.h"
#include "datatypes.h"
#include <U8g2lib.h>
// #include <TaskScheduler.h>
#include <myPushButton.h>



/*--------------------------------------------------------------------------------

REMEBER: hold GPIO0 to put into OTA mode

--------------------------------------------------------------------------------*/

const char compile_date[] = __DATE__ " " __TIME__;

//--------------------------------------------------------------------------------

// #define		WIFI_HOSTNAME		"/esk8/base"

#define 	OLED_GND		12
#define 	OLED_PWR		27
#define 	OLED_SCL		25	// std ESP32
#define 	OLED_SDA		32	// std ESP32
#define 	OLED_ADDR		0x3c

//--------------------------------------------------------------------------------

#define WIFI_HOSTNAME "esk8VESC"
#define WIFI_OTA_NAME "esk8VESC-OTA"

MyWifiHelper wifiHelper(WIFI_HOSTNAME);

//--------------------------------------------------------------------------------

#define MODE_NORMAL		1
#define MODE_WAIT_FOR_OTA		2

volatile int mode;

#define 	PULLUP	true
#define 	OFF_STATE_HIGH	1
#define 	BUTTON 		0

void gpio0_callback( int eventCode, int eventPin, int eventParam );
myPushButton gpio0(BUTTON, PULLUP, OFF_STATE_HIGH, gpio0_callback);

void gpio0_callback( int eventCode, int eventPin, int eventParam ) {

	char payload1[20];
	char numSecsBuff[3];

    switch (eventCode) {
		// case gpio.EV_BUTTON_PRESSED:
		//     break;
		// case gpio.EV_RELEASED:
		//     break;
		case gpio0.EV_HELD_SECONDS:

			if (mode == MODE_WAIT_FOR_OTA) {
				return;
			}

			switch (mode) {
				case MODE_NORMAL:
					mode = MODE_WAIT_FOR_OTA;
					break;
				case MODE_WAIT_FOR_OTA:
					mode = MODE_NORMAL;
					break;
			}
            break;
        default:    
            break;
    }
}
//--------------------------------------------------------------------------------
//SoftwareSerial softwareSerial = SoftwareSerial(VESC_UART_RX, VESC_UART_TX); // ESP8266 (NodeMCU): RX (D5), TX (D6 / GPIO12)
// Serial1(2)
#define 	VESC_UART_RX	16	// orange - VESC 5
#define 	VESC_UART_TX	17	// green - VESC 6
HardwareSerial Serial1(2);

ESP8266VESC esp8266VESC = ESP8266VESC(Serial1);

bool vescConnected = false;
bool sendPacketOk = false;
bool slaveHasBeenOnline = false;

// MyWifiHelper wifiHelper(WIFI_HOSTNAME);

//--------------------------------------------------------------------------------
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);
#define 	OLED_CONTRAST_HIGH	100		// 256 highest
//--------------------------------------------------------------------------------

// #define 	SPI_MOSI		23	// blue
// #define 	SPI_MISO		19 	// orange
// #define 	SPI_CLK			18 	// yellow

/* Hardware configuration: Set up nRF24L01 radio on SPI bus plus pins 7 & 8 */
bool radioNumber = 0;
#define 	SPI_CE			33 //22//25//4 	// white - do we need it?
#define 	SPI_CS			26 //5//32//5	// green
RF24 radio(SPI_CE, SPI_CS);	// ce pin, cs pin

esk8Lib esk8;

#define 	ROLE_MASTER 1
#define 	ROLE_SLAVE 	0

//--------------------------------------------------------------------------------

void setup()
{
    // Debug output
    Serial.begin(9600);

    Serial.println(compile_date);

    // Setup serial connection to VESC
    Serial1.begin(9600);

    initOLED();

    radio.begin();

	esk8.begin(&radio, ROLE_MASTER, radioNumber);

    mode = MODE_NORMAL;
}
//--------------------------------------------------------------------------------

long periodStarts = 0;
#define METADATA_UPDATE_PERIOD	3000
#define LOOP_DELAY				5

void loop() {

	if (mode == MODE_WAIT_FOR_OTA) {
		waitForOta();
	}

	int32_t vescRpm = -1;
	if (millis() - periodStarts > METADATA_UPDATE_PERIOD) {
		periodStarts = millis();
		// update slave
		bool success = getVescValues();
	}

	if (slaveHasBeenOnline == false) {

	}

	esk8.updateMasterPacket(vescRpm);

	long started = millis();

	sendPacketOk = esk8.sendPacketToSlave() == true;

	if (sendPacketOk && slaveHasBeenOnline == false) {
		slaveHasBeenOnline = true;
	}

	if (sendPacketOk) {
		esp8266VESC.setNunchukValues(127, esk8.slavePacket.throttle, 0, 0);
	}
	else if (slaveHasBeenOnline) {
		// in case slave has been connected but then drops for some reason
		esp8266VESC.setNunchukValues(127, 127, 0, 0);
	}
	else {
		// don't send
	}

	updateOLED();

	//Serial.printf("Round-trip time: %ulms \n", millis() - started);

	gpio0.serviceEvents();
    
    delay(LOOP_DELAY);
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
		esk8.masterPacket.batteryVoltage = vescValues.inputVoltage;
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
		Serial.println("The VESC values could not be read!");
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
void updateOLED() {

	int y = 0;

	switch (mode) {
		case MODE_NORMAL:
			u8g2.clearBuffer();
			// throttle
			u8g2.setFont(u8g2_font_logisoso26_tf);	// u8g2_font_logisoso46_tf
			char buff[5];
			itoa(esk8.slavePacket.throttle, buff, 10);
			u8g2.drawStr(0, 26, buff);

			// vesc connected
			y = 64/2+12;
			u8g2.setFont(u8g2_font_courB12_tf);
			if (vescConnected) {
				u8g2.setCursor(0, y);
				u8g2.print("VESC: ");
				u8g2.print(esk8.masterPacket.batteryVoltage, 1);
				//u8g2.drawStr(0, y, "VESC: Connected");
			}
			else {
				u8g2.drawStr(0, y, "VESC: -");
			}

			y = 64/2+12+2+12;
			if (sendPacketOk) {
				u8g2.drawStr(0, y, "CTRL: Connected");
			}
			else {
				u8g2.drawStr(0, y, "CTRL: -");
			}
			u8g2.sendBuffer();
			break;
		case MODE_WAIT_FOR_OTA:
			u8g2.clearBuffer();
			u8g2.setFont(u8g2_font_logisoso26_tf);	// u8g2_font_logisoso46_tf
			u8g2.drawStr(0, (64/2) + (26/2), "OTA");
			u8g2.sendBuffer();
			break;
	}
}
//--------------------------------------------------------------------------------
void waitForOta() {

	updateOLED();

	Serial.println("setupWifi()");
	wifiHelper.setupWifi();
	Serial.println("setupOTA(WIFI_OTA_NAME)");
	wifiHelper.setupOTA(WIFI_OTA_NAME);
	Serial.printf("Waiting: ");

	while (mode == MODE_WAIT_FOR_OTA) {
	    ArduinoOTA.handle();
		Serial.printf(".");
		delay(100);
	}
}
//--------------------------------------------------------------------------------

// void drawBattery(double volts, int x, int y, int w, int h, int b) {
// // x starts from left of nipple.
// 	#define 	n 	b

// 	int battPercent = map(volts*10, 33*10, 42*10, 1*10, 100*10)/10;

// 	// outside box
// 	u8g2.setDrawColor(1);
// 	u8g2.drawBox(x+n, y, w-n, h);
// 	// inside box
// 	//if (h > 20) {
// 		u8g2.setDrawColor(0);
// 		u8g2.drawBox(x+n + b, y+b, w-(b*2)-n, h-(b*2));
// 	//}

// 	u8g2.setDrawColor(1);
// 	// capacity
// 	double barWidth = w-(4*b)-n;
// 	int cap = ((battPercent/100.0) * barWidth);
// 	int notcap = barWidth - cap;
// 	//u8g2.drawBox(x+n+w-(2*b), y+(b*2), cap*(-1), h-(b*4));
// 	u8g2.drawBox(x+n+(b*2) + notcap, y+(b*2), cap, h-(b*4));
// 	// nipple
// 	u8g2.drawBox(x, y + n*2, n, h-(n*4));
	
// 	// u8g2.setFont(u8g2_font_courB18_tf);	// u8g2_font_logisoso46_tf
// 	// char val[100];
// 	// u8g2.setDrawColor(2);
// 	// itoa(battPercent, val, 10);
// 	// int sw = u8g2.getStrWidth(val);
// 	// u8g2.drawStr((128/2)-(sw/2), (64/2) + (18/2), val);

// 	//u8g2.sendBuffer();

// 	u8g2.setDrawColor(1);
// }
// //--------------------------------------------------------------------------------
// void oledBigBattery(double volts, bool update) {
// 	#define w  	100
// 	#define h 	50
// 	#define border	6
// 	drawBattery(volts, (128-w)/2, (64-h)/2, w, h, border);

// 	if (update) {
// 		u8g2.sendBuffer();
// 	}
// }
//--------------------------------------------------------------------------------
// void initRadio(int radioNum) {

//     #define NUM_RETRIES	3

//     radio.begin();
//     radio.setPALevel(RF24_PA_HIGH);
// 	// radio.enableAckPayload();
// 	// radio.enableDynamicPayloads();
// 	radio.setRetries(0, NUM_RETRIES);
// 	radio.printDetails();

//     if (radioNum == RADIO_CONTROLLER) {
//           radio.openWritingPipe(addresses[1]);        // Both radios listen on the same pipes by default, but opposite addresses
//           radio.openReadingPipe(1, addresses[0]);
//     }
//     else if (radioNum == RADIO_BASE) {
// 		radio.openWritingPipe(addresses[0]);        // Both radios listen on the same pipes by default, but opposite addresses
//         radio.openReadingPipe(1, addresses[1]);    
//     }
//     else {
//     	Serial.println("ERROR: INVALID Radio Number!");
//     }
//     radio.startListening();
//     Serial.println("listening");
// }