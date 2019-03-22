#include "Arduino.h"
#include <Rotary.h>
#include "EncoderBasicModule.h"

//--------------------------------------------------------------------------------
EncoderBasicModule::EncoderBasicModule() {}

void EncoderBasicModule::begin(
			int deadmanSwitchPin,
			Rotary rotary,
			EncoderChangedEventCallback encoderChangedEventCallback) {

	_deadmanSwitchPin = deadmanSwitchPin;

	_encoderCounterMin = -20; 	// decceleration (ie -20 divides 0-127 into 20)
	_encoderCounterMax = 15; 	// acceleration (ie 15 divides 127-255 into 15)
	_encoderCounter = 0;

	_encoderChangedEventCallback = encoderChangedEventCallback;

	_rotary = rotary;

	Serial.printf("EncoderBasicModule()\n");
	pinMode(deadmanSwitchPin, INPUT_PULLUP);
}
//--------------------------------------------------------------
void EncoderBasicModule::update() {

}
//--------------------------------------------------------------
int EncoderBasicModule::getEncoderCount() {
	return _encoderCounter;
}
//--------------------------------------------------------------
void EncoderBasicModule::setEncoderCount(int count) {
	// _oldCounter = 0;
	// encoder.writeCounter(_oldCounter);
}
//--------------------------------------------------------------
void EncoderBasicModule::encoderInterruptHandler() {

	unsigned char result = _rotary.process();

	bool canAccelerate = true;	// digitalRead(_deadmanSwitchPin) == 0;

	if (result == DIR_CW && (canAccelerate || _encoderCounter < 0)) {
		if (_encoderCounter < _encoderCounterMax) {
			_encoderCounter++;
			_encoderChangedEventCallback();
		}
	}
	else if (result == DIR_CCW) {
		if (_encoderCounter > _encoderCounterMin) {
			_encoderCounter--;
			_encoderChangedEventCallback();
		}
	}
}
//--------------------------------------------------------------

// void EncoderBasicModule::setEncoderMinMax(int minLimit, int maxLimit) {
// 	_encoderCounterMin = minLimit;
// 	_encoderCounterMax = maxLimit;

// 	encoder.writeMax(_encoderCounterMin); //Set maximum threshold
// 	encoder.writeMin(_encoderCounterMax); //Set minimum threshold
// }

// int EncoderBasicModule::getEncoderMaxLimit() {
// 	return _encoderCounterMax;
// }

// int EncoderBasicModule::getEncoderMinLimit() {
// 	return _encoderCounterMin;
// }

// void EncoderBasicModule::handleCounterChanged(int newCounter, bool canAccelerate) {

// 	if (newCounter > 0 && canAccelerate == false) {
// 		newCounter = 0;
// 		encoder.writeCounter(0);
// 	}

// 	if (_oldCounter != newCounter) {
// 		// counter has changed
// 		if (newCounter > 0) {
// 			if (canAccelerate) {
// 				_oldCounter = newCounter;

// 				int mappedReading = mapCounterTo127to255(_oldCounter);
// 				_encoderChangedEventCallback(mappedReading);
// 			}
// 			else {
// 				// can't go above 0
// 				encoder.writeCounter(0);
// 			}
// 		}
// 		else {
// 			_oldCounter = newCounter;
// 			int mappedReading = mapCounterTo127to255(_oldCounter);
// 			_encoderChangedEventCallback(mappedReading);
// 		}
// 		_oldCounter = newCounter;
// 	}
// }

// map minLimit-0-maxlimit to 0-127-255
int EncoderBasicModule::mapCounterTo127to255(int counter) {
	int rawMiddle = 0;

	if (counter >= rawMiddle) {
		return map(counter, rawMiddle, _encoderCounterMax, 127, 255);
	}
	return map(counter, _encoderCounterMin, rawMiddle, 0, 127);
}

// bool EncoderBasicModule::encoderPressed() {
// 	return encoder.readStatus(E_PUSH);
// }

// void EncoderBasicModule::setupEncoder(int maxCounts, int minCounts) {

// 	encoder.begin(( INTE_DISABLE | LEDE_DISABLE | WRAP_DISABLE | DIRE_RIGHT | IPUP_DISABLE | RMOD_X1 )); //INTE_ENABLE | LEDE_ENABLE | 
// 	encoder.writeCounter(0);
// 	encoder.writeMax(maxCounts); //Set maximum threshold
// 	encoder.writeMin(minCounts); //Set minimum threshold
// 	encoder.writeLEDA(0x00);
// 	encoder.writeLEDB(0x00);
// }
