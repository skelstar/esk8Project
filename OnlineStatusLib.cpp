#include "Arduino.h"
#include "OnlineStatusLib.h"


OnlineStatusLib::OnlineStatusLib(StatusCallback isOfflineCb, StatusCallback isOnlineCb) {
	_isOfflineCb = isOfflineCb;
	_isOnlineCb = isOnlineCb;
	state = ST_ONLINE;
}

bool OnlineStatusLib::serviceState(bool online) {
	switch (state) {
		case TN_ONLINE:
			if (online) {
				state = ST_ONLINE;
				Serial.printf("TN_ONLINE > ST_ONLINE \n");
				_isOnlineCb();
			}
			else {
				state = TN_OFFLINE;
				Serial.printf("TN_ONLINE > TN_OFFLINE \n");
			}
			break;
		case ST_ONLINE:
			if (online == false) {
				state = TN_OFFLINE;
				Serial.printf("ST_ONLINE > TN_OFFLINE \n");
			}
			break;
		case TN_OFFLINE:
			if (online) {
				state = TN_ONLINE;
				Serial.printf("TN_OFFLINE > TN_ONLINE \n");
			}
			else {
				state = ST_OFFLINE;
				Serial.printf("TN_OFFLINE > ST_OFFLINE \n");
				_isOfflineCb();
			}
			break;
		case ST_OFFLINE:
			if (online) {
				state = TN_ONLINE;
				Serial.printf("ST_OFFLINE > TN_ONLINE \n");
			}
			break;
		default:
			state = online ? TN_ONLINE : TN_OFFLINE;
			Serial.printf("DEFAULT state=%d \n", online);
			break;
	}
	bool stateChanged = oldstate != state;
	oldstate = state;
	return stateChanged;
}

bool OnlineStatusLib::getState() { return state; }
bool OnlineStatusLib::isOnline() { return state == ST_ONLINE; }
