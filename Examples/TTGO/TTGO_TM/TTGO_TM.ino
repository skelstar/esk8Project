
#include <Adafruit_GFX.h>                                  // Core graphics library
#include <Adafruit_ST7735.h>                               // Hardware-specific library
// Adafruit_ST7735 tft = Adafruit_ST7735(16, 17, 23, 5, 9); // CS,A0,SDA,SCK,RESET

#define TFT_CS 	5
#define TFT_A0 	16
#define TFT_DC 	16
#define TFT_RST 17

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST); 

#define	ST7735_BLACK   0x0000
#define	ST7735_BLUE    0x001F
#define	ST7735_RED     0xF800
#define	ST7735_GREEN   0x07E0
#define ST7735_CYAN    0x07FF
#define ST7735_MAGENTA 0xF81F
#define ST7735_YELLOW  0xFFE0
#define ST7735_WHITE   0xFFFF

int SCREEN_WIDTH = 240;
int SCREEN_HEIGHT = 320;
// Limited to 4 colors due to memory constraints
int BITS_PER_PIXEL = 2; // 2^2 =  4 colors

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  5        /* Time ESP32 will go to sleep (in seconds) */

// Pins for the ILI9341
// #define TFT_DC     4 //D2
// #define TFT_CS     5 //D1
// #define TFT_LED   15 //D8
// #define TFT_RST    2 //D8

void setup() {
	Serial.begin(9600);

	// The LED pin needs to set HIGH
	// Use this pin to save energy
	// Turn on the background LED

	// pinMode(button1Pin, INPUT);
	// pinMode(button2Pin, INPUT);
	// pinMode(button3Pin, INPUT);


	// tft.initR(tft.ST7735R, speed=10000000, spihost=tft.HSPI, mosi=23, miso=12, clk=18, cs=5, dc=16, rst_pin=17, hastouch=False, bgr=False, width=240, height=320);
	tft.initR();
	tft.fillScreen(ST7735_BLACK);
	// gfx.init();
	// gfx.setRotation(4);
	// gfx.fillBuffer(MINI_BLACK);
	// gfx.commit();
}

void loop() {

}
