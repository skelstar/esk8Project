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
            // spr->fillRect(cursor_x, y, pixelSize * 3, pixelSize * 5, bg_color);
            spr->fillRect(cursor_x, y + 4 * pixelSize, pixelSize, pixelSize, fg_color);
            cursor_x += pixelSize + spacing;
        } else if (ch == '-') {
            // spr->fillRect(cursor_x, y, 3 * pixelSize, 5 * pixelSize, bg_color);
            spr->fillRect(cursor_x + (pixelSize/4), y + 2 * pixelSize, (pixelSize*3)-((pixelSize/4)*2), pixelSize, fg_color);
            cursor_x += 3 * pixelSize + spacing;
        } else if (ch == ' ') {
            // spr->fillRect(cursor_x, y, 3 * pixelSize, 5 * pixelSize, bg_color);
            cursor_x += 3 * pixelSize + spacing;
        } else if (ch == '%') {
            tft_util_draw_digit(spr, 9 + 1 , cursor_x, y, fg_color, bg_color, pixelSize);
            cursor_x += 3 * pixelSize + spacing;
        }
    }
}

int getNumberWidth(int numDigits, int pixelSize, int spacing) {
    return numDigits * 5 * pixelSize;
}

#define WIDGET_SMALL    6
#define WIDGET_MEDIUM   12
#define WIDGET_POS_TOP_LEFT 1
#define WIDGET_POS_TOP_RIGHT 2
#define WIDGET_POS_MIDDLE 3
#define WIDGET_POS_BOTTOM_LEFT 4
#define WIDGET_POS_BOTTOM_RIGHT 5


void populateWidget(TFT_eSprite* spr, int pixelSize, char *number) {
	
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
    		x = 20;
    		y = 10;
			break;
    }

    tft_util_draw_number( spr, number, x, y, TFT_WHITE, TFT_BLACK, spacing, /*textSize = 1*/ pixelSize );

	spr->drawRect(0, 0, spr->width(), spr->height(), TFT_BLUE);
}