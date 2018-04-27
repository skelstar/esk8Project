
#include <esk8Lib.h>

#include <ESP8266VESC.h>
// #include <myWifiHelper.h>
#include "VescUart.h"
#include "datatypes.h"
#include <U8g2lib.h>
#include <myPushButton.h>

#include <painlessMesh.h>


/*--------------------------------------------------------------------------------

REMEMBER: hold GPIO0 to put into OTA mode

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

// #define WIFI_HOSTNAME "esk8VESC"
// #define WIFI_OTA_NAME "esk8VESC-OTA"

// MyWifiHelper wifiHelper(WIFI_HOSTNAME);


#define   MESH_SSID       "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

painlessMesh  mesh;

Scheduler 	userScheduler; // to control your personal task

void sendMessage(); 
Task taskSendMessage( TASK_MILLISECOND * 1, TASK_FOREVER, &sendMessage ); // start with a one second interval

volatile uint32_t otherNode;
volatile long lastSlavePacketTime = 0;
volatile float packetData = 0.1;
bool calc_delay =false;
#define GET_VESC_DATA_INTERVAL	1000

//--------------------------------------------------------------
void sendMessage() {

	bool success = getVescValues();

	if (success) {
		Serial.printf("VESC online\n");

	}
	else {
		String msg = "VESC offline ";
		msg +=  packetData;
		mesh.sendBroadcast(msg);
		Serial.printf("Sending message: %s\n", msg.c_str());
	}

	packetData += 0.1;

	if (calc_delay) {
		mesh.startDelayMeas(otherNode);
	}


	taskSendMessage.setInterval( GET_VESC_DATA_INTERVAL );
}
//--------------------------------------------------------------
void receivedCallback(uint32_t from, String & msg) {
	otherNode = from;
	Serial.printf("startHere: Received from %u msg=%s since: %ums\n", otherNode, msg.c_str(), millis()-lastSlavePacketTime);
	lastSlavePacketTime = millis();
}
//--------------------------------------------------------------
void delayReceivedCallback(uint32_t from, int32_t delay) {
	Serial.printf("Delay to node %u is %d us\n", from, delay);
}

//--------------------------------------------------------------------------------
//SoftwareSerial softwareSerial = SoftwareSerial(VESC_UART_RX, VESC_UART_TX); // ESP8266 (NodeMCU): RX (D5), TX (D6 / GPIO12)
// Serial1(2)
#define 	VESC_UART_RX	16	// orange - VESC 5
#define 	VESC_UART_TX	17	// green - VESC 6
HardwareSerial Serial1(2);

ESP8266VESC esp8266VESC = ESP8266VESC(Serial1);

bool vescConnected = false;
bool slaveHasBeenOnline = false;

//--------------------------------------------------------------------------------
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);
#define 	OLED_CONTRAST_HIGH	100		// 256 highest
//--------------------------------------------------------------------------------

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

	//mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
	//mesh.setDebugMsgTypes(ERROR | DEBUG | CONNECTION | COMMUNICATION);  // set before init() so that you can see startup messages
	mesh.setDebugMsgTypes( ERROR | DEBUG | CONNECTION );  // set before init() so that you can see startup messages

	mesh.init(MESH_SSID, MESH_PASSWORD, &userScheduler, MESH_PORT);
	mesh.onReceive(&receivedCallback);
	mesh.onNodeDelayReceived(&delayReceivedCallback);

	userScheduler.addTask( taskSendMessage );
	taskSendMessage.enable();

    initOLED();

	esk8.begin(ROLE_MASTER);
}
//--------------------------------------------------------------------------------

long periodStarts = 0;
#define METADATA_UPDATE_PERIOD	3000
#define LOOP_DELAY				5
#define SLAVE_TIMEOUT_PERIOD_MS	300

void loop() {

	userScheduler.execute(); // it will run mesh scheduler as well
	mesh.update();

	// if (millis() - periodStarts > METADATA_UPDATE_PERIOD) {
	// 	periodStarts = millis();
	// 	// update slave
	// 	bool success = getVescValues();
	// }

	// esk8.updateMasterPacket(vescRpm);

	bool slaveOnline = slaveIsOnline();	//esk8.sendPacketToSlave() == true;

	if (slaveHasBeenOnline == false && slaveOnline) {
		slaveHasBeenOnline = true;
	}

	sendToVESC(slaveOnline, slaveHasBeenOnline);

	updateOLED(slaveOnline);
    
    // delay(LOOP_DELAY);
}
//--------------------------------------------------------------------------------
bool slaveIsOnline() {
	return millis() - lastSlavePacketTime < SLAVE_TIMEOUT_PERIOD_MS;
}
//--------------------------------------------------------------------------------
void sendToVESC(bool slaveOnline, bool slaveHasBeenOnline) {
	if (slaveOnline) {
		esp8266VESC.setNunchukValues(127, esk8.slavePacket.throttle, 0, 0);
	}
	else if (slaveHasBeenOnline) {
		// in case slave has been connected but then drops for some reason
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
void updateOLED(bool slaveOnline) {

	int y = 0;

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
	if (slaveOnline) {
		u8g2.drawStr(0, y, "CTRL: Connected");
	}
	else {
		u8g2.drawStr(0, y, "CTRL: -");
	}
	u8g2.sendBuffer();
}
//--------------------------------------------------------------------------------
