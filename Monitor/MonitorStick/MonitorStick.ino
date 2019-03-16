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
#include "BLEDevice.h"

// static BLEUUID serviceUUID("0000ffe0-0000-1000-8000-00805f9b34fb");
// static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
// static BLEUUID    CHARACTERISTIC_UUID("0000ffe1-0000-1000-8000-00805f9b34fb");
// static BLEUUID    CHARACTERISTIC_UUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"


static BLEAddress *pServerAddress;
static boolean doConnect = false;
static boolean connected = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;

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
            sendToMaster();
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

//--    

static void notifyCallback(
    BLERemoteCharacteristic* pBLERemoteCharacteristic,
    uint8_t* pData,
    size_t length,
    bool isNotify) {
    Serial.print("Notify! ");
    // Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    // Serial.println(length);
    Serial.print("data: ");
    Serial.println((char*)pData);
}

void setup() {
    // put your setup code here, to run once:
    Wire.begin(21, 22, 100000);
    // u8x8.begin();
    u8g2.begin();

    Serial.begin(9600);
    Serial.println("\nStarting Arduino BLE Client application...");

    u8g2.setFont(u8g2_font_4x6_tr);
    
    setupPeripherals();
    
    bleConnectToServer();
}

long now = 0;

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
    
    // If the flag "doConnect" is true then we have scanned for and found the desired
    // BLE Server with which we wish to connect.  Now we connect to it.  Once we are 
    // connected we set the connected flag to be true.
    // if (doConnect == true) {
    //     if (connectToServer(*pServerAddress)) {
    //         Serial.println("We are now connected to the BLE Server.");
    //         connected = true;
    //     } else {
    //         Serial.println("We have failed to connect to the server; there is nothin more we will do.");
    //     }
    //     doConnect = false;
    // }

    // // If we are connected to a peer BLE Server, update the characteristic each time we are reached
    // // with the current time since boot.
    // if (connected) {
    //     String newValue = "Time since boot: " + String(millis()/1000);
    //     Serial.println("Setting new characteristic value to \"" + newValue + "\"");
    // }

    // if (pRemoteCharacteristic->canRead()) {
    //     std::string value = pRemoteCharacteristic->readValue();
    //     Serial.print("The characteristic value was: ");
    //     Serial.println(value.c_str());

    //     // Set the characteristic's value to be the array of bytes that is actually a string.
    //     Serial.printf("sending to master\n");
    //     char buff[6];
    //     ltoa(millis(), buff, 10);
    //     now = millis();

    //     pRemoteCharacteristic->writeValue(buff, sizeof(buff));
    // }
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

void sendToMaster() {
    Serial.printf("sending to master\n");
    char buff[6];
    ltoa(millis(), buff, 10);
    pRemoteCharacteristic->writeValue(buff, sizeof(buff));
}

bool bleConnectToServer() {
    BLEDevice::init("");
    delay(1000);
    pServerAddress = new BLEAddress("80:7d:3a:c5:6a:36");
    delay(1000);
    BLEClient* pClient = BLEDevice::createClient();
    pClient->connect(*pServerAddress);
    Serial.println("Connected to server");

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
    // if (pRemoteService == nullptr) {
    //   Serial.print("Failed to find our service UUID: ");
    //   Serial.println(SERVICE_UUID);
    //   pClient->disconnect();
    //   return false;
    // }
    // Serial.println(" - Found our service");

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
    // if (pRemoteCharacteristic == nullptr) {
    //   Serial.print("Failed to find our characteristic UUID: ");
    //   Serial.println(CHARACTERISTIC_UUID);
    //   pClient->disconnect();
    //   return false;
    // }
    // Serial.println(" - Found our characteristic");
    if (pRemoteCharacteristic->canNotify()) {
        Serial.println("registering for notify");
        pRemoteCharacteristic->registerForNotify(notifyCallback);
    }

    // Read the value of the characteristic.
//     if (pRemoteCharacteristic->canRead()) {
//       std::string value = pRemoteCharacteristic->readValue();
//       Serial.print("The characteristic value was: ");
//       Serial.println(value.c_str());
//       pRemoteCharacteristic->writeValue("test");
// }
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
