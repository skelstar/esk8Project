#include "TFT_eSPI.h"
#include "Free_Fonts.h"

// https://github.com/Bodmer/TFT_eSPI

TFT_eSPI tft = TFT_eSPI();

TFT_eSprite img_topLeft = TFT_eSprite(&tft);
TFT_eSprite img_topRight = TFT_eSprite(&tft);
TFT_eSprite img_middle = TFT_eSprite(&tft);
TFT_eSprite img_bottomLeft = TFT_eSprite(&tft);
TFT_eSprite img_bottomRight = TFT_eSprite(&tft);

#define SPRITE_SMALL_WIDTH	320/2
#define SPRITE_SMALL_HEIGHT	240/4
#define SPRITE_MED_WIDTH	320
#define SPRITE_MED_HEIGHT	240/2

// TODO: PROGMEM
const bool FONT_DIGITS_3x5[11][5][3] = {
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
        }        
};

enum DisplayState {
    normal,
    vescoffline,
    boardoffline
} displayState = normal;

//--------------------------------------------------------------

void setupDisplay() {

    tft.begin();

    tft.setRotation(1); // 0 is portrait
    tft.fillScreen(TFT_BLACK);            // Clear screen

    // img_topLeft.setColorDepth(8); // Optionally set depth to 8 to halve RAM use
    // img_topLeft.createSprite(SPRITE_SMALL_WIDTH, SPRITE_SMALL_HEIGHT);

    // img_topRight.setColorDepth(8); // Optionally set depth to 8 to halve RAM use
    // img_topRight.createSprite(SPRITE_SMALL_WIDTH, SPRITE_SMALL_HEIGHT);

    // img_middle.setColorDepth(16); // Optionally set depth to 8 to halve RAM use
    // img_middle.setFreeFont(FF21);                 // Select the font
    // img_middle.createSprite(SPRITE_MED_WIDTH, SPRITE_MED_HEIGHT);

    // img_bottomLeft.setColorDepth(8); // Optionally set depth to 8 to halve RAM use
    // img_bottomLeft.createSprite(SPRITE_SMALL_WIDTH, SPRITE_SMALL_HEIGHT);

    // img_bottomRight.setColorDepth(8); // Optionally set depth to 8 to halve RAM use
    // img_bottomRight.createSprite(SPRITE_SMALL_WIDTH, SPRITE_SMALL_HEIGHT);

    // debug.print(STARTUP, "setupDisplay();\n");
}

//-----------------------------------------------------
void tft_util_draw_digit(
        TFT_eSprite * tft, 
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
        TFT_eSprite* spr, 
        char *number, 
        uint8_t x, 
        uint8_t y,
        uint16_t fg_color, 
        uint16_t bg_color, 
        uint8_t spacing, 
        uint8_t pixelSize = 1) {

    int cursor_x = x;
    int number_len = strlen(number);

    spr->fillRect(cursor_x, y, pixelSize * 3 * number_len + (spacing*number_len-1), pixelSize * 5, bg_color);

    for (int i=0; i < number_len; i++) {
        char ch = number[i];
        if (ch >= '0' and ch <= '9') {
            tft_util_draw_digit(spr, ch - '0', cursor_x, y, fg_color, bg_color, pixelSize);
            cursor_x += 3 * pixelSize + spacing;
        } else if (ch == '.') {
            spr->fillRect(cursor_x, y + 4 * pixelSize, pixelSize, pixelSize, fg_color);
            cursor_x += pixelSize + spacing;
        } else if (ch == '-') {
            spr->fillRect(cursor_x + (pixelSize/4), y + 2 * pixelSize, (pixelSize*3)-((pixelSize/4)*2), pixelSize, fg_color);
            cursor_x += 3 * pixelSize + spacing;
        } else if (ch == ' ') {
            cursor_x += 3 * pixelSize + spacing;
        } else if (ch == '%') {
            tft_util_draw_digit(spr, 9 + 1 , cursor_x, y, fg_color, bg_color, pixelSize);
            cursor_x += 3 * pixelSize + spacing;
        }
    }
}
//-----------------------------------------------------
#define WIDGET_SMALL    6
#define WIDGET_MEDIUM   16
#define WIDGET_POS_TOP_LEFT 1
#define WIDGET_POS_TOP_RIGHT 2
#define WIDGET_POS_MIDDLE 3
#define WIDGET_POS_BOTTOM_LEFT 4
#define WIDGET_POS_BOTTOM_RIGHT 5
//-----------------------------------------------------

// void renderSmallWidget(TFT_eSprite* spr, char *number, bool warning) {
	
// 	int spacing = WIDGET_SMALL / 4;
//     int x = 5;
//     int y = 5;
//     int width = strlen(number) * WIDGET_SMALL * 3;
//     int height = 5 * WIDGET_SMALL;
// 	uint16_t bgColour = warning ? TFT_RED : TFT_BLACK;

//     tft_util_draw_number( spr, number, x, y, TFT_WHITE, bgColour, spacing, WIDGET_SMALL );

// 	// spr->drawRect(0, 0, spr->width(), spr->height(), TFT_BLUE);
// }
// //-----------------------------------------------------
// void populateMediumWidget(TFT_eSprite* spr, char *number, char *label, bool warning) {
// 	#define TFT_GREY 0xC618

// 	int spacing = 5;
//     int width = strlen(number) * WIDGET_MEDIUM * 3; // + paddingLeft;
//     int height = 5 * WIDGET_MEDIUM;

