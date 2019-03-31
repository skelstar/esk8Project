#include <OnlineStatusLib.h>
#include <debugHelper.h>
#include "vesc_comm.h";
#include <TaskScheduler.h>
#include <rom/rtc.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include "wificonfig.h";
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <BlynkSimpleEsp32.h>
// #include "RemoteDebug.h"        //https://github.com/JoaoLopesF/RemoteDebug
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager DEVELOPER BRANCH

// https://raw.githubusercontent.com/LilyGO/TTGO-TS/master/Image/TS%20V1.0.jpg

#include "display_gfx.h";

/*--------------------------------------------------------------------------------*/

const char compile_date[] = __DATE__ " " __TIME__;
const char file_name[] = __FILE__;

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

bool connectedToWifi = false;


#define PREFS_NAMESPACE 			"data"
#define PREFS_TOTAL_AMP_HOURS	"totalAmpHours"
// #define PREFS_TRIP_AMP_HOURS		"tripAmpHours"
#define PREFS_POWERED_DOWN		"poweredDown"
#define PREFS_LAST_VOLTAGE_READ 	"lastVolts"

#include "nvmstorage.h";

//--------------------------------------------------------------------------------


//--------------------------------------------------------------------------------

char auth[] = "5db4749b3d1f4aa5846fc01dfaf2188a";

#define BLYNK_PRINT Serial

