

// https://github.com/m5stack/m5-docs/blob/98ac9a89d6faa37e065daabf368988d8a7c54c49/docs/en/core/m5stick.md
// https://github.com/m5stack/M5Stack/blob/master/examples/Stick/FactoryTest/FactoryTest.ino

#include <Arduino.h>
#include <U8x8lib.h>
#include <SPI.h>
#include <Wire.h>

#include <HUDLibrary.h>

#include "FastLED.h"

#define LedPin 19
#define IrPin 17
#define BuzzerPin 26

#define YELLOW_PORT_PIN	13
// #define WHITE_PORT_PIN 	25
#define NEOPIXEL_PIN 	25
#define BUTTON_PIN		35

//---------------------------------------------------------------------

#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define NUMPIXELS 8


CRGB pixels[NUMPIXELS];

#define BRIGHTNESS          60

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

bool mpu9250_exis = false;

void mpu9250_test() {
	uint8_t data = 0;
	Wire.beginTransmission(0x68);         
	Wire.write(0x75);                     
	Wire.endTransmission(true);
	Wire.requestFrom(0x68, 1);  
	if (Wire.available() == false) {
		Serial.printf("mpu9250 not available!\n");
	}
	else {
		data = Wire.read();                   

		Serial.print("mpu9250 addr: ");
		Serial.println(data, HEX);
		if (data == 0x71) {
			mpu9250_exis = true;
		}
	}
}
/*************************************************************/

#include <esp_now.h>
#include <WiFi.h>

// Adafruit_NeoPixel pixels = Adafruit_NeoPixel( NUMPIXELS, WHITE_PORT_PIN, NEO_GRB + NEO_KHZ800);

// const uint32_t COLOUR_OFF = pixels.Color(0, 0, 0);
// const uint32_t COLOUR_LIGHT_GREEN = pixels.Color(0, 10, 0);
// const uint32_t COLOUR_LIGHT_RED = pixels.Color(10, 0, 0);



// Init ESP Now with fallback
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

// config AP SSID
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

TaskHandle_t NeopixelsTask;

//--------------------------------------------------
//--------------------------------------------------
void setup() {

	Serial.begin(9600);
	Serial.println("ESPNow/Basic/Slave Example");

	FastLED.addLeds<LED_TYPE,NEOPIXEL_PIN,COLOR_ORDER>(pixels, NUMPIXELS).setCorrection(TypicalLEDStrip);

	//Set device in AP mode to begin with
	WiFi.mode(WIFI_AP);
	// // configure device AP mode
	configDeviceAP();
	// // This is the mac address of the Slave in AP Mode
	// Serial.printf("AP MAC: %s\n", WiFi.softAPmacAddress());
	// // Init ESPNow with a fallback logic
	// InitESPNow();

	// esp_now_register_recv_cb(OnDataRecv);


	int core = xPortGetCoreID();
    Serial.printf("Main code running on core %d, FastLED running on %d\n", core, FASTLED_SHOW_CORE);

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

	if (millis() - now > 1000) {
		now = millis();
		pixels[0] = mapStateToColour( now/1000 % 2 );
		pixels[1] = mapStateToColour( now/1000 % 3 );
		pixels[2] = mapStateToColour( now/1000 % 4 );
		FastLEDshowESP32();
	}

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
		case hud.FlashingError:
			return CRGB::Red;
			break;
		case hud.Error:
			return CRGB::Blue;
			break;
		case hud.Ok:
			return CRGB::White;
			break;
	}
}

// callback when data is recv from Master
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
	char macStr[18];
	memcpy(&hud.data, data, sizeof(hud.data));

	Serial.printf("From Board: %d, %d, %d and %d\n", 
		hud.data.id, 
		hud.data.controllerLedState,
		hud.data.boardLedState,
		hud.data.vescLedState);

	for (int i=0; i<NUMPIXELS; i++) {
		pixels[i] = CRGB::Black;
	}

	// pixels.setPixelColor(0, mapStateToColour(hud.data.boardLedState));
	// pixels.setPixelColor(1, mapStateToColour(hud.data.controllerLedState));
	// pixels.show();
}


