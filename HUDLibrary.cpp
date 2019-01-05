#include "Arduino.h"
#include "HUDLibrary.h"


HUDLib::HUDLib(	bool debug) {
	_debugOutput = debug;

	data.id = 0;
	data.controllerLedState = Ok;
	data.boardLedState = Ok;
	data.vescLedState = Ok;
}
