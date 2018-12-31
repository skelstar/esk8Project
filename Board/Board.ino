
#include <SPI.h>
#include <RF24Network.h> 
#include <RF24.h> 
#include <esk8Lib.h>
#include <OnlineStatusLib.h>

#include <ESP8266VESC.h>
#include <VescUart.h>	// https://github.com/SolidGeek/VescUart

#include "datatypes.h"
#include <debugHelper.h>
#include <rom/rtc.h>
#include <esp_int_wdt.h>
#include <esp_task_wdt.h>
#include <TaskScheduler.h>

/*--------------------------------------------------------------------------------*/

const char compile_date[] = __DATE__ " " __TIME__;
const char file_name[] = __FILE__;

//--------------------------------------------------------------

bool controllerStatusChanged = true;
bool vescStatusChanged = true;
int currentThrottle = 127;
int currentEncoderButton = 0;
volatile long lastRxFromController = 0;
//--------------------------------------------------------------------------------
bool radioNumber = 0;
// TTGO-TQ
// #define SPI_CE        15
// #define SPI_CS        13
// #define NRF24_POWER_PIN        2
// M5Stack
// #define SPI_CE        5 
// #define SPI_CS        13
// DEV board
#define SPI_CE        33    	// white/purple
#define SPI_CS        26  	// green

RF24 radio(SPI_CE, SPI_CS);	// ce pin, cs pin
RF24Network network(radio);

esk8Lib esk8;

VescUart UART;

#define 	GET_VESC_DATA_INTERVAL	1000
#define 	CONTROLLER_TIMEOUT		300

//--------------------------------------------------------------------------------
#define	STARTUP 			1 << 0	// 1
#define WARNING 			1 << 1	// 2
#define ERROR 				1 << 2	// 4
#define DEBUG 				1 << 3	// 8
#define COMMUNICATION 		1 << 4	// 16
#define HARDWARE			1 << 5	// 32
#define STATUS				1 << 6	// 64 << MAX

debugHelper debug;

volatile long lastControllerPacketTime = 0;
volatile float packetData = 0.1;

//--------------------------------------------------------------------------------
#define 	VESC_UART_RX		16		// orange - VESC 5
#define 	VESC_UART_TX		17		// green - VESC 6
#define 	VESC_UART_BAUDRATE	19200	// old: 9600

HardwareSerial Serial1(2);

//--------------------------------------------------------------

int txCharsLength = 0;

void controllerPacketAvailableCallback( uint16_t from ) {
	lastRxFromController = millis();
	Serial.printf("r");
	txCharsLength++;
	if ( txCharsLength > 30 ) {
		txCharsLength = 0;
		Serial.printf("\n");
	}
}

//--------------------------------------------------------------

void controllerOfflineCallback() {
	esk8.controllerPacket.throttle = 127;
	debug.print(STATUS, "controllerOfflineCallback(); (%d)\n", millis() - lastRxFromController);
}
void controllerOnlineCallback() {
	debug.print(STATUS, "controllerOnlineCallback();\n");
}
void vescOfflineCallback() {
	debug.print(STATUS, "vescOfflineCallback();\n");
}
void vescOnlineCallback() {
	debug.print(STATUS, "vescOnlineCallback();\n");
}

OnlineStatusLib controllerStatus(controllerOfflineCallback, controllerOnlineCallback, /*debug*/ false);
OnlineStatusLib vescStatus(vescOfflineCallback, vescOnlineCallback, /*debug*/ false);

//--------------------------------------------------------------

portMUX_TYPE mmux = portMUX_INITIALIZER_UNLOCKED;

//--------------------------------------------------------------------------------

#define SEND_TO_CONTROLLER_INTERVAL	300
#define SEND_TO_VESC_INTERVAL	300

Scheduler runner;

void tSendToVESC_callback();
Task tSendToVESC(SEND_TO_VESC_INTERVAL, TASK_FOREVER, &tSendToVESC_callback);
void tSendToVESC_callback() {

	taskENTER_CRITICAL(&mmux);

	UART.nunchuck.valueY = esk8.controllerPacket.throttle;
	UART.nunchuck.lowerButton = false;

  	UART.setNunchuckValues();

    taskEXIT_CRITICAL(&mmux);
}

