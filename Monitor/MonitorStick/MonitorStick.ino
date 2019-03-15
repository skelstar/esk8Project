/*
Stick test
    hardware: M5Stack Stick
  
  please install the U8g2 library first ...
*/

#include <Arduino.h>
// #include <U8x8lib.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <Wire.h>
#include <myPushButton.h>

#define LedPin 19
#define IrPin 17
#define BuzzerPin 26
#define BtnPin 35

// U8X8_SH1107_64X128_4W_HW_SPI u8x8(14, /* dc=*/ 27, /* reset=*/ 33);
U8G2_SH1107_64X128_F_4W_HW_SPI u8g2(U8G2_R1, /* cs=*/ 14, /* dc=*/ 27, /* reset=*/ 33);


//https://github.com/olikraus/u8g2/wiki/fntgrpiconic#open_iconic_arrow_2x2

bool mpu9250_exis = false;
void mpu9250_read() {
    // uint8_t data = 0;
    // Wire.beginTransmission(0x68);         
    // Wire.write(0x75);                     
    // Wire.endTransmission(true);
    // Wire.requestFrom(0x68, 1);  
    // data = Wire.read();                   

    // Serial.print("mpu9250 addr: ");
    // Serial.println(data, HEX);
    // if(data == 0x71) {
    //     mpu9250_exis = true;
    // }
}


#define PULLUP		true
#define OFFSTATE	HIGH

void listener_Button(int eventCode, int eventPin, int eventParam);
myPushButton button(BtnPin, PULLUP, OFFSTATE, listener_Button);
void listener_Button(int eventCode, int eventPin, int eventParam) {
    
	switch (eventCode) {
		case button.EV_BUTTON_PRESSED:
			Serial.println("EV_BUTTON_PRESSED");
			break;
		case button.EV_RELEASED:
			Serial.println("EV_RELEASED");
			break;
		case button.EV_DOUBLETAP:
			Serial.println("EV_DOUBLETAP");
			break;
		case button.EV_HELD_SECONDS:
			Serial.println("EV_HELD_SECONDS");
			break;
    }
}

void setup() {
    // put your setup code here, to run once:
    Wire.begin(21, 22, 100000);
    // u8x8.begin();
    u8g2.begin();
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(0, 20, "test");

    Serial.begin(9600);
    
    setupPeripherals();
}

void loop()
{
    digitalWrite(LedPin, 1 - digitalRead(LedPin));
    ledcWrite(1, ledcRead(1) ? 0 : 512);
    delay(200);
    if(digitalRead(BtnPin) == 0){
        // lcdWriteLine(0, "hello", "world");
        u8g2.drawStr(0, 20, "test");
        u8g2.setFont(u8g2_font_open_iconic_arrow_2x_t);
        u8g2.drawGlyph(0, 40, 0x40);
        u8g2.setFont(u8g2_font_4x6_tr);

        u8g2.sendBuffer();
        buzzerBuzz();
    } else {
        u8g2.clearDisplay();
    }

    button.serviceEvents();
    // delay(200);
}

void lcdWriteLine(uint8_t line, char* title, char* value) {
    // u8x8.drawString(0, line, title);
    // u8x8.drawString(0, line+1, value);
}

void buzzerBuzz() {
    for(int i=0;i<100;i++){
        digitalWrite(BuzzerPin,HIGH);
        delay(1);
        digitalWrite(BuzzerPin,LOW);
        delay(1);
    }
}

void setupPeripherals() {
    pinMode(LedPin, OUTPUT);
    pinMode(IrPin, OUTPUT);
    pinMode(BuzzerPin, OUTPUT);
    // pinMode(BtnPin, INPUT_PULLUP);
    ledcSetup(1, 38000, 10);
    ledcAttachPin(IrPin, 1);
    digitalWrite(BuzzerPin, LOW);
    // u8x8.fillDisplay();
    // u8x8.setFont(u8x8_font_chroma48medium8_r);
    delay(1500);
    // u8x8.clearDisplay();
    mpu9250_read();
}