//     int middleX = 320/2;
// 	int startX = middleX - width/2;// paddingLeft;
// 	int startY = 10;

// 	uint16_t bgColour = warning ? TFT_RED : TFT_BLACK;
//     spr->fillRect(0, 0, spr->width(), spr->height(), bgColour);
// 	tft_util_draw_number( spr, number, startX, startY, TFT_WHITE, bgColour, spacing, WIDGET_MEDIUM );

// 	int labelsX = middleX;
// 	spr->setTextColor(TFT_GREY, bgColour);
// 	spr->setTextDatum(BC_DATUM);
// 	spr->drawString(label, middleX, spr->height() - 4);
// 	// spr->drawRect(0, 0, spr->width(), spr->height(), TFT_BLUE);
// }//-----------------------------------------------------
// //-----------------------------------------------------
// void pushTextToMiddleOfSprite(TFT_eSprite *spr, char* text, int x, int y, uint16_t bgColour) {
//     spr->fillRect(0, 0, spr->width(), spr->height(), bgColour);
//     spr->setTextDatum(MC_DATUM);
//     spr->drawString(text, spr->width()/2, spr->height()/2);
//     spr->pushSprite(x, y);
// }
//--------------------------------------------------------------
#define LEFT            0
#define CENTRE          320/2
#define RIGHT           320
#define TOP             0
#define MIDDLE          240/2
#define BOTTOM          240
#define SECTOR_HEIGHT   240/4

// #define WIDGET_SMALL    6
// #define WIDGET_MEDIUM   12
// #define WIDGET_POS_TOP_LEFT 1
// #define WIDGET_POS_TOP_RIGHT 2
// #define WIDGET_POS_MIDDLE 3
// #define WIDGET_POS_BOTTOM_LEFT 4
// #define WIDGET_POS_BOTTOM_RIGHT 5

void updateDisplay() {

	// if ( esk8.controllerPacket.throttle == 127 ) {

	//     // commsStats
	//     char value[5];  // xx.x\0

	//     char topleft[] = "123";
	//     char topRight[] = "456";
	//     char bottomleft[] = "789";
	//     char bottomright[] = "012";

	// 	vTaskDelay( 1 );

	//     // if ( currentFailRatio == 1 ) {
	//     //     pushTextToMiddleOfSprite(&img_middle, "BOARD OFFLINE!", /*x*/0, /*y*/(240/2) - (img_middle.height()/2), TFT_RED);
	//     //     img_middle.pushSprite(0, (240/2) - (img_middle.height()/2));
	//     // }
	//     // else if ( false == esk8.boardPacket.vescOnline ) {
	//     //     pushTextToMiddleOfSprite(&img_middle, "VESC OFFLINE!", /*x*/0, /*y*/(240/2) - (img_middle.height()/2), TFT_RED);
	//     //     img_middle.pushSprite(0, (240/2) - (img_middle.height()/2));
	//     // }
	//     // else  {
	//     	// odometer
	//     	int32_t tripmeters = rotations_to_meters(esk8.boardPacket.odometer);
	// 		dtostrf(tripmeters/1000.0, 4, 1, value);
	//     	renderSmallWidget( &img_topLeft, value, /*warning*/ false);
	//     	img_topLeft.pushSprite(0, 0);

	//     	// battery voltage
	//     	bool warning = esk8.boardPacket.batteryVoltage < 34;
	// 		dtostrf(esk8.boardPacket.batteryVoltage, 4, 1, value);
	//     	populateMediumWidget( &img_middle, value, "BATTERY (v)", /*warning*/ warning);
	// 	    img_middle.pushSprite(0, (240/2) - (img_middle.height()/2));
	//     // }
	// }
}





#define BATTERY_WIDTH	100
#define BATTERY_HEIGHT	50
#define BORDER_SIZE 6
#define KNOB_HEIGHT 20

void drawBattery(int percent, float voltage) {
    // u8g2.clearBuffer();
    // int outsideX = (128-(BATTERY_WIDTH+BORDER_SIZE))/2; // includes batt knob
    // int outsideY = (128-BATTERY_HEIGHT)/2;
	// tft.fillScreen(ST7735_BLACK);	// clear screen
	// tft.fillRect(
	// 	outsideX, 
	// 	outsideY, 
	// 	BATTERY_WIDTH, 
	// 	BATTERY_HEIGHT, 
	// 	ST7735_WHITE
	// );
    // tft.fillRect(
    //     outsideX + BATTERY_WIDTH, 
    //     outsideY + (BATTERY_HEIGHT-KNOB_HEIGHT)/2, 
    //     BORDER_SIZE, 
    //     KNOB_HEIGHT,
	// 	ST7735_WHITE
    // ); // knob
    // tft.fillRect(
	// 	outsideX + BORDER_SIZE, 
	// 	outsideY + BORDER_SIZE, 
	// 	BATTERY_WIDTH - BORDER_SIZE*2, 
	// 	BATTERY_HEIGHT - BORDER_SIZE*2,
	// 	ST7735_BLACK
	// );	// inside batt
    // tft.fillRect(
    //     outsideX + BORDER_SIZE*2, 
    //     outsideY + BORDER_SIZE*2, 
    //     (BATTERY_WIDTH - BORDER_SIZE*4)*percent/100, 
    //     BATTERY_HEIGHT - BORDER_SIZE*4,
	// 	ST7735_WHITE
    // );	// capacity
    // tft.setCursor(0, 100);
	// tft.setTextSize(2);
	// tft.setTextColor(ST7735_RED);
	// tft.print(voltage);
}