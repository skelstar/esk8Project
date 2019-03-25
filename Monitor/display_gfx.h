#include <Adafruit_GFX.h>                                  // Core graphics library
#include <Adafruit_ST7735.h>       

// try: https://github.com/olikraus/ucglib/wiki/reference

#define TFT_CS 16
#define TFT_DC 17
#define TFT_MOSI 23
#define TFT_SCLK 5
#define TFT_RST 9

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST); 

/*
#define ST7735_BLACK      
#define ST7735_WHITE      
#define ST7735_RED        
#define ST7735_GREEN      
#define ST7735_BLUE       
#define ST7735_CYAN       
#define ST7735_MAGENTA    
#define ST7735_YELLOW     
#define ST7735_ORANGE
*/

void setupDisplay() {
	tft.initR(INITR_18GREENTAB);                             // 1.44 v2.1
	tft.fillScreen(ST7735_BLACK);                            // CLEAR
	tft.setRotation(3);                                      // 
    tft.setTextColor(ST7735_RED);
    tft.setCursor(0, 10);
    tft.print(WiFi.localIP().toString());
}                        // Hardware-specific library

#define BATTERY_WIDTH	100
#define BATTERY_HEIGHT	50
#define BORDER_SIZE 6
#define KNOB_HEIGHT 20

const bool FONT_DIGITS_3x5[12][5][3] = {
	{
			{1, 1, 1},
			{1, 0, 1},
			{1, 0, 1},
			{1, 0, 1},
			{1, 1, 1},
	},
	{
			{0, 0, 1},
			{0, 0, 1},
			{0, 0, 1},
			{0, 0, 1},
			{0, 0, 1},
	},
	{
			{1, 1, 1},
			{0, 0, 1},
			{1, 1, 1},
			{1, 0, 0},
			{1, 1, 1},
	},
	{
			{1, 1, 1},
			{0, 0, 1},
			{0, 1, 1},
			{0, 0, 1},
			{1, 1, 1},
	},
	{
			{1, 0, 1},
			{1, 0, 1},
			{1, 1, 1},
			{0, 0, 1},
			{0, 0, 1},
	},
	{
			{1, 1, 1},
			{1, 0, 0},
			{1, 1, 1},
			{0, 0, 1},
			{1, 1, 1},
	},
	{
			{1, 1, 1},
			{1, 0, 0},
			{1, 1, 1},
			{1, 0, 1},
			{1, 1, 1},
	},
	{
			{1, 1, 1},
			{0, 0, 1},
			{0, 0, 1},
			{0, 0, 1},
			{0, 0, 1},
	},
	{
			{1, 1, 1},
			{1, 0, 1},
			{1, 1, 1},
			{1, 0, 1},
			{1, 1, 1},
	},
	{
			{1, 1, 1},
			{1, 0, 1},
			{1, 1, 1},
			{0, 0, 1},
			{1, 1, 1},
	},
	// % = 10
	{	
			{1, 0, 1},
			{0, 0, 1},
			{0, 1, 0},
			{1, 0, 0},
			{1, 0, 1},
	},
	{	// v == 11
			{0, 0, 0},
			{0, 0, 0},
			{1, 0, 1},
			{1, 0, 1},
			{0, 1, 0},
	}        
};


