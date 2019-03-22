
#include <OnlineStatusLib.h>
#include <ESP8266VESC.h>
#include <VescUart.h>	// https://github.com/SolidGeek/VescUart
#include "datatypes.h"
#include <debugHelper.h>
// #include <rom/rtc.h>
// #include <esp_int_wdt.h>
// #include <esp_task_wdt.h>
#include <TaskScheduler.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include "wificonfig.h";
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

/*--------------------------------------------------------------------------------*/

const char compile_date[] = __DATE__ " " __TIME__;
const char file_name[] = __FILE__;

//--------------------------------------------------------------

VescUart UART;

#define 	CONTROLLER_TIMEOUT							500
#define 	CONTROLLER_CONSECUTIVE_TIMEOUTS_ALLOWANCE	1
#define 	GET_VESC_DATA_AND_SEND_TO_CONTROLLER_INTERVAL	1000
#define 	SEND_TO_VESC_INTERVAL						200
#define 	GET_FROM_VESC_INTERVAL						1000


struct STICK_DATA {
	float batteryVoltage;
	float motorCurrent;
	bool moving;
	bool vescOnline;
};
STICK_DATA stickdata;

float ampHours = 0.0;

//--------------------------------------------------------------------------------

char auth[] = "5db4749b3d1f4aa5846fc01dfaf2188a";

//--------------------------------------------------------------------------------
#define	STARTUP 			1 << 0	// 1
#define WARNING 			1 << 1	// 2
#define VESC_COMMS 		    1 << 2	// 4
#define DEBUG 				1 << 3	// 8
// #define CONTROLLER_COMMS 	1 << 4	// 16
#define HARDWARE			1 << 5	// 32
#define STATUS				1 << 6	// 64 << MAX

debugHelper debug;

//--------------------------------------------------------------
// #define 	VESC_UART_RX		16		// orange
// #define 	VESC_UART_TX		17		// green
#define 	VESC_UART_BAUDRATE	115200	// old: 19200

HardwareSerial VescSerial(2);

#define INBUILT_LED	2

//--------------------------------------------------------------

// portMUX_TYPE mmux = portMUX_INITIALIZER_UNLOCKED;

//--------------------------------------------------------------

Scheduler runner;

bool ledOn;

void tGetFromVESC_callback();
Task tGetFromVESC( GET_FROM_VESC_INTERVAL, TASK_FOREVER, &tGetFromVESC_callback );
void tGetFromVESC_callback() {

	if (getVescValues() == false) {
		// vesc offline
	}
}

/**************************************************************/

void vescOfflineCallback() {
	debug.print(STATUS, "vescOfflineCallback();\n");
}
void vescOnlineCallback() {
	debug.print(STATUS, "vescOnlineCallback();\n");
}

OnlineStatusLib vescStatus(
	vescOfflineCallback, 
	vescOnlineCallback, 
	/*offlineNumConsecutiveTimesAllowance*/ 1, 
	/*debug*/ false);

/**************************************************************/

bool deviceConnected = false;

//--------------------------------------------------------------------------------

void setup()
{
	Serial.begin(9600);

    Serial.println("Starting BLE work!");

	pinMode(INBUILT_LED, OUTPUT);

	VescSerial.begin(VESC_UART_BAUDRATE);
	UART.setSerialPort(&VescSerial);

	Serial.println("Ready");

	// ArduinoOTA.setHostname("Monitor OTA");  // For OTA - Use your own device identifying name
	// ArduinoOTA.begin();  // For OTA

    // ArduinoOTA
    //     .onStart([]() {
    //         String type;
    //         if (ArduinoOTA.getCommand() == U_FLASH)
    //         type = "sketch";
    //         else // U_SPIFFS
    //         type = "filesystem";

    //         // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    //         Serial.println("Start updating " + type);
    //     })
    //     .onEnd([]() {
    //         Serial.println("\nEnd");
    //     })
    //     .onProgress([](unsigned int progress, unsigned int total) {
    //         Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    //     })
    //     .onError([](ota_error_t error) {
    //         Serial.printf("Error[%u]: ", error);
    //         if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    //         else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    //         else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    //         else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    //         else if (error == OTA_END_ERROR) Serial.println("End Failed");
    //     });

    // ArduinoOTA.begin();

	debug.init();
	debug.addOption(STARTUP, "STARTUP");
	debug.addOption(DEBUG, "DEBUG");
	debug.addOption(VESC_COMMS, "VESC_COMMS");
	// debug.addOption(CONTROLLER_COMMS, "CONTROLLER");
	debug.addOption(HARDWARE, "HARDWARE");
	debug.addOption(STATUS, "STATUS");
	// debug.setFilter( STARTUP | STATUS | CONTROLLER_COMMS );
	// debug.setFilter( STARTUP | CONTROLLER_COMMS | DEBUG );
	// debug.setFilter( STARTUP | VESC_COMMS | CONTROLLER_COMMS | HARDWARE);
	debug.setFilter( STARTUP | VESC_COMMS );

    debug.print(STARTUP, "%s\n", file_name);
	debug.print(STARTUP, "%s\n", compile_date);

	/** Define which ports to use as UART */

	bool vescOnline = getVescValues();
	// debug.print(STARTUP, "%s\n", vescOnline ? "VESC Online!" : "ERROR: VESC Offline!");

	runner.startNow();
	runner.addTask(tGetFromVESC);
	tGetFromVESC.enable();
}

