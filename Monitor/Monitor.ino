#include <OnlineStatusLib.h>
#include <debugHelper.h>
#include "vesc_comm.h";
#include <TaskScheduler.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include "wificonfig.h";
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <BlynkSimpleEsp32.h>
#include "RemoteDebug.h"        //https://github.com/JoaoLopesF/RemoteDebug

// https://raw.githubusercontent.com/LilyGO/TTGO-TS/master/Image/TS%20V1.0.jpg

#include "display_gfx.h";

/*--------------------------------------------------------------------------------*/

const char compile_date[] = __DATE__ " " __TIME__;
const char file_name[] = __FILE__;

RemoteDebug Debug;
#define HOST_NAME "remotedebug"

//--------------------------------------------------------------

uint8_t vesc_packet[PACKET_MAX_LENGTH];

#define 	GET_FROM_VESC_INTERVAL						500

struct VESC_DATA {
	float batteryVoltage;
	float motorCurrent;
	bool moving;
	bool vescOnline;
	float ampHours;
};
VESC_DATA vescdata;

#define DATA_NAMESPACE 	"data"
#define DATA_AMP_HOURS_USED_THIS_CHARGE	"totalAmpHours"
#define DATA_LAST_VOLTAGE_READ "lastVolts"

#include "nvmstorage.h";

//--------------------------------------------------------------------------------


//--------------------------------------------------------------------------------

char auth[] = "5db4749b3d1f4aa5846fc01dfaf2188a";

#define BLYNK_PRINT Serial

BLYNK_WRITE(V1) {
	if ( param.asInt() == 1) {
		// Serial.printf("button clicked in app\n");
	}
}

//--------------------------------------------------------------------------------
#define	STARTUP 			1 << 0	// 1
#define WARNING 			1 << 1	// 2
#define VESC_COMMS 		    1 << 2	// 4
//#define DEBUG 				1 << 3	// 8
// #define CONTROLLER_COMMS 	1 << 4	// 16
#define HARDWARE			1 << 5	// 32
#define STATUS				1 << 6	// 64 << MAX

// debugHelper debug;

//--------------------------------------------------------------
// #define 	VESC_UART_RX		16		// orange
// #define 	VESC_UART_TX		17		// green
#define 	VESC_UART_BAUDRATE	115200	// old: 19200

//--------------------------------------------------------------

// portMUX_TYPE mmux = portMUX_INITIALIZER_UNLOCKED;

//--------------------------------------------------------------

Scheduler runner;

bool ledOn;

bool firstTime = false;
float lastVoltsRead = 0.0;
float lastStableVoltsRead = 0.0;
bool interimUpdated = false;	// when not moving
bool alreadyStoreValues = false;
bool appendAmpHoursOnPowerDown = false;
long lastReport = 0;

void tGetFromVESC_callback();
Task tGetFromVESC( GET_FROM_VESC_INTERVAL, TASK_FOREVER, &tGetFromVESC_callback );
void tGetFromVESC_callback() {

	float battVoltsOld = vescdata.batteryVoltage;

	if (getVescValues() == false) {
		// vesc offline
		if (millis() - lastReport > 5000) {
			lastReport = millis();
			debugD("vesc offline\n");
		}
	}
	else {
		bool updateDisplay = battVoltsOld != vescdata.batteryVoltage;
		if ( updateDisplay ) {
			// debugD("Total: %.1f\n", recallFloat(DATA_AMP_HOURS_USED_THIS_CHARGE));
			// debugD("updating display: %.1f %.1f \n", battVoltsOld, vescdata.batteryVoltage);
			drawBatteryTopScreen( vescdata.batteryVoltage );
			drawAmpHoursUsed( vescdata.ampHours );
			drawTotalAmpHours( recallFloat(DATA_AMP_HOURS_USED_THIS_CHARGE) );
		}
		if ( vescPoweringDown(vescdata.batteryVoltage) ) {
			// store values (not batteryVoltage)
			if ( alreadyStoreValues == false ) {
				storeValuesOnPowerdown(vescdata);
				alreadyStoreValues = true;
				debugD("stored values on power down\n");
				float tripAH = vescdata.ampHours;
				float totalAH = recallFloat(DATA_AMP_HOURS_USED_THIS_CHARGE);
				sendBlynkPowerdownNotification(tripAH, totalAH);
			}
		}
		else {
			if ( vescdata.moving == false ) {
				// save volts
				lastStableVoltsRead = vescdata.batteryVoltage;
				Blynk.virtualWrite(V0, vescdata.batteryVoltage);
				Blynk.virtualWrite(V3, vescdata.ampHours);
				// send notification if first time
				if (firstTime == false) {
					firstTime = true;
					sendBlynkStartupNotification(vescdata.batteryVoltage);
					// debug.print(STARTUP, "First time - Board ampHours: %.1f, stored ampHours: %.1f\n", vescdata.ampHours, recallFloat(DATA_AMP_HOURS_USED_THIS_CHARGE));
				}
			}
			else {
				// moving
			}
			lastVoltsRead = vescdata.batteryVoltage;
		}
	}
}

