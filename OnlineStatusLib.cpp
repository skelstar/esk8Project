#include "Arduino.h"
#include "OnlineStatusLib.h"


OnlineStatusLib::OnlineStatusLib(StatusCallback isOfflineCb, StatusCallback isOnlineCb, bool debug) {
	_isOfflineCb = isOfflineCb;
	_isOnlineCb = isOnlineCb;
	_debugOutput = debug;
	state = ST_ONLINE;
}

bool OnlineStatusLib::serviceState(bool online) {
	switch (state) {
		case TN_ONLINE:
			if (online) {
				state = ST_ONLINE;
				debug( "TN_ONLINE > ST_ONLINE \n" );
				_isOnlineCb();
			}
			else {
				state = TN_OFFLINE;
				debug( "TN_ONLINE > TN_OFFLINE \n" );
			}
			break;
		case ST_ONLINE:
			if (online == false) {
				state = TN_OFFLINE;
				debug( "ST_ONLINE > TN_OFFLINE \n" );
			}
			break;
		case TN_OFFLINE:
			if (online) {
				state = TN_ONLINE;
				debug( "TN_OFFLINE > TN_ONLINE \n" );
			}
			else {
				state = ST_OFFLINE;
				debug( "TN_OFFLINE > ST_OFFLINE \n" );
				_isOfflineCb();
			}
			break;
		case ST_OFFLINE:
			if (online) {
				state = TN_ONLINE;
				debug( "ST_OFFLINE > TN_ONLINE \n" );
			}
			break;
		default:
			state = online ? TN_ONLINE : TN_OFFLINE;
			break;
	}
	bool stateChanged = oldstate != state;
	oldstate = state;
	return stateChanged;
}

void OnlineStatusLib::debug(char *output) {
	if ( _debugOutput ) {
		Serial.printf("%s", output);
	}
}

bool OnlineStatusLib::getState() { return state; }
bool OnlineStatusLib::isOnline() { return state == ST_ONLINE; }