//*************************************************************

long now = 0;

void loop() {

	// esp_task_wdt_feed();

	runner.execute();
}
//*************************************************************
bool controllerOnline = true;

//--------------------------------------------------------------
bool getVescValues() {
	/*
	struct dataPackage {
		float avgMotorCurrent;
		float avgInputCurrent;
		float dutyCycleNow;
		%ld rpm;
		float inpVoltage;
		float ampHours;
		float ampHoursCharged;
		long tachometer;
		long tachometerAbs;
	}; */

    bool success = UART.getVescValues();
	if ( success ) {

		stickdata.batteryVoltage = UART.data.inpVoltage;
		ampHours = UART.data.ampHours;
		stickdata.moving = UART.data.tachometer > 100;
		stickdata.motorCurrent = UART.data.avgMotorCurrent;
		stickdata.vescOnline = true;

		Serial.printf("inpVoltage: %.1f\n", UART.data.inpVoltage);
		Serial.printf("ampHours: %.1f\n", UART.data.ampHours);
		Serial.printf("rpm: %ul\n", UART.data.rpm);
		// bool moving = UART.data.rpm > 100;
		bool accelerating = UART.data.avgMotorCurrent > 0.2;
		Serial.printf("moving: %d accelerating: %d \n", stickdata.moving, accelerating);
		Serial.printf("motor current: %.1f\n", UART.data.avgMotorCurrent);
		Serial.printf("Odometer: %ul\n", UART.data.tachometerAbs/42);
	}
	else {
		stickdata.vescOnline = false;
		stickdata.batteryVoltage = 0.0;
		stickdata.moving = false;
		stickdata.motorCurrent = 0.0;

		debug.print(VESC_COMMS, "vescOnline = false\n");	
	}
    return success;
}
//--------------------------------------------------------------
// void print_reset_reason(RESET_REASON reason, int cpu)
// {
// 	debug.print(STARTUP, "Reboot reason (CPU%d): ", cpu);
// 	switch ( reason)
// 	{
// 		case 1 :  debug.print(STARTUP, "POWERON_RESET \n");break;          /**<1, Vbat power on reset*/
// 		case 3 :  debug.print(STARTUP, "SW_RESET \n");break;               /**<3, Software reset digital core*/
// 		case 4 :  debug.print(STARTUP, "OWDT_RESET \n");break;             /**<4, Legacy watch dog reset digital core*/
// 		case 5 :  debug.print(STARTUP, "DEEPSLEEP_RESET \n");break;        /**<5, Deep Sleep reset digital core*/
// 		case 6 :  debug.print(STARTUP, "SDIO_RESET \n");break;             /**<6, Reset by SLC module, reset digital core*/
// 		case 7 :  debug.print(STARTUP, "TG0WDT_SYS_RESET \n");break;       /**<7, Timer Group0 Watch dog reset digital core*/
// 		case 8 :  debug.print(STARTUP, "TG1WDT_SYS_RESET \n");break;       /**<8, Timer Group1 Watch dog reset digital core*/
// 		case 9 :  debug.print(STARTUP, "RTCWDT_SYS_RESET \n");break;       /**<9, RTC Watch dog Reset digital core*/
// 		case 10 : debug.print(STARTUP, "INTRUSION_RESET \n");break;       /**<10, Instrusion tested to reset CPU*/
// 		case 11 : debug.print(STARTUP, "TGWDT_CPU_RESET \n");break;       /**<11, Time Group reset CPU*/
// 		case 12 : debug.print(STARTUP, "SW_CPU_RESET \n");break;          /**<12, Software reset CPU*/
// 		case 13 : debug.print(STARTUP, "RTCWDT_CPU_RESET \n");break;      /**<13, RTC Watch dog Reset CPU*/
// 		case 14 : debug.print(STARTUP, "EXT_CPU_RESET \n");break;         /**<14, for APP CPU, reseted by PRO CPU*/
// 		case 15 : debug.print(STARTUP, "RTCWDT_BROWN_OUT_RESET \n");break;/**<15, Reset when the vdd voltage is not stable*/
// 		case 16 : debug.print(STARTUP, "RTCWDT_RTC_RESET \n");break;      /**<16, RTC Watch dog reset digital core and rtc module*/
// 		default : debug.print(STARTUP, "NO_MEAN\n");
// 	}
// }