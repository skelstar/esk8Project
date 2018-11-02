
#include<EncoderModuleLib.h>

EncoderModuleLib encoder();

void encoderChangedEvent(int encoderValue);
void encoderPressedEventCallback();
bool canAccelerateCallback();
void encoderOnlineEvent(bool online);

EncoderModuleLib encoderModule(
	&encoderChangedEvent, 
	&encoderPressedEventCallback,
	&encoderOnlineEvent,
	&canAccelerateCallback,
	 -20, 15);

void encoderChangedEvent(int encoderValue) {
	Serial.printf("encoderChangedEvent(%d); \n", encoderValue);
}

void encoderPressedEventCallback() {
	Serial.printf("encoderPressedEventCallback(); \n");
}

bool canAccelerateCallback() {
	return encoderModule.encoderPressed();
}

void encoderOnlineEvent(bool online) {
}

void setup() {

	Serial.begin(9600);
	Serial.println("Ready");

}

void loop() {

	encoderModule.update();

	delay(100);
}