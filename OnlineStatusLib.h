#ifndef OnlineStatusLib_h
#define OnlineStatusLib_h

#include <Arduino.h>

class OnlineStatusLib 
{
	typedef void ( *StatusCallback )();

	#define	TN_ONLINE 	1
	#define	ST_ONLINE 	2
	#define	TN_OFFLINE  3
	#define	ST_OFFLINE  4

	private:
		uint8_t state = ST_ONLINE;
		uint8_t oldstate = ST_ONLINE;

		StatusCallback _isOfflineCb;
		StatusCallback _isOnlineCb;

	public:

		OnlineStatusLib(StatusCallback isOfflineCb, StatusCallback isOnlineCb);

		bool serviceState(bool online);
		bool getState();
		bool isOnline();
};

#endif