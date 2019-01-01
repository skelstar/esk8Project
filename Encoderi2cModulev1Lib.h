#ifndef Encoderi2cModulev1Lib_h
#define Encoderi2cModulev1Lib_h

#include <Arduino.h>
#include <i2cEncoderLib.h>


class Encoderi2cModulev1Lib 
{
	typedef void ( *EncoderChangedEventCallback )( int value );
	typedef void ( *EncoderPressedEventCallback )();
    typedef bool ( *GetEncoderCanAccelerateCallback )();

    private:
		// lower number = more coarse
		int _encoderCounterMinLimit = -20; 	// decceleration (ie -20 divides 0-127 into 20)
		int _encoderCounterMaxLimit = 15; 	// acceleration (ie 15 divides 127-255 into 15)
		int _oldCounter = 0;
		int _oldCanAccelerate = false;

		EncoderChangedEventCallback _encoderChangedEventCallback;
		EncoderPressedEventCallback _encoderPressedEventCallback;
		GetEncoderCanAccelerateCallback _getCanAccelerateCallback;

		void handleCounterChanged(int newCounter, bool canAccelerate);
		void setupEncoder(int maxCounts, int minCounts);
		int mapCounterTo127to255(int counter);

    public:

        Encoderi2cModulev1Lib(
        	EncoderChangedEventCallback encoderChangedEventCallback,
        	EncoderPressedEventCallback encoderPressedEventCallback,
        	GetEncoderCanAccelerateCallback getCanAccelerateCallback,
			int minLimit, 
			int maxLimit);

        void update();
        bool isConnected();
        bool encoderPressed();
        void setEncoderCount(int count);
        void setEncoderMinMax(int minLimit, int maxLimit);
        int getEncoderMaxLimit();
        int getEncoderMinLimit();
};

#endif
