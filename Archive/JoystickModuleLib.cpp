#include "Arduino.h"
#include "JoystickModuleLib.h"

//--------------------------------------------------------------------------------
JoystickModuleLib::JoystickModuleLib(
        	JoystickChangedEventCallback joystickChangedEventCallback,
			JoystickCanAccelerateCallback canAccelerateCallback) {

	_joystickChangedEventCallback = joystickChangedEventCallback;
	_canAccelerateCallback = canAccelerateCallback;
}

void JoystickModuleLib::begin(Axis axisToUse) {
	_axisToUse = axisToUse;
}

void JoystickModuleLib::update() {

	bool newCanAccelerate = _canAccelerateCallback();

	bool canAccelerateStateChanged = _oldCanAccelerate != newCanAccelerate;
	_oldCanAccelerate = newCanAccelerate;

	bool joystickChanged = updateJoystick();

	if (joystickChanged || canAccelerateStateChanged) {
		delay(1);
		handleCounterChanged(_newJoyValue, newCanAccelerate);
	}
}

bool JoystickModuleLib::isConnected() {
	Wire.beginTransmission(JOY_ADDR);
	Wire.write(0x00);
	Wire.endTransmission();
	Wire.requestFrom(JOY_ADDR, 1);
	return Wire.available();
}

void JoystickModuleLib::handleCounterChanged(int newJoyValue, bool canAccelerate) {

	if (newJoyValue > 127 && canAccelerate == false) {
		_joyValue = 127;
		return;
	}

	if (_joyValue != newJoyValue) {
		// throttle has changed
		_joyValue = newJoyValue;
		_joystickChangedEventCallback(newJoyValue);
	}
}

bool JoystickModuleLib::updateJoystick() {
	uint8_t x_data;
	uint8_t y_data;
	uint8_t button_data;

	Wire.requestFrom(JOY_ADDR, 3);
	if (Wire.available()) {
		x_data = Wire.read();
		y_data = Wire.read();
		button_data = Wire.read();

		if (_axisToUse == X_AXIS) {
			_newJoyValue = x_data;
			return _newJoyValue == x_data;
		}
		else if (_axisToUse == Y_AXIS) {
			_newJoyValue = y_data;
			return _newJoyValue == y_data;
		}
		else {
			return false;
		}
	}
	return false;
}

bool JoystickModuleLib::joystickPressed() {
	// return encoder.readStatus(E_PUSH);
}
