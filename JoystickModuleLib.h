#ifndef JoystickModuleLib_h
#define JoystickModuleLib_h

#include <Arduino.h>
#include <Wire.h>

class JoystickModuleLib 
{
	typedef void ( *JoystickChangedEventCallback )( int value );
	typedef bool ( *JoystickCanAccelerateCallback )();

    private:
		// lower number = more coarse
		// int _joystickRawMinLimit = -20; 	// decceleration (ie -20 divides 0-127 into 20)
		// int _joystickRawMaxLimit = 15; 	// acceleration (ie 15 divides 127-255 into 15)
		int _joyValue = 127;
		int _newJoyValue = 127;
		int _oldCanAccelerate = false;
	
		#define JOY_ADDR 0x52

		JoystickChangedEventCallback _joystickChangedEventCallback;
		JoystickCanAccelerateCallback _canAccelerateCallback;

		void handleCounterChanged(int newCounter, bool canAccelerate);
		bool updateJoystick();

    public:
		enum Axis {
			X_AXIS,
			Y_AXIS
		};

		Axis _axisToUse;

        JoystickModuleLib(
        	JoystickChangedEventCallback JoystickChangedEventCallback,
			JoystickCanAccelerateCallback canAccelerateCallback);
        void begin(Axis axisToUse);
        void update();
        bool isConnected();
        bool joystickPressed();
};

#endif
