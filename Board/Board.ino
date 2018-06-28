
#include <SPI.h>
#include "RF24.h"
#include <esk8Lib.h>
#include <Adafruit_NeoPixel.h>
#include <U8g2lib.h>

#include <ESP8266VESC.h>
#include "VescUart.h"
#include "datatypes.h"
#include <U8g2lib.h>
#include <myPushButton.h>
#include <debugHelper.h>
#include <rom/rtc.h>
#include <esp_int_wdt.h>
#include <esp_task_wdt.h>
#include <TaskScheduler.h>

/*--------------------------------------------------------------------------------*/

const char compile_date[] = __DATE__ " " __TIME__;

//--------------------------------------------------------------

// struct Status_Type {
// 	uint8_t status;
// 	bool change;
// };

// Status_Type controllerStatus;
// Status_Type vescStatus;

bool controllerStatusChanged = true;
bool controllerOnline = false;
bool vescStatusChanged = true;
int currentThrottle = 127;
int currentEncoderButton = 0;

//--------------------------------------------------------------------------------

// #define LED_PIN     4
// #define NUM_LEDS    17
// #define BRIGHTNESS  64

// Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRBW + NEO_KHZ800);

//--------------------------------------------------------------------------------

#define 	OLED_GND		12
#define 	OLED_PWR		27
#define 	OLED_SCL		22//25	// std ESP32
#define 	OLED_SDA		21//32	// std ESP32
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
#define	STARTUP 			1 << 0
#define WARNING 			1 << 1
#define ERROR 				1 << 2
#define DEBUG 				1 << 3
#define CONTROLLER_COMMS 	1 << 4
#define HARDWARE			1 << 5
#define VESC_COMMS			1 << 6
#define ONLINE_STATUS		1 << 7
#define STATE 				1 << 8

debugHelper debug;

volatile uint32_t otherNode;
volatile long lastControllerPacketTime = 0;

//--------------------------------------------------------------

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);
 #define 	OLED_CONTRAST_HIGH	100		// 256 highest