BLYNK_WRITE(V1) {
	if ( param.asInt() == 1) {
		storeFloat( PREFS_TOTAL_AMP_HOURS, 0.0 );
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
			// debugD("vesc offline\n");
		}
	}
	else {
		bool updateDisplay = battVoltsOld != vescdata.batteryVoltage;
		if ( updateDisplay ) {
			drawBatteryTopScreen( vescdata.batteryVoltage, connectedToWifi );
			drawAmpHoursUsed( vescdata.ampHours );
			drawTotalAmpHours( recallFloat(PREFS_TOTAL_AMP_HOURS) );
		}
		if ( vescPoweringDown(vescdata.batteryVoltage) ) {
			// store values (not batteryVoltage)
			if ( alreadyStoreValues == false ) {
				storeValuesOnPowerdown(vescdata);
				alreadyStoreValues = true;
				float tripAH = vescdata.ampHours;
				float totalAH = recallFloat(PREFS_TOTAL_AMP_HOURS);
				sendBlynkPowerdownNotification(tripAH, totalAH);
			}
		}
		else {
			if ( vescdata.moving == false ) {
				// save volts
				lastStableVoltsRead = vescdata.batteryVoltage;
				if ( connectedToWifi ) {
					Blynk.virtualWrite(V0, vescdata.batteryVoltage);
					Blynk.virtualWrite(V3, vescdata.ampHours);
				}
				// send notification if first time
				if (firstTime == false) {
					firstTime = true;
					sendBlynkStartupNotification(vescdata.batteryVoltage);
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
	if ( connectedToWifi ) {
		char message[100];
		char battString[6];
		dtostrf(volts, 2, 1, battString); // Leave room for too large numbers!
		sprintf(message, "Battery: %sv", battString);
		// Serial.printf("Notification (startup) sent - %s\n", message);
		Blynk.notify(message);
	}
}

void sendBlynkPowerdownNotification(float tripAH, float totalAH) {
	if ( connectedToWifi ) {
		char message[100];
		char tripAHString[10];
		char totalAHString[10];
		dtostrf(tripAH, 5, 1, tripAHString); // Leave room for too large numbers!
		dtostrf(totalAH, 5, 1, totalAHString); // Leave room for too large numbers!
		sprintf(message, "AmpHours: %sAH, Total: %sAH", tripAHString, totalAHString);
		Blynk.notify(message);
		// Serial.printf("Notification (powerdown) sent - '%s'\n", message);
	}
}

bool vescPoweringDown(float volts) {
	return volts < 32.0;
}

/**************************************************************/

void vescOfflineCallback() {
}

void vescOnlineCallback() {
}

OnlineStatusLib vescStatus(
	vescOfflineCallback, 
	vescOnlineCallback, 
	1 /*offlineNumConsecutiveTimesAllowance*/, 
	false /*debug*/);

/**************************************************************/

bool deviceConnected = false;

char* filename = "/data.txt";

WiFiManager wm;

//--------------------------------------------------------------------------------

void setup()
{
	// Serial.begin(9600);

  	vesc_comm_init(VESC_UART_BAUDRATE);

	setupWifiOTA();

	if ( connectedToWifi ) {
	}

	setupDisplay();

	if ( connectedToWifi ) {
		Blynk.begin(auth, ssid, pass);
	}

	bool vescOnline = getVescValues();

	runner.startNow();
	runner.addTask(tGetFromVESC);
	tGetFromVESC.enable();

	drawBatteryTopScreen(vescdata.batteryVoltage, connectedToWifi);

	storeUInt8(PREFS_POWERED_DOWN, 0);
}

//*************************************************************

long now = 0;

void loop() {

	if ( connectedToWifi ) {	
		ArduinoOTA.handle();
		Blynk.run();
	}

	wm.process();

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

    bool success = vesc_comm_fetch_packet(vesc_packet) > 0;

	if ( success ) {

		// // debugD("%d (rpm) %.1f (Ah) %u (tacho) %u (tachoabs)\n",
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
//--------------------------------------------------------------
void setupWifiOTA() {

    //reset settings - wipe credentials for testing
    //wm.resetSettings();
	wm.setConfigPortalBlocking(false);
    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name ( "Esk8MonitorConnectAP"),
    // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
    // then goes into a blocking loop awaiting configuration and will return success result
    bool res;

    // res = wm.autoConnect(); // auto generated AP name from chipid
    res = wm.autoConnect("Esk8MonitorConnectAP"); // anonymous ap
    // res = wm.autoConnect("Esk8MonitorConnectAP","password"); // password protected ap
    if ( res == false ) {
        // "Failed to connect"
		connectedToWifi = false;
    } 
    else {
        // connected
		connectedToWifi = true;

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
}
//--------------------------------------------------------------
char* get_reset_reason(RESET_REASON reason, int cpu)
{
	switch ( reason )
	{
		case 1 :  return "POWERON_RESET";          /**<1, Vbat power on reset*/
		case 3 :  return "SW_RESET";               /**<3, Software reset digital core*/
		case 4 :  return "OWDT_RESET";             /**<4, Legacy watch dog reset digital core*/
		case 5 :  return "DEEPSLEEP_RESET";        /**<5, Deep Sleep reset digital core*/
		case 6 :  return "SDIO_RESET";             /**<6, Reset by SLC module, reset digital core*/
		case 7 :  return "TG0WDT_SYS_RESET";       /**<7, Timer Group0 Watch dog reset digital core*/
		case 8 :  return "TG1WDT_SYS_RESET";       /**<8, Timer Group1 Watch dog reset digital core*/
		case 9 :  return "RTCWDT_SYS_RESET";       /**<9, RTC Watch dog Reset digital core*/
		case 10 : return "INTRUSION_RESET";       /**<10, Instrusion tested to reset CPU*/
		case 11 : return "TGWDT_CPU_RESET";       /**<11, Time Group reset CPU*/
		case 12 : return "SW_CPU_RESET";          /**<12, Software reset CPU*/
		case 13 : return "RTCWDT_CPU_RESET";      /**<13, RTC Watch dog Reset CPU*/
		case 14 : return "EXT_CPU_RESET";         /**<14, for APP CPU, reseted by PRO CPU*/
		case 15 : return "RTCWDT_BROWN_OUT_RESET";/**<15, Reset when the vdd voltage is not stable*/
		case 16 : return "RTCWDT_RTC_RESET";      /**<16, RTC Watch dog reset digital core and rtc module*/
		default : return "NO_MEAN";
	}
}


