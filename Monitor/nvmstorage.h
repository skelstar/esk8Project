#ifndef Preferences_h
#include <Preferences.h>;
#endif

Preferences preferences;

void storeFloat(char* name, float value) {
	preferences.begin(DATA_NAMESPACE, false);	// r/w
	preferences.putFloat(name, value);
	preferences.end();
}

float recallFloat(char* name) {
	preferences.begin(DATA_NAMESPACE, false);	// r/w
	float result = preferences.getFloat(name, 0.0);
	preferences.end();
	return result;
}

void storeValuesOnPowerdown(VESC_DATA data) {
    float storedAmpHours = recallFloat(DATA_AMP_HOURS_USED_THIS_CHARGE);
    storeFloat(DATA_AMP_HOURS_USED_THIS_CHARGE, data.ampHours + storedAmpHours);
	debugD("storing values on power down %.1f\n", data.ampHours + storedAmpHours);
}
