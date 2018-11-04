#include "Arduino.h"
#include "EncoderModuleLib.h"


i2cEncoderLib encoder(0x30); 

//--------------------------------------------------------------------------------
EncoderModuleLib::EncoderModuleLib(
        	EncoderChangedEventCallback encoderChangedEventCallback,
        	EncoderPressedEventCallback encoderPressedEventCallback,
			EncoderOnlineEventCallback encoderOnlineEventCallback,
			EncoderCanAccelerateCallback canAccelerateCallback,
			int minLimit, 
			int maxLimit) {

	_encoderChangedEventCallback = encoderChangedEventCallback;
	_encoderPressedEventCallback = encoderPressedEventCallback;
	_encoderOnlineEventCallback = encoderOnlineEventCallback;
	_canAccelerateCallback = canAccelerateCallback;

	_encoderCounterMinLimit = minLimit;
	_encoderCounterMaxLimit = maxLimit;

	setupEncoder(_encoderCounterMaxLimit, _encoderCounterMinLimit);
}

void EncoderModuleLib::update() {

	if (encoder.updateStatus()) {

		int newCounter = encoder.readCounterByte();
		delay(1);

		handleCounterChanged(newCounter);
	
		if (encoder.readStatus(E_PUSH)) {
			_encoderPressedEventCallback();
		}
	
		if (encoder.readStatus(E_MAXVALUE)) {
		}
	
		if (encoder.readStatus(E_MINVALUE)) {
		}
	}
}

void EncoderModuleLib::setEncoderCount(int count) {
	encoder.writeCounter(count);
}

void EncoderModuleLib::setEncoderMinMax(int minLimit, int maxLimit) {
	_encoderCounterMinLimit = minLimit;
	_encoderCounterMaxLimit = maxLimit;

	encoder.writeMax(_encoderCounterMinLimit); //Set maximum threshold
	encoder.writeMin(_encoderCounterMaxLimit); //Set minimum threshold
}

int EncoderModuleLib::getEncoderMaxLimit() {
	return _encoderCounterMaxLimit;
}

int EncoderModuleLib::getEncoderMinLimit() {
	return _encoderCounterMinLimit;
}

void EncoderModuleLib::handleCounterChanged(int newCounter) {

	if (_oldCounter != newCounter) {
		// counter has changed
		if (newCounter > 0) {
			if (_canAccelerateCallback()) {
				_oldCounter = newCounter;
				_encoderChangedEventCallback(newCounter);
			}
			else {
				// can't go above 0
				encoder.writeCounter(0);
			}

		}
		else {
			_oldCounter = newCounter;
			_encoderChangedEventCallback(newCounter);
		}
		_oldCounter = newCounter;
	}
}

bool EncoderModuleLib::encoderPressed() {
	return encoder.readStatus(E_PUSH);
}

void EncoderModuleLib::setupEncoder(int maxCounts, int minCounts) {

	encoder.begin(( INTE_DISABLE | LEDE_DISABLE | WRAP_DISABLE | DIRE_RIGHT | IPUP_DISABLE | RMOD_X1 )); //INTE_ENABLE | LEDE_ENABLE | 
	encoder.writeCounter(0);
	encoder.writeMax(maxCounts); //Set maximum threshold
	encoder.writeMin(minCounts); //Set minimum threshold
	encoder.writeLEDA(0x00);
	encoder.writeLEDB(0x00);
}
