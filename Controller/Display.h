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
        TFT_eSprite* tft, 
        char *number, 
        uint8_t x, 
        uint8_t y,
        uint16_t fg_color, 
        uint16_t bg_color, 
        uint8_t spacing, 
        uint8_t pixelSize = 1) {

    int cursor_x = x;
    int number_len = strlen(number);

    tft->fillRect(cursor_x, y, pixelSize * 3 * number_len + (spacing*number_len-1), pixelSize * 5, bg_color);

    for (int i=0; i < number_len; i++) {
        char ch = number[i];
        if (ch >= '0' and ch <= '9') {
            tft_util_draw_digit(tft, ch - '0', cursor_x, y, fg_color, bg_color, pixelSize);
            cursor_x += 3 * pixelSize + spacing;
        } else if (ch == '.') {
            // tft->fillRect(cursor_x, y, pixelSize * 3, pixelSize * 5, bg_color);
            tft->fillRect(cursor_x, y + 4 * pixelSize, pixelSize, pixelSize, fg_color);
            cursor_x += pixelSize + spacing;
        } else if (ch == '-') {
            // tft->fillRect(cursor_x, y, 3 * pixelSize, 5 * pixelSize, bg_color);
            tft->fillRect(cursor_x + (pixelSize/4), y + 2 * pixelSize, (pixelSize*3)-((pixelSize/4)*2), pixelSize, fg_color);
            cursor_x += 3 * pixelSize + spacing;
        } else if (ch == ' ') {
            // tft->fillRect(cursor_x, y, 3 * pixelSize, 5 * pixelSize, bg_color);
            cursor_x += 3 * pixelSize + spacing;
        } else if (ch == '%') {
            tft_util_draw_digit(tft, 9 + 1 , cursor_x, y, fg_color, bg_color, pixelSize);
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


void drawWidget(int widget, int pos, char *number) {
	int spacing = widget / 4;
    int x;
    int y;
    int width = strlen(number) * widget * 3 + (spacing*(strlen(number)-1));
    int height = 5 * widget;

    switch (pos) {
        case WIDGET_POS_TOP_LEFT:
			x = 0; y = 0;
			break;
        case WIDGET_POS_TOP_RIGHT:
			x = 320 - width; y = 0;
			break;
        case WIDGET_POS_MIDDLE:
			x = (320/2) - getNumberWidth(5, widget, 5)/2; 
			y = (240/2) - (height/2);
			break;
        case WIDGET_POS_BOTTOM_LEFT:
			x = 0; y = 240-(5*widget);
			break;
        case WIDGET_POS_BOTTOM_RIGHT:
			x = 320 - width; y = 240-(5*widget);
			break;
    }

    tft_util_draw_number( &img, number, x, y, TFT_WHITE, TFT_BLACK, spacing, /*textSize = 1*/ widget );
}