#ifndef EncoderModuleLib_h
#define EncoderModuleLib_h

#include <Arduino.h>
#include "i2cEncoderLib.h"

class EncoderModuleLib 
{
	typedef void ( *EncoderChangedEventCallback )( int value );
	typedef void ( *EncoderPressedEventCallback )();
	typedef void ( *EncoderOnlineEventCallback )( bool online );
	typedef bool ( *EncoderCanAccelerateCallback )();

    private:
		// lower number = more coarse
		int _encoderCounterMinLimit = -20; 	// decceleration (ie -20 divides 0-127 into 20)
		int _encoderCounterMaxLimit = 15; 	// acceleration (ie 15 divides 127-255 into 15)
		int _oldCounter = 0;

		EncoderChangedEventCallback _encoderChangedEventCallback;
		EncoderPressedEventCallback _encoderPressedEventCallback;
		EncoderOnlineEventCallback _encoderOnlineEventCallback;
		EncoderCanAccelerateCallback _canAccelerateCallback;

		void handleCounterChanged(int newCounter);
		void setupEncoder(int maxCounts, int minCounts);

    public:
		// enum StateCode {
		// 	ST_NOT_HELD,
		// 	EV_BUTTON_PRESSED,
		// 	ST_WAIT_FOR_RELEASE,
		// 	EV_SPECFIC_TIME_REACHED,
		// 	EV_RELEASED,
		// 	EV_HELD_SECONDS,
		// 	EV_DOUBLETAP
		// };

        EncoderModuleLib(
        	EncoderChangedEventCallback encoderChangedEventCallback,
        	EncoderPressedEventCallback encoderPressedEventCallback,
			EncoderOnlineEventCallback encoderOnlineEventCallback,
			EncoderCanAccelerateCallback canAccelerateCallback,
			int minLimit, 
			int maxLimit);

        void update();
        bool encoderPressed();
};

#endif
