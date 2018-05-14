#include <WiiChuck.h>

Accessory nunchuck1;

#define POWER_WAKE_BUTTON_PIN	13

void setup() {
	Serial.begin(9600);
	nunchuck1.begin();
	if (nunchuck1.type == Unknown) {
		/** If the device isn't auto-detected, set the type explicatly
		 * 	NUNCHUCK,
		 WIICLASSIC,
		 GuitarHeroController,
		 GuitarHeroWorldTourDrums,
		 DrumController,
		 DrawsomeTablet,
		 Turntable
		 */
		nunchuck1.type = WIICLASSIC;

	}
	
	pinMode(POWER_WAKE_BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
	Serial.println("-------------------------------------------");


	nunchuck1.readData();    // Read inputs and update maps

	Serial.printf("interrupt? : %d \n", digitalRead( POWER_WAKE_BUTTON_PIN ));
	Serial.print(nunchuck1.getButtonX()?255:0); Serial.print(" | ");
	Serial.print(nunchuck1.getButtonY()?255:0); Serial.print(" | ");
	Serial.print(nunchuck1.getButtonA()?255:0); Serial.print(" | ");
	Serial.print(nunchuck1.getButtonB()?255:0); Serial.print(" | ");
	Serial.println();

	nunchuck1.printInputsClassic();
	// nunchuck1.printInputs(); // Print all inputs
	// for (int i = 0; i < WII_VALUES_ARRAY_SIZE; i++) {
	// 	Serial.println(
	// 			"Controller Val " + String(i) + " = "
	// 					+ String((uint8_t) nunchuck1.values[i]));
	// }

	delay(100);

}
