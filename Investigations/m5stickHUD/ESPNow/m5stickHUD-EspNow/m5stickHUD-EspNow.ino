

// https://github.com/m5stack/m5-docs/blob/98ac9a89d6faa37e065daabf368988d8a7c54c49/docs/en/core/m5stick.md
// https://github.com/m5stack/M5Stack/blob/master/examples/Stick/FactoryTest/FactoryTest.ino

#include <Arduino.h>
#include <U8x8lib.h>
#include <SPI.h>
#include <Wire.h>

#include <HUDLibrary.h>

#include <Adafruit_NeoPixel.h>

#define LedPin 19
#define IrPin 17
#define BuzzerPin 26

#define YELLOW_PORT_PIN	13
#define WHITE_PORT_PIN 	25
#define BUTTON_PIN		35

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

#define CHANNEL 1

#define NUMPIXELS 2
Adafruit_NeoPixel pixels = Adafruit_NeoPixel( NUMPIXELS, /*pin*/ YELLOW_PORT_PIN, NEO_GRB + NEO_KHZ800);

const uint32_t COLOUR_LIGHT_GREEN = pixels.Color(0, 50, 0);
const uint32_t COLOUR_BRIGHT_RED = pixels.Color(255, 0, 0);



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

void setup() {
	Serial.begin(9600);
	Serial.println("ESPNow/Basic/Slave Example");

    pixels.begin();
	pixels.setPixelColor(0, COLOUR_BRIGHT_RED);
	pixels.setPixelColor(1, COLOUR_BRIGHT_RED);
	pixels.show();

	//Set device in AP mode to begin with
	WiFi.mode(WIFI_AP);
	// configure device AP mode
	configDeviceAP();
	// This is the mac address of the Slave in AP Mode
	Serial.printf("AP MAC: %s\n", WiFi.softAPmacAddress());
	// Init ESPNow with a fallback logic
	InitESPNow();

	esp_now_register_recv_cb(OnDataRecv);
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

	pixels.setPixelColor(0, mapStateToColour(hud.data.controllerLedState));
	pixels.setPixelColor(1, mapStateToColour(hud.data.boardLedState));
	// pixels.setPixelColor(0, mapStateToColour(hud.data.controllerLedState));
	pixels.show();
}

long now = 0;

void loop() {

	// if (millis() - now > 1000) {
	// 	now = millis();
	// 	// mpu9250_test();
	// }
}
//--------------------------------------------------
uint32_t mapStateToColour(byte state) {
	switch (state) {
		case 4:
			return pixels.Color(120, 0, 0);
			break;
		case 3:
			return pixels.Color(255, 0, 0);
			break;
		case 2:
			return pixels.Color(0, 100, 100);
			break;
		case 1:
			return pixels.Color(100, 50, 0);
			break;
		case 0:
			return pixels.Color(0, 0, 255);
			break;
	}
}


