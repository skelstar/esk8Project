#ifndef HUDLib_h
#define HUDLib_h

#include <Arduino.h>



#define HUD_SSID "HUD_SSID"


class HUDLib 
{
	private:
		bool _debugOutput;

	public:

		enum LedState {
			Ok,
			Error,
			FlashingError
		};

		struct HudStruct {
			int id;
			LedState controllerLedState;
			LedState boardLedState;
			LedState vescLedState;
		};

		HUDLib(bool debug);

		HudStruct data;
};

#endif