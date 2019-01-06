

// https://github.com/m5stack/m5-docs/blob/98ac9a89d6faa37e065daabf368988d8a7c54c49/docs/en/core/m5stick.md
// https://github.com/m5stack/M5Stack/blob/master/examples/Stick/FactoryTest/FactoryTest.ino
// https://github.com/FastLED/FastLED/blob/master/examples/DemoReelESP32/DemoReelESP32.ino

#include <Arduino.h>
#include <U8x8lib.h>
#include <SPI.h>
#include <Wire.h>

#include <HUDLibrary.h>
#include <myPushButton.h>
#include <debugHelper.h>
#include <TaskScheduler.h>

#include "FastLED.h"

#define LedPin 19
#define IrPin 17
#define BuzzerPin 26

#define YELLOW_PORT_PIN	13
#define WHITE_PORT_PIN 	25
#define NEOPIXEL_PIN 	WHITE_PORT_PIN
#define BUTTON_PIN		35
#define LED_BUTTON		YELLOW_PORT_PIN

//---------------------------------------------------------------------


#define	STARTUP 		1 << 0
#define DEBUG 			1 << 1
#define COMMUNICATION 	1 << 2
#define HARDWARE		1 << 3
// #define ONLINE_STATUS	1 << 5
// #define TIMING			1 << 6

debugHelper debug;

#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define NUMPIXELS 1


CRGB pixels[NUMPIXELS];

CRGB currentColour = CRGB::Black;

#define BRIGHTNESS_DIM 5
#define BRIGHTNESS_MED 20
#define BRIGHTNESS_HIGH 255
int currentBrightness = BRIGHTNESS_DIM;

#define PIXEL_CONTROLLER	0
#define PIXEL_BOARD			6
#define PIXEL_VESC			5

// -- The core to run FastLED.show()
#define FASTLED_SHOW_CORE 0

// -- Task handles for use in the notifications
static TaskHandle_t FastLEDshowTaskHandle = 0;
static TaskHandle_t userTaskHandle = 0;

/** show() for ESP32
 *  Call this function instead of FastLED.show(). It signals core 0 to issue a show, 
 *  then waits for a notification that it is done.
 */
void FastLEDshowESP32()
{
    if (userTaskHandle == 0) {
        // -- Store the handle of the current task, so that the show task can notify it when it's done
        userTaskHandle = xTaskGetCurrentTaskHandle();

        // -- Trigger the show task
        xTaskNotifyGive(FastLEDshowTaskHandle);

        // -- Wait to be notified that it's done
        const TickType_t xMaxBlockTime = pdMS_TO_TICKS( 200 );
        ulTaskNotifyTake(pdTRUE, xMaxBlockTime);
        userTaskHandle = 0;
    }
}

//---------------------------------------------------------------------

HUDLib hud(/*debug*/ true);

U8X8_SH1107_64X128_4W_HW_SPI u8x8(14, /* dc=*/ 27, /* reset=*/ 33);

//--------------------------------------------------------------

#define PULLUP		true
#define OFFSTATE	LOW
void buttonCallback(int eventCode, int eventPin, int eventParam);
myPushButton button(LED_BUTTON, PULLUP, /* offstate*/ HIGH, buttonCallback);
void buttonCallback(int eventCode, int eventPin, int eventParam) {
    
	switch (eventCode) {
		case button.EV_RELEASED:
			switch (currentBrightness) {
				case BRIGHTNESS_DIM: currentBrightness = BRIGHTNESS_MED; break;
				case BRIGHTNESS_MED: currentBrightness = BRIGHTNESS_HIGH; break;
				case BRIGHTNESS_HIGH: currentBrightness = BRIGHTNESS_DIM; break;
			}
			FastLED.setBrightness(currentBrightness);
			FastLEDshowESP32();
			break;
		case button.EV_HELD_SECONDS:
			if (eventParam == 3) {
				debug.print(HARDWARE, "Powering down\n");
				powerDown();
			}
			break;
    }
}


/*************************************************************/


//--------------------------------------------------------------

Scheduler runner;

bool tFlashLed_onEnable();
void tFlashLed_onDisable();
void tFlashLedOn_callback();
void tFlashLedOff_callback();

Task tFlashLed(500, TASK_FOREVER, &tFlashLedOff_callback);

bool tFlashLed_onEnable() {
	pixels[0] = currentColour;
	FastLEDshowESP32();
	tFlashLed.enable();
    return true;
}
void tFlashLed_onDisable() {
	// currentBrightness
	// pixels[0] = currentColour;
	// FastLEDshowESP32();
	tFlashLed.disable();
}
void tFlashLedOn_callback() {
	tFlashLed.setCallback(&tFlashLedOff_callback);
	pixels[0] = currentColour;
	FastLEDshowESP32();
	debug.print(HARDWARE, "tFlashLedOn_callback\n");
	return;
}
void tFlashLedOff_callback() {
	tFlashLed.setCallback(&tFlashLedOn_callback);
	pixels[0] = CRGB::Black;
	FastLEDshowESP32();
	debug.print(HARDWARE, "tFlashLedOff_callback\n");
	return;
}



#include <esp_now.h>
#include <WiFi.h>

TaskHandle_t NeopixelsTask;

//--------------------------------------------------
const char compile_date[] = __DATE__ " " __TIME__;
const char file_name[] = __FILE__;
//--------------------------------------------------

