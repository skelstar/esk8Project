#ifndef EncoderBasicModule_h
#define EncoderBasicModule_h

#include <Arduino.h>
#include <Rotary.h>

class EncoderBasicModule 
{
	typedef void ( *EncoderChangedEventCallback )();
	typedef void ( *EncoderPressedEventCallback )();
	typedef void ( *EncoderOnlineEventCallback )( bool online );
	typedef bool ( *EncoderCanAccelerateCallback )();
	typedef void ( *AttachInterruptsTask )( int pinA, int pinB );

    public:

    	EncoderBasicModule();
		void begin(
			int deadmanSwitchPin,
			Rotary rotary,
			EncoderChangedEventCallback _encoderChangedEventCallback);
        void update();
        int getEncoderCount();
        void setEncoderCount(int count);
        void setEncoderMinMax(int minLimit, int maxLimit);
		void encoderInterruptHandler();
		// int getEncoderMaxLimit();
		// int getEncoderMinLimit();

    private:
		// lower number = more coarse
		int _encoderCounterMin; 	// decceleration (ie -20 divides 0-127 into 20)
		int _encoderCounterMax; 	// acceleration (ie 15 divides 127-255 into 15)
		int _deadmanSwitchPin;
		int _encoderCounter = 0;
		int _oldCounter = 0;
		int _oldCanAccelerate = false;

		Rotary _rotary = Rotary(0, 0);

		EncoderChangedEventCallback _encoderChangedEventCallback;
		EncoderPressedEventCallback _encoderPressedEventCallback;
		EncoderOnlineEventCallback _encoderOnlineEventCallback;
		EncoderCanAccelerateCallback _canAccelerateCallback;
		AttachInterruptsTask _attachInterruptsTask;

		// void handleCounterChanged(int newCounter, bool canAccelerate);
		int mapCounterTo127to255(int counter);
};

#endif
