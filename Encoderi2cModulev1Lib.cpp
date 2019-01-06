#include "Arduino.h"
#include "i2cEncoderLib.h"
#include "Encoderi2cModulev1Lib.h"

// https://www.tindie.com/products/Saimon/i2c-encoder-connect-rotary-encoders-on-i2c-bus/

i2cEncoderLib encoder(0x30); 

//--------------------------------------------------------------------------------
Encoderi2cModulev1Lib::Encoderi2cModulev1Lib(
        	EncoderChangedEventCallback encoderChangedEventCallback,
        	EncoderPressedEventCallback encoderPressedEventCallback,
        	GetEncoderCanAccelerateCallback getCanAccelerateCallback,
			int minLimit, 
			int maxLimit) {

	_encoderChangedEventCallback = encoderChangedEventCallback;
	_encoderPressedEventCallback = encoderPressedEventCallback;
	_getCanAccelerateCallback = getCanAccelerateCallback;

	_encoderCounterMinLimit = minLimit;
	_encoderCounterMaxLimit = maxLimit;

	setupEncoder(_encoderCounterMaxLimit, _encoderCounterMinLimit);
}

void Encoderi2cModulev1Lib::update() {

	bool newCanAccelerate = _getCanAccelerateCallback();

	bool canAccelerateStateChanged = _oldCanAccelerate != newCanAccelerate;
	_oldCanAccelerate = newCanAccelerate;

	bool encoderChanged = encoder.updateStatus();

	if (encoderChanged || canAccelerateStateChanged) {

		int newCounter = encoder.readCounterByte();
		delay(1);

		handleCounterChanged(newCounter, newCanAccelerate);
	
		if (encoder.readStatus(E_PUSH)) {
			_encoderPressedEventCallback();
		}
	
		if (encoder.readStatus(E_MAXVALUE)) {
		}
	
		if (encoder.readStatus(E_MINVALUE)) {
		}
	}
}

bool Encoderi2cModulev1Lib::isConnected() {
	return encoder.isConnected();
}

void Encoderi2cModulev1Lib::setEncoderCount(int count) {
	_oldCounter = 0;
	encoder.writeCounter(_oldCounter);
}

void Encoderi2cModulev1Lib::setEncoderMinMax(int minLimit, int maxLimit) {
	_encoderCounterMinLimit = minLimit;
	_encoderCounterMaxLimit = maxLimit;

	encoder.writeMax(_encoderCounterMinLimit); //Set maximum threshold
	encoder.writeMin(_encoderCounterMaxLimit); //Set minimum threshold
}

int Encoderi2cModulev1Lib::getEncoderMaxLimit() {
	return _encoderCounterMaxLimit;
}

int Encoderi2cModulev1Lib::getEncoderMinLimit() {
	return _encoderCounterMinLimit;
}

void Encoderi2cModulev1Lib::handleCounterChanged(int newCounter, bool canAccelerate) {

	if (newCounter > 0 && canAccelerate == false) {
		newCounter = 0;
		encoder.writeCounter(0);
	}

	if (_oldCounter != newCounter) {
		// counter has changed
		if (newCounter > 0) {
			if (canAccelerate) {
				_oldCounter = newCounter;

				int mappedReading = mapCounterTo127to255(_oldCounter);
				_encoderChangedEventCallback(mappedReading);
			}
			else {
				// can't go above 0
				encoder.writeCounter(0);
			}
		}
		else {
			_oldCounter = newCounter;
			int mappedReading = mapCounterTo127to255(_oldCounter);
			_encoderChangedEventCallback(mappedReading);
		}
		_oldCounter = newCounter;
	}
}

// map minLimit-0-maxlimit to 0-127-255
int Encoderi2cModulev1Lib::mapCounterTo127to255(int counter) {
	int rawMiddle = 0;

	if (counter >= rawMiddle) {
		return map(counter, rawMiddle, _encoderCounterMaxLimit, 127, 255);
	}
	return map(counter, _encoderCounterMinLimit, rawMiddle, 0, 127);
}

bool Encoderi2cModulev1Lib::encoderPressed() {
	return encoder.readStatus(E_PUSH);
}

void Encoderi2cModulev1Lib::setupEncoder(int maxCounts, int minCounts) {

	encoder.begin(( INTE_DISABLE | LEDE_DISABLE | WRAP_DISABLE | DIRE_RIGHT | IPUP_DISABLE | RMOD_X1 ));
	encoder.writeCounter(0);
	encoder.writeMax(maxCounts); //Set maximum threshold
	encoder.writeMin(minCounts); //Set minimum threshold
	encoder.writeLEDA(0x00);
	encoder.writeLEDB(0x00);
}
