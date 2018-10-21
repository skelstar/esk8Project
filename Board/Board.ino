
#include <SPI.h>
#include <RF24Network.h> 
#include <RF24.h> 
#include "WiFi.h"
#include <esk8Lib.h>

#include <ESP8266VESC.h>
#include <VescUart.h>

#include "datatypes.h"
#include <debugHelper.h>
#include <rom/rtc.h>
#include <esp_int_wdt.h>
#include <esp_task_wdt.h>
#include <TaskScheduler.h>

/*--------------------------------------------------------------------------------*/

const char compile_date[] = __DATE__ " " __TIME__;

//--------------------------------------------------------------

bool controllerStatusChanged = true;
bool controllerOnline = false;
bool vescStatusChanged = true;
int currentThrottle = 127;
int currentEncoderButton = 0;

//--------------------------------------------------------------------------------
bool radioNumber = 0;
// WEMOS TTGO
#define 	SPI_CE			33	// white - do we need it?
#define 	SPI_CS			26	// green
const char boardSetup[] = "WEMOS TTGO Board";

RF24 radio(SPI_CE, SPI_CS);	// ce pin, cs pin
RF24Network network(radio);

esk8Lib esk8;

VescUart UART;

#define 	GET_VESC_DATA_INTERVAL	1000
#define 	CONTROLLER_ONLINE_MS	500

//--------------------------------------------------------------------------------
#define	STARTUP 			1 << 0	// 1
#define WARNING 			1 << 1	// 2
#define ERROR 				1 << 2	// 4
#define DEBUG 				1 << 3	// 8
#define COMMUNICATION 		1 << 4	// 16
#define HARDWARE			1 << 5	// 32
#define VESC_COMMS			1 << 6	// 64

debugHelper debug;

volatile long lastControllerPacketTime = 0;
volatile float packetData = 0.1;

//--------------------------------------------------------------
void loadPacketForController(bool gotDataFromVesc) {

	if (gotDataFromVesc) {
		debug.print(VESC_COMMS, "VESC online\n");
	}
	else {
		debug.print(VESC_COMMS, "VESC OFFLINE: mock data: %.1f\n", esk8.boardPacket.batteryVoltage);
	}
}

//--------------------------------------------------------------------------------

#define 	VESC_UART_RX		16		// orange - VESC 5
#define 	VESC_UART_TX		17		// green - VESC 6
#define 	VESC_UART_BAUDRATE	19200	// old: 9600

HardwareSerial Serial1(2);

//--------------------------------------------------------------

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
					// debug.print(ONLINE_STATUS, "-> TN_ONLINE \n");
					state = online ? ST_ONLINE : TN_OFFLINE;
					break;
				case ST_ONLINE:
					lastControllerOnlineTime = millis();
					state = online ? ST_ONLINE : TN_OFFLINE;
					break;
				case TN_OFFLINE:
					// debug.print(ONLINE_STATUS, "-> TN_OFFLINE \n");
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
portMUX_TYPE mmux = portMUX_INITIALIZER_UNLOCKED;

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

	taskENTER_CRITICAL(&mmux);

	UART.nunchuck.valueY = throttle;
	UART.nunchuck.lowerButton = false;

	UART.setNunchuckValues();

    taskEXIT_CRITICAL(&mmux);
}
//--------------------------------------------------------------------------------
TaskHandle_t RF24CommsRxTask;
//--------------------------------------------------------------------------------
void controllerPacketAvailable_Callback(int test) {
	debug.print(COMMUNICATION, "controllerPacketAvailable_Callback() \n");
}
//--------------------------------------------------------------
void setup()
{
    Serial.begin(9600);

	debug.init();

	// #define	STARTUP 			1 << 0	// 1
	// #define WARNING 			1 << 1	// 2
	// #define ERROR 				1 << 2	// 4
	// #define DEBUG 				1 << 3	// 8
	// #define CONTROLLER_COMMS 	1 << 4	// 16
	// #define HARDWARE			1 << 5	// 32
	// #define VESC_COMMS			1 << 6	// 64

	debug.addOption(STARTUP, "STARTUP");
	debug.addOption(DEBUG, "DEBUG");
	debug.addOption(ERROR, "ERROR");
	debug.addOption(COMMUNICATION, "COMMUNICATION");
	debug.addOption(VESC_COMMS, "VESC_COMMS");
	debug.setFilter( STARTUP | COMMUNICATION );

	debug.print(STARTUP, "%s\n", compile_date);
	debug.print(STARTUP, "NOTE: %s\n", boardSetup);

	print_reset_reason(rtc_get_reset_reason(0), 0);
	print_reset_reason(rtc_get_reset_reason(1), 1);

    // Setup serial connection to VESC
    Serial1.begin(VESC_UART_BAUDRATE);

	while (!Serial) {;}

	/** Define which ports to use as UART */
	UART.setSerialPort(&Serial1);

    radio.begin();
    radio.setPALevel(RF24_PA_MIN);

	esk8.begin(&radio, &network, esk8.RF24_BOARD, controllerPacketAvailable_Callback);
	//esk8.enableDebug();

	runner.startNow();
	runner.addTask(tSendToVESC);
	tSendToVESC.enable();

	debug.print(STARTUP, "Starting Core 0 \n");

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

#define SEND_TO_CONTROLLER_INTERVAL	500

//*************************************************************
void loop() {

	bool timeToUpdateController = millis() - intervalStarts > SEND_TO_CONTROLLER_INTERVAL;

	if (timeToUpdateController) {
		intervalStarts = millis();
		// update controller
		bool success = getVescValues();

		bool vescStatusChanged = vescStatus.serviceState(success);

		//update with data if VESC offline
		loadPacketForController(success);
	}

	esp_task_wdt_feed();

	runner.execute();

	vTaskDelay( 10 );
}
//*************************************************************
long nowms = 0;

void codeForRF24CommsRxTask( void *parameter ) {

	debug.print(STARTUP, "codeForReceiverTask() core: %d \n", xPortGetCoreID());

	for (;;) {

		bool controllerOnline = esk8.controllerOnline();

		controllerStatusChanged = controllerStatus.serviceState(controllerOnline);

		if (millis() - nowms > 500) {
			nowms = millis();

			if ( esk8.send() == false ) {	//  && esk8.controllerOnline()
				debug.print(COMMUNICATION, "esk8.send() == false \n");
			}
		}

		esk8.service();

		vTaskDelay( 10 );
	}
	vTaskDelete(NULL);
}
//*************************************************************

bool getVescValues() {

    VESCValues vescValues;

    if ( UART.getVescValues() ) {
    	vescConnected = true;
		// Serial.println(UART.data.rpm);
		Serial.println(UART.data.inpVoltage);
		// Serial.println(UART.data.ampHours);
		// Serial.println(UART.data.tachometerAbs);

		esk8.boardPacket.batteryVoltage = UART.data.inpVoltage;
    }
    else {
    	vescConnected = false;
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