//--------------------------------------------------------------
void loadPacketForController(bool gotDataFromVesc) {

	if (gotDataFromVesc) {
		debug.print(VESC_COMMS, "VESC online\n");
	}
	else {
		// dummy data
		esk8.boardPacket.batteryVoltage = 0;
		debug.print(VESC_COMMS, "VESC OFFLINE \n");
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
long lastControllerOnlineTime = 0;

//--------------------------------------------------------------
#define	TN_ONLINE 	1
#define	ST_ONLINE 	2
#define	TN_OFFLINE  3
#define	ST_OFFLINE  4

class OnlineStatus 
{
	private:
		uint8_t state = ST_OFFLINE;
		uint8_t oldstate = ST_OFFLINE;

	public:

		bool serviceState(bool online) {
			switch (state) {
				case TN_ONLINE:
					debug.print(ONLINE_STATUS, "-> TN_ONLINE \n");
					state = online ? ST_ONLINE : TN_OFFLINE;
					break;
				case ST_ONLINE:
					lastControllerOnlineTime = millis();
					state = online ? ST_ONLINE : TN_OFFLINE;
					break;
				case TN_OFFLINE:
					debug.print(ONLINE_STATUS, "-> TN_OFFLINE \n");
					state = online ? TN_ONLINE : ST_OFFLINE;
					break;
				case ST_OFFLINE:
					state = online ? TN_ONLINE : ST_OFFLINE;
					break;
				default:
					state = online ? TN_ONLINE : TN_OFFLINE;
					break;
			}
			bool stateChanged = oldstate != state;
			oldstate = state;
			return stateChanged;
		}
};

OnlineStatus controllerStatus;
OnlineStatus vescStatus;

//--------------------------------------------------------------

uint8_t controllerCommsState;
uint8_t vescCommsState;
long lastControllerOfflineTime = 0;
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

void tUpdateOLED_callback();
Task tUpdateOLED(1023, TASK_FOREVER, &tUpdateOLED_callback);
void tUpdateOLED_callback() {
	debug.print(DEBUG, "tUpdateOLED(); \n");
	showOnlineOfflineOLED(esk8.controllerOnline(), esk8.boardPacket.batteryVoltage != 0);
}

//--------------------------------------------------------------------------------

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
	debug.addOption(ONLINE_STATUS, "ONLINE_STATUS");
	debug.addOption(STATE, "STATE");

	debug.setFilter( STARTUP );

	debug.print(STARTUP, "%s\n", compile_date);
	debug.print(STARTUP, "NOTE: %s\n", boardSetup);

	print_reset_reason(rtc_get_reset_reason(0), 0);
	print_reset_reason(rtc_get_reset_reason(1), 1);

    // Setup serial connection to VESC
    Serial1.begin(9600);

    initOLED();

  //   strip.setBrightness(BRIGHTNESS);
  //   strip.begin();
  //   delay(50);
  //   strip.show(); // Initialize all pixels to 'off'
  //   for (int i=0; i<NUM_LEDS; i++) {
		// strip.setPixelColor(i, strip.Color(0, 0, 0));
  //   }
  //   strip.show();

    radio.begin();

	esk8.begin(&radio, ROLE_BOARD, radioNumber, &debug);

	runner.startNow();
	runner.addTask(tSendToVESC);
	tSendToVESC.enable();
	// tUpdateOLED.enable();

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

long updateOLEDtime = 0;
#define UPDATE_OLED_PERIOD	1050

void loop() {

	bool timeToUpdateController = millis() - intervalStarts > esk8.getSendInterval();
	bool timeToUpdateOLED = millis() - updateOLEDtime > UPDATE_OLED_PERIOD;

	if (timeToUpdateController) {
		intervalStarts = millis();
		// update controller
		bool success = getVescValues();

		bool vescStatusChanged = vescStatus.serviceState(success);

		//update with data if VESC offline
		loadPacketForController(success);
	}

	if (timeToUpdateOLED) {
		if (!esk8.controllerOnline() || esk8.boardPacket.batteryVoltage == 0) {
			showOnlineOfflineOLED(esk8.controllerOnline(), esk8.boardPacket.batteryVoltage != 0);
		}
		else {
			showBatteryVoltage();
		}
	}

	runner.execute();

	// updateLEDs();

	delay(10);
}
//*************************************************************
long nowms = 0;

void codeForRF24CommsRxTask( void *parameter ) {
	debug.print(STARTUP, "codeForReceiverTask() core: %d \n", xPortGetCoreID());

	for (;;) {

		// if (millis()-nowms > 1000) {
		// 	long oldnow = nowms;
		// 	nowms = millis();
		// 	debug.print(DEBUG, "%u %d \n", millis()/1000, millis()-oldnow);
		// }

		haveControllerData = esk8.checkForPacket();

		bool controllerOnline = esk8.controllerOnline();

		controllerStatusChanged = controllerStatus.serviceState(controllerOnline);

		esp_task_wdt_feed();

		delay(50);
	}
	vTaskDelete(NULL);
}
//*************************************************************
//--------------------------------------------------------------------------------
void initOLED() {

    // OLED
    // pinMode(OLED_GND, OUTPUT); digitalWrite(OLED_GND, LOW);
    // pinMode(OLED_PWR, OUTPUT); digitalWrite(OLED_PWR, HIGH);

	u8g2.begin();
	u8g2.setContrast(OLED_CONTRAST_HIGH);

	u8g2.clearBuffer();
	u8g2.setFont(u8g2_font_logisoso46_tf);	// u8g2_font_inr38_mr u8g2_font_logisoso46_tf u8g2_font_logisoso26_tf

	int width = u8g2.getStrWidth("GO!");
	int height = 46;

	u8g2.drawStr((128/2)-(width/2), (64/2) + (height/2), "GO!");
	u8g2.sendBuffer();
}
//--------------------------------------------------------------
void showOnlineOfflineOLED(bool controllerOnline, bool vescOnline) {
	u8g2.clearBuffer();
	u8g2.setFont(u8g2_font_logisoso46_tf);	// u8g2_font_inr38_mr u8g2_font_logisoso46_tf u8g2_font_logisoso26_tf

	int width = u8g2.getStrWidth("* *");
	int height = 46;

	if (controllerOnline && vescOnline) {
		u8g2.drawStr((128/2)-(width/2), (64/2) + (height/2), "* *");
	}
	else if (controllerOnline && !vescOnline) {
		u8g2.drawStr((128/2)-(width/2), (64/2) + (height/2), "* -");
	}
	else if (!controllerOnline && vescOnline) {
		u8g2.drawStr((128/2)-(width/2), (64/2) + (height/2), "- *");
	}
	else {
		u8g2.drawStr((128/2)-(width/2), (64/2) + (height/2), "- -");
	}
	u8g2.sendBuffer();
}
//--------------------------------------------------------------
void showBatteryVoltage() {
	u8g2.clearBuffer();
	u8g2.setFont(u8g2_font_logisoso46_tf);	// u8g2_font_inr38_mr u8g2_font_logisoso46_tf u8g2_font_logisoso26_tf

	int width = u8g2.getStrWidth("39.1");
	int height = 46;

	u8g2.setCursor(0, (64/2) + (height/2));
 	u8g2.print(esk8.boardPacket.batteryVoltage, 1);
	u8g2.sendBuffer();
}
//--------------------------------------------------------------
void updateLEDs() {
	// return;

	// bool changed;
	// bool throttleChanged = currentThrottle != esk8.controllerPacket.throttle;
	// bool encoderButtonChanged = currentEncoderButton != esk8.controllerPacket.encoderButton; 

	// if (!controllerStatusChanged && !vescStatusChanged && !throttleChanged && !encoderButtonChanged) {
	// 	return;
	// }

	// debug.print(STATE, "controllerStatusChanged: %d \n", controllerStatusChanged);
	// debug.print(STATE, "vescStatusChanged: %d \n", vescStatusChanged);
	// debug.print(STATE, "currentThrottle != esk8.controllerPacket.throttle: %d \n", 
	// 	currentThrottle != esk8.controllerPacket.throttle);
	// debug.print(STATE, "currentEncoderButton != esk8.controllerPacket.encoderButton: %d \n", 
	// 	currentEncoderButton != esk8.controllerPacket.encoderButton);

	// for (int i = 0; i < NUM_LEDS; i++) {
	// 	switch (i) {
	// 		case 0:	// controller online status
	// 		case 1: // VESC online
	// 			if (controllerCommsState == ST_ONLINE) {
	// 				strip.setPixelColor(i, strip.Color(0, 255, 0));
	// 			} else {
	// 				strip.setPixelColor(i, strip.Color(255, 0, 0));
	// 			}
	// 			break;
	// 		case 3: // VESC online
	// 		case 4: // VESC online
	// 		 	if (vescCommsState == ST_ONLINE) {
	// 		 		strip.setPixelColor(i, strip.Color(0, 255, 0));
	// 		 	} else {
	// 				strip.setPixelColor(i, strip.Color(255, 0, 0));
	// 			}
	// 			break;
	// 		case 6:
	// 		case 7:
	// 			if (controllerCommsState == ST_ONLINE) {
	// 				if (throttleChanged) {
	// 					if (esk8.controllerPacket.throttle > 127) {
	// 			 			strip.setPixelColor(i, strip.Color(0, 0, 255));	
	// 			 		} else if (esk8.controllerPacket.throttle < 127) {
	// 			 			strip.setPixelColor(i, strip.Color(0, 255, 0));	
	// 			 		} else {
	// 						strip.setPixelColor(i, strip.Color(0, 0, 0, 255));
	// 				 	}
	// 			 	}
	// 			}
	// 			else if (controllerStatusChanged) {
	// 				strip.setPixelColor(i, strip.Color(0, 0, 0));	
	// 			}
	// 			break;
	// 		case 9:
	// 		case 10:
	// 			if (encoderButtonChanged) {
	// 				if (esk8.controllerPacket.encoderButton == 1) {
	// 					strip.setPixelColor(i, strip.Color(0, 0, 255));	
	// 				} else {
	// 					strip.setPixelColor(i, strip.Color(0, 0, 0));
	// 				}
	// 			}
	// 			break;
	// 		default:
	// 			strip.setPixelColor(i, strip.Color(0, 0, 0));
	// 			break;
	// 	}

	// 	strip.show();
	// 	controllerStatusChanged = false;
	// 	vescStatusChanged = false;
	// 	currentThrottle = esk8.controllerPacket.throttle;
	// 	currentEncoderButton = esk8.controllerPacket.encoderButton;
	// }
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
		debug.print(VESC_COMMS, "The VESC values could not be read!\n");
	}
	return vescConnected;
}
//--------------------------------------------------------------------------------
void print_reset_reason(RESET_REASON reason, int cpu)
{
	debug.print(STARTUP, "Reboot reason (CPU%d): ", cpu);
	switch ( reason)
	{
		case 1 :  debug.print(STARTUP, "POWERON_RESET \n");break;          /**<1, Vbat power on reset*/
		case 3 :  debug.print(STARTUP, "SW_RESET \n");break;               /**<3, Software reset digital core*/
		case 4 :  debug.print(STARTUP, "OWDT_RESET \n");break;             /**<4, Legacy watch dog reset digital core*/
		case 5 :  debug.print(STARTUP, "DEEPSLEEP_RESET \n");break;        /**<5, Deep Sleep reset digital core*/
		case 6 :  debug.print(STARTUP, "SDIO_RESET \n");break;             /**<6, Reset by SLC module, reset digital core*/
		case 7 :  debug.print(STARTUP, "TG0WDT_SYS_RESET \n");break;       /**<7, Timer Group0 Watch dog reset digital core*/
		case 8 :  debug.print(STARTUP, "TG1WDT_SYS_RESET \n");break;       /**<8, Timer Group1 Watch dog reset digital core*/
		case 9 :  debug.print(STARTUP, "RTCWDT_SYS_RESET \n");break;       /**<9, RTC Watch dog Reset digital core*/
		case 10 : debug.print(STARTUP, "INTRUSION_RESET \n");break;       /**<10, Instrusion tested to reset CPU*/
		case 11 : debug.print(STARTUP, "TGWDT_CPU_RESET \n");break;       /**<11, Time Group reset CPU*/
		case 12 : debug.print(STARTUP, "SW_CPU_RESET \n");break;          /**<12, Software reset CPU*/
		case 13 : debug.print(STARTUP, "RTCWDT_CPU_RESET \n");break;      /**<13, RTC Watch dog Reset CPU*/
		case 14 : debug.print(STARTUP, "EXT_CPU_RESET \n");break;         /**<14, for APP CPU, reseted by PRO CPU*/
		case 15 : debug.print(STARTUP, "RTCWDT_BROWN_OUT_RESET \n");break;/**<15, Reset when the vdd voltage is not stable*/
		case 16 : debug.print(STARTUP, "RTCWDT_RTC_RESET \n");break;      /**<16, RTC Watch dog reset digital core and rtc module*/
		default : debug.print(STARTUP, "NO_MEAN\n");
	}
}