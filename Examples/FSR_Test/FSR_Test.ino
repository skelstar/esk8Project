
#include <driver/adc.h>

int baseLine = 0;


void setup() {
	Serial.begin(9600);

	Serial.printf("\nReady.\n");
	pinMode(36, INPUT);
	adc1_config_width(ADC_WIDTH_12Bit);

	baseLine = getBaslinePressure();
}

void loop() {
	Serial.printf("%u - %d\n", adc1_get_voltage(ADC1_CHANNEL_0), map(adc1_get_voltage(ADC1_CHANNEL_0), baseLine, 4095, 0, 127));
	delay(200);
}

int getBaslinePressure() {
	return adc1_get_voltage(ADC1_CHANNEL_0);
}