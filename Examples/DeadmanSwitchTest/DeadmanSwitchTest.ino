
#define DEADMAN_SWITCH		2


void setup() {

	Serial.begin(9600);

	pinMode(DEADMAN_SWITCH, INPUT_PULLUP);
}

void loop() {

	int result = digitalRead(DEADMAN_SWITCH);
	Serial.printf("Switch: %d \n", result);

	delay(500);
}