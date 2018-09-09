
#include <driver/adc.h>

int baseLine = 0;


/*
	3v3
	 |
	 |
	---
	
	10k

	---
	 |
	 |
	io36

FSR goes across io36 and GND 
*/


void setup() {
	Serial.begin(9600);

	Serial.printf("\nReady.\n");
	pinMode(36, INPUT);
	adc1_config_width(ADC_WIDTH_12Bit);

	baseLine = getBaslinePressure();
}

void loop() {

	int mappedRaw = mapToCurve(adc1_get_voltage(ADC1_CHANNEL_0));
	Serial.printf("%u - %d - %d\n", 
		adc1_get_voltage(ADC1_CHANNEL_0), 
		mappedRaw,
		map(adc1_get_voltage(ADC1_CHANNEL_0), baseLine, 4095, 0, 127));


	delay(200);
}

// http://forum.arduino.cc/index.php?topic=145443.0
int mapToCurve(unsigned long raw) {
	unsigned long reading = raw;
	reading = reading * reading; // 4096*4096 = 16,777,216
	reading = reading / (16777216/127);
	return reading;
}

int getBaslinePressure() {
	return adc1_get_voltage(ADC1_CHANNEL_0);
}