//-----------------------------------------------------
void tft_util_draw_digit(
        Adafruit_ST7735 * tft, 
        uint8_t digit, 
        uint8_t x, 
        uint8_t y,
        uint16_t fg_color, 
        uint16_t bg_color, 
        uint8_t pixelSize = 1) {

    for (int xx = 0; xx < 3; xx++) {
        for (int yy = 0; yy < 5; yy++) {
            uint16_t color = FONT_DIGITS_3x5[digit][yy][xx] ? fg_color : bg_color;
            int x1 = x + xx * pixelSize;
            int y1 = y + yy * pixelSize;
            tft->fillRect(x1, y1, pixelSize, pixelSize, color);
        }
    }
}
//-----------------------------------------------------
void tft_util_draw_number(
        Adafruit_ST7735 * tft, 
        char *number, 
        uint8_t x, 
        uint8_t y,
        uint16_t fg_color, 
        uint16_t bg_color, 
        uint8_t spacing, 
        uint8_t pixelSize = 1) {

    int cursor_x = x;
    int number_len = strlen(number);

    tft->fillRect(
		cursor_x, /* x */
		y, /* y */
		pixelSize * 3 * number_len + (spacing*number_len-1), /* width */
		pixelSize * 5, /* height */
		bg_color /* colour */);

    for (int i=0; i < number_len; i++) {
        char ch = number[i];
        if (ch >= '0' and ch <= '9') {
            tft_util_draw_digit(tft, ch - '0', cursor_x, y, fg_color, bg_color, pixelSize);
            cursor_x += 3 * pixelSize + spacing;
        } else if (ch == '.') {
            tft->fillRect(cursor_x, y + 4 * pixelSize, pixelSize, pixelSize, fg_color);
            cursor_x += pixelSize + spacing;
        } else if (ch == '-') {
            tft->fillRect(cursor_x + (pixelSize/4), y + 2 * pixelSize, (pixelSize*3)-((pixelSize/4)*2), pixelSize, fg_color);
            cursor_x += 3 * pixelSize + spacing;
        } else if (ch == ' ') {
            cursor_x += 3 * pixelSize + spacing;
        } else if (ch == '%') {
            tft_util_draw_digit(tft, 9 + 1 , cursor_x, y, fg_color, bg_color, pixelSize);
            cursor_x += 3 * pixelSize + spacing;
        } else if (ch == 'v') {
            tft_util_draw_digit(tft, 11, cursor_x, y, fg_color, bg_color, pixelSize);
            cursor_x += 3 * pixelSize + spacing;
		}
    }
}
//-----------------------------------------------------
void drawBattery(int percent, float voltage) {
    // u8g2.clearBuffer();
	int middleOffset = 0;
    int outsideX = (128-(BATTERY_WIDTH+BORDER_SIZE))/2; // includes batt knob
    int outsideY = (128-BATTERY_HEIGHT)/2 + middleOffset;
	tft.fillScreen(ST7735_BLACK);	// clear screen
	tft.fillRect(
		outsideX, 
		outsideY, 
		BATTERY_WIDTH, 
		BATTERY_HEIGHT, 
		ST7735_WHITE
	);
    tft.fillRect(
        outsideX + BATTERY_WIDTH, 
        outsideY + (BATTERY_HEIGHT-KNOB_HEIGHT)/2, 
        BORDER_SIZE, 
        KNOB_HEIGHT,
		ST7735_WHITE
    ); // knob
    tft.fillRect(
		outsideX + BORDER_SIZE, 
		outsideY + BORDER_SIZE, 
		BATTERY_WIDTH - BORDER_SIZE*2, 
		BATTERY_HEIGHT - BORDER_SIZE*2,
		ST7735_BLACK
	);	// inside batt
    tft.fillRect(
        outsideX + BORDER_SIZE*2, 
        outsideY + BORDER_SIZE*2, 
        (BATTERY_WIDTH - BORDER_SIZE*4)*percent/100, 
        BATTERY_HEIGHT - BORDER_SIZE*4,
		ST7735_WHITE
    );	// capacity
    tft.setCursor(0, 100);
	tft.setTextSize(2);
	tft.setTextColor(ST7735_RED);
	//tft.print(voltage);

	char buff[8];
	char value[5];
	dtostrf(voltage, 4, 1, value);
	sprintf(buff, "%sv", value);
	int x = 128/2 - (4 * 3) + (2 * 4) + 3;
	tft_util_draw_number(&tft, buff, /*x*/x, /*y*/outsideY+BATTERY_HEIGHT + 2, ST7735_ORANGE, ST7735_BLACK, /*spacing*/2, /*pixel*/3);
}
