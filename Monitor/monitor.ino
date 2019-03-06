
#include <OnlineStatusLib.h>
#include <ESP8266VESC.h>
#include <VescUart.h>	// https://github.com/SolidGeek/VescUart
#include "datatypes.h"
#include <debugHelper.h>
#include <rom/rtc.h>
#include <esp_int_wdt.h>
#include <esp_task_wdt.h>
#include <TaskScheduler.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include "wificonfig.h";

#define BLYNK_PRINT Serial

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
#define 	VESC_UART_RX		16		// orange - VESC 5
#define 	VESC_UART_TX		17		// green - VESC 6
#define 	VESC_UART_BAUDRATE	19200	// old: 9600

HardwareSerial Serial1(2);

//--------------------------------------------------------------

portMUX_TYPE mmux = portMUX_INITIALIZER_UNLOCKED;

//--------------------------------------------------------------

Scheduler runner;

void tSendToVESC_callback();
Task tSendToVESC(SEND_TO_VESC_INTERVAL, TASK_FOREVER, &tSendToVESC_callback);
void tSendToVESC_callback() {

	taskENTER_CRITICAL(&mmux);

    taskEXIT_CRITICAL(&mmux);
}

void tGetFromVESC_callback();
Task tGetFromVESC( GET_FROM_VESC_INTERVAL, TASK_FOREVER, &tGetFromVESC_callback );
void tGetFromVESC_callback() {

	taskENTER_CRITICAL(&mmux);

	if (getVescValues() == false) {
		// vesc offline
	}

    taskEXIT_CRITICAL(&mmux);
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


BLYNK_APP_CONNECTED() {
  Serial.println("App Connected.");
}

// This is called when Smartphone App is closed
BLYNK_APP_DISCONNECTED() {
  Serial.println("App Disconnected.");
}

// This function will be called every time Slider Widget
// in Blynk app writes values to the Virtual Pin 1
BLYNK_WRITE(V1)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V1 to a variable
  // You can also use:
  // String i = param.asStr();
  // double d = param.asDouble();
  Serial.print("V1 Slider value is: ");
  Serial.println(pinValue);
}



//--------------------------------------------------------------------------------
TaskHandle_t RF24CommsRxTask;
//--------------------------------------------------------------------------------
void setup()
{
    Serial.begin(9600);

	Blynk.begin(auth, ssid, pass);

	Serial.println("Ready");

	Blynk.notify("Yaaay... button is pressed!");

    // Serial1.begin(VESC_UART_BAUDRATE);

	// debug.init();
	// debug.addOption(STARTUP, "STARTUP");
	// debug.addOption(DEBUG, "DEBUG");
	// debug.addOption(VESC_COMMS, "VESC_COMMS");
	// // debug.addOption(CONTROLLER_COMMS, "CONTROLLER");
	// debug.addOption(HARDWARE, "HARDWARE");
	// debug.addOption(STATUS, "STATUS");
	// // debug.setFilter( STARTUP | STATUS | CONTROLLER_COMMS );
	// // debug.setFilter( STARTUP | CONTROLLER_COMMS | DEBUG );
	// // debug.setFilter( STARTUP | VESC_COMMS | CONTROLLER_COMMS | HARDWARE);
	// debug.setFilter( STARTUP | VESC_COMMS );

    // debug.print(STARTUP, "%s\n", file_name);
	// debug.print(STARTUP, "%s\n", compile_date);

	// print_reset_reason(rtc_get_reset_reason(0), 0);
	// print_reset_reason(rtc_get_reset_reason(1), 1);

  	// //while (!Serial) {;}

	// /** Define which ports to use as UART */
	// UART.setSerialPort(&Serial1);

	// bool vescOnline = getVescValues();
	// debug.print(STARTUP, "%s\n", vescOnline ? "VESC Online!" : "ERROR: VESC Offline!");

	// runner.startNow();
	// //runner.addTask(tSendToVESC);
	// //tSendToVESC.enable();
	// runner.addTask(tGetFromVESC);
	// tGetFromVESC.enable();

	// debug.print(STARTUP, "Starting Core 0 \n");

	// debug.print(STARTUP, "loop() core: %d \n", xPortGetCoreID());

	xTaskCreatePinnedToCore (
		codeForRF24CommsRxTask,	// function
		"RF24 Comms Rx Task",		// name
		10000,			// stack
		NULL,			// parameter
		1,				// priority
		&RF24CommsRxTask,	// handle
		0);				// port	
	vTaskDelay(100);
}

//*************************************************************

void loop() {

	esp_task_wdt_feed();

	runner.execute();

	vTaskDelay( 10 );

	// Serial.printf(".");

	Blynk.run();
}
//*************************************************************
long nowms = 0;
bool controllerOnline = true;

void codeForRF24CommsRxTask( void *parameter ) {

	debug.print(STARTUP, "codeForReceiverTask() core: %d \n", xPortGetCoreID());

	for (;;) {

		vTaskDelay( 10 );
	}
	vTaskDelete(NULL);
}

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

    bool success = false;	// UART.getVescValues();
	if ( success ) {
		// esk8.boardPacket.batteryVoltage = UART.data.inpVoltage;
		// esk8.boardPacket.odometer = UART.data.tachometerAbs/42;
		// esk8.boardPacket.areMoving = UART.data.rpm > 100;
		// Serial.printf("areMoving: %d\n", esk8.boardPacket.areMoving);

		// Serial.print("avgMotorCurrent: "); 	Serial.println(UART.data.avgMotorCurrent);
		// Serial.print("avgInputCurrent: "); 	Serial.println(UART.data.avgInputCurrent);
		// Serial.print("dutyCycleNow: "); 	Serial.println(UART.data.dutyCycleNow);
		// Serial.printf("rpm: %ld\n", UART.data.rpm);
		// Serial.print("inputVoltage: "); 	Serial.println(UART.data.inpVoltage);
		// Serial.print("ampHours: "); 		Serial.println(UART.data.ampHours);
		// Serial.print("ampHoursCharges: "); 	Serial.println(UART.data.ampHoursCharged);
		// Serial.print("tachometer: "); 		Serial.println(UART.data.tachometer;);
		// Serial.print("tachometerAbs: "); 	Serial.println(UART.data.tachometerAbs);
	}
	else {
		// debug.print(VESC_COMMS, "vescOnline = false\n");	
	}
    return success;
}
//--------------------------------------------------------------
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