void tGetFromVESC_callback();
Task tGetFromVESC(SEND_TO_CONTROLLER_INTERVAL, TASK_FOREVER, &tGetFromVESC_callback);
void tGetFromVESC_callback() {

	// bool vescOnline = getVescValues();
	// bool vescStatusChanged = vescStatus.serviceState(vescOnline);
	// // esk8.boardPacket.vescOnline = vescOnline;

	// esk8.sendPacketToController();
}
//--------------------------------------------------------------------------------
TaskHandle_t RF24CommsRxTask;
//--------------------------------------------------------------------------------
void setup()
{
    Serial.begin(9600);
    Serial1.begin(VESC_UART_BAUDRATE);

	debug.init();

	debug.addOption(STARTUP, "STARTUP");
	debug.addOption(DEBUG, "DEBUG");
	debug.addOption(ERROR, "ERROR");
	debug.addOption(COMMUNICATION, "COMMUNICATION");
	debug.addOption(STATUS, "STATUS");
	debug.setFilter( STARTUP | STATUS | COMMUNICATION );
	// debug.setFilter( STARTUP | COMMUNICATION | DEBUG );

    debug.print(STARTUP, "%s\n", file_name);
	debug.print(STARTUP, "%s\n", compile_date);

	// print_reset_reason(rtc_get_reset_reason(0), 0);
	// print_reset_reason(rtc_get_reset_reason(1), 1);


  	while (!Serial) {;}

	/** Define which ports to use as UART */
	UART.setSerialPort(&Serial1);

	setupRadio();

	runner.startNow();
	runner.addTask(tSendToVESC);
	// runner.addTask(tGetFromVESC);
	tSendToVESC.enable();
	// tGetFromVESC.enable();

	debug.print(STARTUP, "Starting Core 0 \n");

	debug.print(STARTUP, "loop() core: %d \n", xPortGetCoreID());

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
long intervalStarts = 0;
int otaHandleTimeoutMillis = 30*1000;

void loop() {

	bool timeToUpdateController = millis() - intervalStarts > SEND_TO_CONTROLLER_INTERVAL;

	if ( timeToUpdateController ) {
		intervalStarts = millis();
		// update controller
		bool vescOnline = getVescValues();
		esk8.sendPacketToController();

		bool vescStatusChanged = vescStatus.serviceState(vescOnline);
	}

	esp_task_wdt_feed();

	esk8.service();

	runner.execute();

	vTaskDelay( 10 );
}
//*************************************************************
long nowms = 0;
bool controllerOnline = true;

void codeForRF24CommsRxTask( void *parameter ) {

	debug.print(STARTUP, "codeForReceiverTask() core: %d \n", xPortGetCoreID());

	for (;;) {

		bool controllerOnline = millis() - lastRxFromController < CONTROLLER_TIMEOUT;

		controllerStatusChanged = controllerStatus.serviceState(controllerOnline);

		vTaskDelay( 10 );
	}
	vTaskDelete(NULL);
}

/***************************************************************/
bool getVescValues() {

    bool success = UART.getVescValues();
	// esk8.boardPacket.vescOnline = success;
	// esk8.boardPacket.batteryVoltage = UART.data.inpVoltage;
	// debug.print(DEBUG, "esk8.boardPacket.batteryVoltage: %.1f \n", esk8.boardPacket.batteryVoltage);
    return success;
}
//--------------------------------------------------------------
void setupRadio() {
	SPI.begin();     
	// TTGO TQ
	#ifdef NRF24_POWER_PIN
	pinMode(NRF24_POWER_PIN, OUTPUT);
	digitalWrite(NRF24_POWER_PIN, HIGH);   
	#endif
	radio.begin();
	radio.setAutoAck(true);
	esk8.begin(&radio, &network, esk8.RF24_BOARD, controllerPacketAvailableCallback);
}

//--------------------------------------------------------------------------------

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