void sendBlynkStartupNotification(float volts) {
	char message[100];
	char battString[6];
	dtostrf(volts, 2, 1, battString); // Leave room for too large numbers!
	sprintf(message, "Battery: %sv", battString);
	// Serial.printf("Notification (startup) sent - %s\n", message);
	Blynk.notify(message);
}

void sendBlynkPowerdownNotification(float tripAH, float totalAH) {
	char message[100];
	char tripAHString[10];
	char totalAHString[10];
	dtostrf(tripAH, 5, 1, tripAHString); // Leave room for too large numbers!
	dtostrf(totalAH, 5, 1, totalAHString); // Leave room for too large numbers!
	sprintf(message, "AmpHours: %sAH, Total: %sAH", tripAHString, totalAHString);
	Blynk.notify(message);
	// Serial.printf("Notification (powerdown) sent - '%s'\n", message);
}

bool vescPoweringDown(float volts) {
	return volts < 32.0;
}

/**************************************************************/

void vescOfflineCallback() {
	// debug.print(STATUS, "vescOfflineCallback();\n");
}

void vescOnlineCallback() {
	// debug.print(STATUS, "vescOnlineCallback();\n");
}

OnlineStatusLib vescStatus(
	vescOfflineCallback, 
	vescOnlineCallback, 
	/*offlineNumConsecutiveTimesAllowance*/ 1, 
	/*debug*/ false);

/**************************************************************/

bool deviceConnected = false;

char* filename = "/data.txt";

//--------------------------------------------------------------------------------

void setup()
{
	// Serial.begin(9600);

  	vesc_comm_init(VESC_UART_BAUDRATE);

	setupWifiOTA();

	String hostNameWifi = HOST_NAME;
    hostNameWifi.concat(".local");
	Debug.begin(HOST_NAME); // Initialize the WiFi server
    Debug.setResetCmdEnabled(true); // Enable the reset command
	Debug.showProfiler(true); // Profiler (Good to measure times, to optimize codes)
	Debug.showColors(true); // Colors

	setupDisplay();

	Blynk.begin(auth, ssid, pass);

	bool vescOnline = getVescValues();

	runner.startNow();
	runner.addTask(tGetFromVESC);
	tGetFromVESC.enable();

	drawBatteryTopScreen(vescdata.batteryVoltage);
}

//*************************************************************

long now = 0;

