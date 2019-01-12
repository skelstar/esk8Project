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

    img_topLeft.setColorDepth(8); // Optionally set depth to 8 to halve RAM use
    img_topLeft.createSprite(SPRITE_SMALL_WIDTH, SPRITE_SMALL_HEIGHT);

    img_topRight.setColorDepth(8); // Optionally set depth to 8 to halve RAM use
    img_topRight.createSprite(SPRITE_SMALL_WIDTH, SPRITE_SMALL_HEIGHT);

    img_middle.setColorDepth(16); // Optionally set depth to 8 to halve RAM use
    img_middle.setFreeFont(FF21);                 // Select the font
    img_middle.createSprite(SPRITE_MED_WIDTH, SPRITE_MED_HEIGHT);

    img_bottomLeft.setColorDepth(8); // Optionally set depth to 8 to halve RAM use
    img_bottomLeft.createSprite(SPRITE_SMALL_WIDTH, SPRITE_SMALL_HEIGHT);

    img_bottomRight.setColorDepth(8); // Optionally set depth to 8 to halve RAM use
    img_bottomRight.createSprite(SPRITE_SMALL_WIDTH, SPRITE_SMALL_HEIGHT);

    debug.print(STARTUP, "setupDisplay();\n");
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

void populateSmallWidget(TFT_eSprite* spr, int pixelSize, char *number) {
	
	int spacing = pixelSize / 4;
    int x;
    int y;
    int width = strlen(number) * pixelSize * 3;	// + (spacing*(strlen(number)-1));
    int height = 5 * pixelSize;

    switch (pixelSize) {
    	case WIDGET_SMALL:
    		x = 5;
    		y = 5;
    		break;
		case WIDGET_MEDIUM:
    		x = 320/2 - width/2;
    		y = 10;
			break;
    }

    tft_util_draw_number( spr, number, x, y, TFT_WHITE, TFT_BLACK, spacing, /*textSize = 1*/ pixelSize );

	spr->drawRect(0, 0, spr->width(), spr->height(), TFT_BLUE);
}
//-----------------------------------------------------
void populateMediumWidget(TFT_eSprite* spr, int pixelSize, char *number, char *label, bool warning) {
	#define TFT_GREY 0xC618

	int spacing = 5;
    int width = strlen(number) * pixelSize * 3; // + paddingLeft;
    int height = 5 * pixelSize;

    int middleX = 320-100;
	int startX = middleX - width;// paddingLeft;
	int startY = 10;

	uint16_t bgColour = warning ? TFT_RED : TFT_BLACK;
    spr->fillRect(0, 0, spr->width(), spr->height(), bgColour);
    tft_util_draw_number( spr, number, startX, startY, TFT_WHITE, bgColour, spacing, pixelSize );

	int labelsX = middleX;
	spr->setTextColor(TFT_GREY, bgColour);
	spr->setTextDatum(BR_DATUM);
	spr->drawString(label, labelsX, spr->height() - 4);
	// spr->drawRect(0, 0, spr->width(), spr->height(), TFT_BLUE);
}//-----------------------------------------------------
//-----------------------------------------------------
void pushTextToMiddleOfSprite(TFT_eSprite *spr, char* text, int x, int y, uint16_t bgColour) {
    spr->fillRect(0, 0, spr->width(), spr->height(), bgColour);
    spr->setTextDatum(MC_DATUM);
    spr->drawString(text, spr->width()/2, spr->height()/2);
    spr->pushSprite(x, y);
}
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

	if ( esk8.controllerPacket.throttle == 127 ) {

		debug.print(DEBUG, "Updating display\n");

	    // commsStats
	    char value[5];  // xx.x\0

	    char topleft[] = "123";
	    char topRight[] = "456";
	    char bottomleft[] = "789";
	    char bottomright[] = "012";

	    if ( currentFailRatio == 1 ) {
	        pushTextToMiddleOfSprite(&img_middle, "BOARD OFFLINE!", /*x*/0, /*y*/(240/2) - (img_middle.height()/2), TFT_RED);
	        img_middle.pushSprite(0, (240/2) - (img_middle.height()/2));
	    }
	    else if ( false == esk8.boardPacket.vescOnline ) {
	        pushTextToMiddleOfSprite(&img_middle, "VESC OFFLINE!", /*x*/0, /*y*/(240/2) - (img_middle.height()/2), TFT_RED);
	        img_middle.pushSprite(0, (240/2) - (img_middle.height()/2));
	    }
	    else  { //if ( millis()/1000 % 2 == 0 ) {
	        bool warning = currentFailRatio > 0.1;
	        int decimalRatio = currentFailRatio*100;
	        sprintf( value, "%d", decimalRatio);
	        // Serial.printf("%s\n", stats);
	        populateMediumWidget( &img_middle, WIDGET_MEDIUM, value, "FAIL RATIO (%)", /*warning*/ warning);
	    }
	    img_middle.pushSprite(0, (240/2) - (img_middle.height()/2));
	}
}