void setup() {

	Serial.begin(9600);

	debug.init();
	debug.addOption(STARTUP, "STARTUP");
	debug.addOption(DEBUG, "DEBUG");
	debug.addOption(HARDWARE, "HARDWARE");
	debug.addOption(COMMUNICATION, "COMMUNICATION");
	debug.setFilter( STARTUP | DEBUG | COMMUNICATION | HARDWARE );	

    debug.print(STARTUP, "%s\n", file_name);
	debug.print(STARTUP, "%s\n", compile_date);

	setupDisplay();

	pinMode(BuzzerPin, OUTPUT);
	digitalWrite(BuzzerPin, LOW);

	FastLED.addLeds<LED_TYPE,NEOPIXEL_PIN,COLOR_ORDER>(pixels, NUMPIXELS).setCorrection(TypicalLEDStrip);
	FastLED.setBrightness(currentBrightness);

	setupEspNow();

	esp_now_register_recv_cb(OnDataRecv);

	runner.startNow();
	runner.addTask(tFlashLed);

	int core = xPortGetCoreID();
    debug.print(STARTUP, "Main code running on core %d, FastLED running on %d\n", core, FASTLED_SHOW_CORE);

    // -- Create the FastLED show task
    xTaskCreatePinnedToCore(
      /*function*/FastLEDshowTask, 
      /*name*/"FastLEDshowTask", 
      /*stack*/2048, 
      /*parameter*/NULL, 
      /*priority*/2, 
      /*handle*/&FastLEDshowTaskHandle, 
      /*core*/FASTLED_SHOW_CORE);
}
//--------------------------------------------------
//--------------------------------------------------
long now = 0;

void loop() {

	runner.execute();

	button.serviceEvents();

	vTaskDelay( 10 );
}

/* Show Task. This function runs on core 0 and just waits for requests to call FastLED.show() */
void FastLEDshowTask(void *pvParameters)
{
	// -- Run forever...
	for(;;) {
		// -- Wait for the trigger
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		// -- Do the show (synchronously)
		FastLED.show();
		// -- Notify the calling task
		xTaskNotifyGive(userTaskHandle);
	}
}
//--------------------------------------------------
//--------------------------------------------------
CRGB mapStateToColour(byte state) {
	switch (state) {
		case hud.FlashingError: return CRGB::Red;
		case hud.Error:			return CRGB::Blue;
		case hud.Ok:			return CRGB::Green;
	}
}

// callback when data is recv from Master
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
	char macStr[18];
	memcpy(&hud.data, data, sizeof(hud.data));

	debug.print(COMMUNICATION, "From Board: %d, %d, %d and %d\n", 
		hud.data.id, 
		hud.data.controllerLedState,
		hud.data.boardLedState,
		hud.data.vescLedState);

	if (hud.data.boardLedState == hud.Ok) {
		CRGB okColour = mapStateToColour( hud.data.boardLedState );
		solidColour( okColour );
	}
	else {
		flashColour( CRGB::Red, 100 );
	}
}
//--------------------------------------------------------------
void solidColour(CRGB colour) {
	tFlashLed.disable();
	currentColour = colour;
	pixels[PIXEL_CONTROLLER] = currentColour;
	FastLEDshowESP32();
}
//--------------------------------------------------------------
void flashColour(CRGB colour, long interval) {
	currentColour = colour;
	tFlashLed.setInterval(interval);
	tFlashLed.enable();
}
//--------------------------------------------------------------
void setupDisplay() {
    u8x8.begin();
    u8x8.setFont(u8x8_font_chroma48medium8_r);
    delay(1500);
    u8x8.clearDisplay();
	u8x8.drawString(0,0,"RUNNING");
}
//--------------------------------------------------------------
void InitESPNow() {
	WiFi.disconnect();
	if (esp_now_init() == ESP_OK) {
		Serial.println("ESPNow Init Success");
	}
	else {
		Serial.println("ESPNow Init Failed");
		ESP.restart();
	}
}
//--------------------------------------------------------------
void configDeviceAP() {
	#define CHANNEL 1

	String Prefix = "HUD_SSID";
	String Mac = WiFi.macAddress();
	String SSID = Prefix + Mac;
	String Password = "123456789";
	bool result = WiFi.softAP(SSID.c_str(), Password.c_str(), CHANNEL, 0);
	if (!result) {
		Serial.println("AP Config failed.");
	} else {
		Serial.println("AP Config Success. Broadcasting with AP: " + String(SSID));
	}
}
//--------------------------------------------------------------
void setupEspNow() {
	btStop();
	//Set device in AP mode to begin with
	WiFi.mode(WIFI_AP);
	// // configure device AP mode
	configDeviceAP();
	// // This is the mac address of the Slave in AP Mode
	// debug.print(DEBUG, "AP MAC: %s\n", WiFi.softAPmacAddress());
	// // Init ESPNow with a fallback logic
	InitESPNow();
}
//--------------------------------------------------------------
void powerDown() {
	delay(300);

    u8x8.clearDisplay();

	// leds
	for (int i=0; i<NUMPIXELS; i++) {
		pixels[i] = CRGB::Black;
	}
	FastLEDshowESP32();
	// wifi
	WiFi.mode(WIFI_OFF);
	btStop();

	// esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN , LOW);

	while(digitalRead(LED_BUTTON) == LOW) {
		delay(10);
	}
	esp_deep_sleep_start();}
//--------------------------------------------------------------
