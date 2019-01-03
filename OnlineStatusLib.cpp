#include "Arduino.h"
#include "OnlineStatusLib.h"


OnlineStatusLib::OnlineStatusLib(
		StatusCallback isOfflineCb, 
		StatusCallback isOnlineCb, 
		int offlineNumConsecutiveTimesAllowance,
		bool debug) {
	_isOfflineCb = isOfflineCb;
	_isOnlineCb = isOnlineCb;
	_debugOutput = debug;

	_offlineNumConsecutiveTimesAllowance = offlineNumConsecutiveTimesAllowance;
	state = ST_ONLINE;
}

bool OnlineStatusLib::serviceState(bool gotResponse) {
	switch (state) {
		case TN_ONLINE:
			if (gotResponse) {
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

			if ( shouldGoOffline(gotResponse) ) {
				state = TN_OFFLINE;
				debug( "ST_ONLINE > TN_OFFLINE \n" );
			}
			break;
		case TN_OFFLINE:
			if (gotResponse) {
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
			if (gotResponse) {
				state = TN_ONLINE;
				debug( "ST_OFFLINE > TN_ONLINE \n" );
			}
			break;
		default:
			state = gotResponse ? TN_ONLINE : TN_OFFLINE;
			break;
	}
	bool stateChanged = oldstate != state;
	oldstate = state;
	return stateChanged;
}
//-------------------------------------------------------
void OnlineStatusLib::debug(char *output) {
	if ( _debugOutput ) {
		Serial.printf("%s", output);
	}
}
//-------------------------------------------------------
bool OnlineStatusLib::getState() { return state; }
//-------------------------------------------------------
bool OnlineStatusLib::isOnline() { return state == ST_ONLINE; }
//-------------------------------------------------------
bool OnlineStatusLib::shouldGoOffline(bool gotResponse) {
	if ( gotResponse == false ) {
		_offlineConsecutiveTimesCount++;
		if ( _offlineConsecutiveTimesCount >= _offlineNumConsecutiveTimesAllowance) {
			_offlineConsecutiveTimesCount = 0;
			return true;	// should go offline
		}
		else {
			Serial.printf("Offline %d consecutive times\n", _offlineConsecutiveTimesCount);
			return false;	// not offline yet
		}
	}
	else {
		_offlineConsecutiveTimesCount = 0;
		return false;
	}
	return false;
}