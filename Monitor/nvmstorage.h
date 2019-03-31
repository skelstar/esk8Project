#ifndef Preferences_h
#include <Preferences.h>;
// https://github.com/espressif/arduino-esp32/blob/master/libraries/Preferences/src/Preferences.h
#endif

Preferences preferences;

void storeUInt8(char* name, uint8_t value) {
	preferences.begin(PREFS_NAMESPACE, false);	// r/w
	preferences.putUChar(name, value);
	preferences.end();
}

int recallUInt8(char* name)  {
	preferences.begin(PREFS_NAMESPACE, false);	// r/w
	float result = preferences.getUChar(name, 0.0);
	preferences.end();
	return result;
}

void storeFloat(char* name, float value) {
	preferences.begin(PREFS_NAMESPACE, false);	// r/w
	preferences.putFloat(name, value);
	preferences.end();
}

float recallFloat(char* name) {
	preferences.begin(PREFS_NAMESPACE, false);	// r/w
	float result = preferences.getFloat(name, 0.0);
	preferences.end();
	return result;
}

void storeValuesOnPowerdown(VESC_DATA data) {
    float storedAmpHours = recallFloat(PREFS_TOTAL_AMP_HOURS);
    storeFloat(PREFS_TOTAL_AMP_HOURS, data.ampHours + storedAmpHours);
	storeUInt8(PREFS_POWERED_DOWN, 1);	// true
	//debugD("storing values on power down %.1f\n", data.ampHours + storedAmpHours);
}