void loop() {

	Debug.handle();
	
	ArduinoOTA.handle();

	runner.execute();

	Blynk.run();
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

    bool success = vesc_comm_fetch_packet(vesc_packet) > 0;

	if ( success ) {

		// debugD("%d (rpm) %.1f (Ah) %u (tacho) %u (tachoabs)\n",
		// 	vesc_comm_get_rpm(vesc_packet),
		// 	vesc_comm_get_amphours_discharged(vesc_packet),
		// 	vesc_comm_get_tachometer(vesc_packet),
		// 	vesc_comm_get_tachometer_abs(vesc_packet)
		// );

		vescdata.batteryVoltage = vesc_comm_get_voltage(vesc_packet);
		vescdata.moving = vesc_comm_get_rpm(vesc_packet) > 50;
		vescdata.motorCurrent = vesc_comm_get_motor_current(vesc_packet);	// UART.data.avgMotorCurrent;
		vescdata.ampHours = vesc_comm_get_amphours_discharged(vesc_packet);
		vescdata.vescOnline = true;
	}
	else {
		vescdata.vescOnline = false;
		vescdata.batteryVoltage = 0.0;
		vescdata.moving = false;
		vescdata.motorCurrent = 0.0;
	}
    return success;
}

void setupWifiOTA() {

	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	while (WiFi.waitForConnectResult() != WL_CONNECTED) {
		// delay(2000);
		// ESP.restart();
	}

	ArduinoOTA.setHostname("Monitor OTA TS");  // For OTA - Use your own device identifying name
	ArduinoOTA.begin();  // For OTA

    ArduinoOTA
        .onStart([]() {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH)
	            type = "sketch";
            else // U_SPIFFS
    	        type = "filesystem";
        })
        .onEnd([]() {
        })
        .onProgress([](unsigned int progress, unsigned int total) {
        })
        .onError([](ota_error_t error) {
        });

    ArduinoOTA.begin();
}
//--------------------------------------------------------------
// void print_reset_reason(RESET_REASON reason, int cpu)
// {
// 	// debug.print(STARTUP, "Reboot reason (CPU%d): ", cpu);
// 	switch ( reason)
// 	{
// 		case 1 :  // debug.print(STARTUP, "POWERON_RESET \n");break;          /**<1, Vbat power on reset*/
// 		case 3 :  // debug.print(STARTUP, "SW_RESET \n");break;               /**<3, Software reset digital core*/
// 		case 4 :  // debug.print(STARTUP, "OWDT_RESET \n");break;             /**<4, Legacy watch dog reset digital core*/
// 		case 5 :  // debug.print(STARTUP, "DEEPSLEEP_RESET \n");break;        /**<5, Deep Sleep reset digital core*/
// 		case 6 :  // debug.print(STARTUP, "SDIO_RESET \n");break;             /**<6, Reset by SLC module, reset digital core*/
// 		case 7 :  // debug.print(STARTUP, "TG0WDT_SYS_RESET \n");break;       /**<7, Timer Group0 Watch dog reset digital core*/
// 		case 8 :  // debug.print(STARTUP, "TG1WDT_SYS_RESET \n");break;       /**<8, Timer Group1 Watch dog reset digital core*/
// 		case 9 :  // debug.print(STARTUP, "RTCWDT_SYS_RESET \n");break;       /**<9, RTC Watch dog Reset digital core*/
// 		case 10 : // debug.print(STARTUP, "INTRUSION_RESET \n");break;       /**<10, Instrusion tested to reset CPU*/
// 		case 11 : // debug.print(STARTUP, "TGWDT_CPU_RESET \n");break;       /**<11, Time Group reset CPU*/
// 		case 12 : // debug.print(STARTUP, "SW_CPU_RESET \n");break;          /**<12, Software reset CPU*/
// 		case 13 : // debug.print(STARTUP, "RTCWDT_CPU_RESET \n");break;      /**<13, RTC Watch dog Reset CPU*/
// 		case 14 : // debug.print(STARTUP, "EXT_CPU_RESET \n");break;         /**<14, for APP CPU, reseted by PRO CPU*/
// 		case 15 : // debug.print(STARTUP, "RTCWDT_BROWN_OUT_RESET \n");break;/**<15, Reset when the vdd voltage is not stable*/
// 		case 16 : // debug.print(STARTUP, "RTCWDT_RTC_RESET \n");break;      /**<16, RTC Watch dog reset digital core and rtc module*/
// 		default : // debug.print(STARTUP, "NO_MEAN\n");
// 	}
// }

