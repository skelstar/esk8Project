#include <SPI.h>
#include <RF24.h> 
#include "WiFi.h"

#include <TaskScheduler.h>

#include <Wire.h>

#include <myPushButton.h>
#include <debugHelper.h>
#include <esk8Lib.h>

#include "i2cEncoderLib.h"

// #include <Adafruit_NeoPixel.h>
// #include <M5Stack.h>
#include "TFT_eSPI.h"
#include "Free_Fonts.h" 

//--------------------------------------------------------------------------------

#define	PIXEL_PIN			16	// was 5

#define M5_BUTTON_A			39
#define M5_BUTTON_B			38
#define M5_BUTTON_C			37

//--------------------------------------------------------------

#define	STARTUP 		1 << 0
#define DEBUG 			1 << 1
#define COMMUNICATION 	1 << 2
#define HARDWARE		1 << 3
#define ONLINE_STATUS	1 << 5
#define TIMING			1 << 6

debugHelper debug;

esk8Lib esk8;


#define IWIDTH	100
#define IHEIGHT	100

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite img = TFT_eSprite(&tft);

//--------------------------------------------------------------

void setupRadio();

//--------------------------------------------------------------------------------

int32_t encoderCounter;
int32_t oldEncoderCounter = 0;
uint8_t encoder_status;
// stats
int32_t packetsSent = 0;
int32_t packetsFailed = 0;

//--------------------------------------------------------------

volatile long lastRxFromBoard = 0;
volatile bool rxDataFromBoard = false;
portMUX_TYPE mmux = portMUX_INITIALIZER_UNLOCKED;

//--------------------------------------------------------------

#define SPI_CE        5 	//33    // white/purple
#define SPI_CS        13	//26  // green

#define ROLE_CONTROLLER    0

int role = ROLE_CONTROLLER;
bool radioNumber = 1;

RF24 radio(SPI_CE, SPI_CS);    // ce pin, cs pin

int offlineCount = 0;
int encoderOfflineCount = 0;
bool encoderOnline = true;

i2cEncoderLib encoder(0x30); 

//--------------------------------------------------------------------------------

const char compile_date[] = __DATE__ " " __TIME__;

int mapEncoderToThrottleValue(int raw);

//--------------------------------------------------------------

bool throttleChanged = false;
int throttle = 127;

#define SEND_TO_BOARD_INTERVAL_MS 			200

//--------------------------------------------------------------

class EncoderModule 
{
	//#include "i2cEncoderLib.h"

	#define TRINKET_MODULE_ADDR		0x4
	// #define ENCODER_MODULE_ADDR		0x30

	#define ENCODER_MODULE_CMD_SET_PIXEL		1
	#define ENCODER_MODULE_CMD_SET_LIMITS		2
	#define ENCODER_MODULE_CMD_SET_BRIGHTNESS	3

	#define ENCODER_MODULE_LED_COLOUR_BLACK	0
	#define ENCODER_MODULE_LED_COLOUR_RED	1
	#define ENCODER_MODULE_LED_COLOUR_BLUE	2
	#define ENCODER_MODULE_LED_COLOUR_GREEN	3

	typedef void ( *EncoderChangedEventCallback )( int value );
	typedef void ( *EncoderOnlineEventCallback )( bool online );

	//i2cEncoderLib encoder(0x30); 

	private:
		int encoderCounterState = 0;
		bool encoderOnline = true;
		int deadmanSwitch = 0;
		i2cEncoderLib *_encoder;

		EncoderChangedEventCallback _encoderChangedEventCallback;
		EncoderOnlineEventCallback _encoderOnlineEventCallback;

	public:

		// lower number = more coarse
		int encoderCounterMinLimit = -20; 	// decceleration (ie -20 divides 0-127 into 20)
		int encoderCounterMaxLimit = 15; 	// acceleration (ie 15 divides 127-255 into 15)

		EncoderModule(EncoderChangedEventCallback encoderChangedEventCallback,
			EncoderOnlineEventCallback encoderOnlineEventCallback,
			int minLimit, int maxLimit, i2cEncoderLib *encoder) 
		{
			_encoderChangedEventCallback = encoderChangedEventCallback;
			_encoderOnlineEventCallback = encoderOnlineEventCallback;
			encoderCounterMinLimit = minLimit;
			encoderCounterMaxLimit = maxLimit;

			_encoder = encoder;

			setupEncoder(encoderCounterMaxLimit, encoderCounterMinLimit);
		}

		int setPixel(byte encoderLedColour) {
			Wire.beginTransmission(TRINKET_MODULE_ADDR);
			Wire.write(ENCODER_MODULE_CMD_SET_PIXEL);
			Wire.write(encoderLedColour);
			return Wire.endTransmission();
		}

		int setPixelBrightness(byte brightness) {
			Wire.beginTransmission(TRINKET_MODULE_ADDR);
			Wire.write(ENCODER_MODULE_CMD_SET_BRIGHTNESS);
			Wire.write(brightness);
			return Wire.endTransmission();		
		}

		int setEncoderLimits(int min, int max) {
			encoderCounterMinLimit = min;
			encoderCounterMaxLimit = max;
			Serial.printf("min: %d (%d) max: %d \n", (byte)min, 0 - (255-(byte)min), (byte)max);
			// Wire.beginTransmission(TRINKET_MODULE_ADDR);
			// Wire.write(ENCODER_MODULE_CMD_SET_LIMITS);
			// Wire.write((byte)min);
			// Wire.write((byte)max);
			// return Wire.endTransmission();		
		}

		void update() {

			if (_encoder->updateStatus()) {
				if (_encoder->readStatus(E_PUSH)) {
					Serial.println("Encoder Pushed!");
				}
				if (_encoder->readStatus(E_MAXVALUE)) {
					Serial.println("Encoder Max!");
					_encoder->writeLEDA(0xFF);
					delay(50);
					// _encoder->writeLEDA(0x00);
				}
				if (_encoder->readStatus(E_MINVALUE)) {
					Serial.println("Encoder Min!");
					_encoder->writeLEDB(0xFF);
					delay(50);
					// _encoder->writeLEDB(0x00);
				}
				Serial.printf("Encoder: %d \n", _encoder->readCounterByte());			
			}

			Wire.requestFrom(TRINKET_MODULE_ADDR, 1);

			if (Wire.available()) {
				deadmanSwitch = Wire.read();


				// int _encoderCounter = Wire.read();
				// if (encoderCounterState != _encoderCounter) {
				// 	encoderCounterState = _encoderCounter;
				// 	_encoderChangedEventCallback(_encoderCounter);
				// }
				// if (encoderOnline == false) {
				// 	encoderOnline = true;
				// 	_encoderOnlineEventCallback(true);
				// }
			}
			else {
				if (encoderOnline == true) {
					encoderOnline = false;
					_encoderOnlineEventCallback(false);	
				}
			}
		}

		bool deadmanIsPressed() {
			return deadmanSwitch == 1;
		}

		//----------------------------------------------------------------
		void setupEncoder(int maxCounts, int minCounts) {

			_encoder->begin(( INTE_DISABLE | LEDE_DISABLE | WRAP_DISABLE | DIRE_RIGHT | IPUP_DISABLE | RMOD_X1 )); //INTE_ENABLE | LEDE_ENABLE | 
			_encoder->writeCounter(0);
			_encoder->writeMax(maxCounts); //Set maximum threshold
			_encoder->writeMin(minCounts); //Set minimum threshold
			_encoder->writeLEDA(0x00);
			_encoder->writeLEDB(0x00);
		}
};

void encoderChangedEvent(int encoderValue) {
	debug.print(HARDWARE, "encoderChangedEvent(%d); \n", encoderValue);
	oldEncoderCounter = encoderCounter;

	encoderCounter = encoderValue > 127
		 ? -256 + encoderValue 	// ie -254 = -2
		 : encoderValue;
	setControllerPacketThrottle();
	updateDisplay(/* mode */ 1, /* backlight */ esk8.controllerPacket.throttle <= 127 );
}

void encoderOnlineEvent(bool online) {
	encoderOnline = online;
	updateDisplay(/* mode */ 1, /* backlight */ 1);
}

EncoderModule encoderModule(&encoderChangedEvent, &encoderOnlineEvent, -20, 15, &encoder);

//--------------------------------------------------------------
void m5ButtonACallback(int eventCode, int eventPin, int eventParam);

#define PULLUP		true
#define OFFSTATE	LOW

myPushButton m5ButtonA(M5_BUTTON_A, PULLUP, /* offstate*/ HIGH, m5ButtonACallback);

void m5ButtonACallback(int eventCode, int eventPin, int eventParam) {
    
	switch (eventCode) {
		case m5ButtonA.EV_BUTTON_PRESSED:
			break;
		// case m5ButtonA.EV_RELEASED:
		// 	break;
		// case m5ButtonA.EV_DOUBLETAP:
		// 	break;
		// case m5ButtonA.EV_HELD_SECONDS:
		// 	break;
    }
}

//--------------------------------------------------------------

Scheduler runner;

bool tFlashLeds_onEnable();
void tFlashLeds_onDisable();
void tFlashLedsOn_callback();
void tFlashLedsOff_callback();

Task tFlashLeds(500, TASK_FOREVER, &tFlashLedsOff_callback);

bool tFlashLeds_onEnable() {
	encoderModule.setPixel(ENCODER_MODULE_LED_COLOUR_RED);
	tFlashLeds.enable();
    return true;
}
void tFlashLeds_onDisable() {
	encoderModule.setPixel(ENCODER_MODULE_LED_COLOUR_BLACK);
	tFlashLeds.disable();
}
void tFlashLedsOn_callback() {
	tFlashLeds.setCallback(&tFlashLedsOff_callback);
	encoderModule.setPixel(ENCODER_MODULE_LED_COLOUR_RED);
	debug.print(HARDWARE, "tFlashLedsOn_callback\n");
	return;
}
void tFlashLedsOff_callback() {
	tFlashLeds.setCallback(&tFlashLedsOn_callback);
	encoderModule.setPixel(ENCODER_MODULE_LED_COLOUR_BLACK);
	debug.print(HARDWARE, "tFlashLedsOff_callback\n");
	return;
}

//--------------------------------------------------------------
void sendMessage() {

	taskENTER_CRITICAL(&mmux);
	bool sentOK = sendToBoard();
    taskEXIT_CRITICAL(&mmux);

	// packetsSent++;
    if ( sentOK ) {
		throttleChanged = false;
		lastRxFromBoard = millis();
    }
    else {
    	// packetsFailed++;
        debug.print(COMMUNICATION, "tSendControllerValues_callback(): ERR_NOT_SEND_OK \n");
    }
}

void tSendControllerValues_callback() {
    sendMessage();
}
Task tSendControllerValues(SEND_TO_BOARD_INTERVAL_MS, TASK_FOREVER, &tSendControllerValues_callback);

//--------------------------------------------------------------
void boardOfflineCallback() {
	debug.print(ONLINE_STATUS, "offlineCallback();\n");
	tFlashLeds.enable();
	offlineCount++;
	updateDisplay(/* mode */ 1, /* backlight */ 1);
	// M5.Speaker.tone(330, 100);	// tone 330, 200ms
}

void boardOnlineCallback() {
	debug.print(ONLINE_STATUS, "onlineCallback();\n");	
	tFlashLeds.disable();
	updateDisplay(/* mode */ 1, /* backlight */ 1);
}
//--------------------------------------------------------------
#define	TN_ONLINE 	1
#define	ST_ONLINE 	2
#define	TN_OFFLINE  3
#define	ST_OFFLINE  4

class OnlineStatus 
{
	typedef void ( *StatusCallback )();

	private:
		uint8_t state = ST_ONLINE;
		uint8_t oldstate = ST_ONLINE;

		StatusCallback _offlineCallback;
		StatusCallback _onlineCallback;

	public:

		OnlineStatus(StatusCallback offlineCallback, StatusCallback onlineCallback) {
			_offlineCallback = offlineCallback;
			_onlineCallback = onlineCallback;
			state = ST_ONLINE;
		}

		bool serviceState(bool online) {
			switch (state) {
				case TN_ONLINE:
					state = online ? ST_ONLINE : TN_OFFLINE;
					_onlineCallback();
					break;
				case ST_ONLINE:
					state = online ? ST_ONLINE : TN_OFFLINE;
					break;
				case TN_OFFLINE:
					state = online ? TN_ONLINE : ST_OFFLINE;
					_offlineCallback();
					break;
				case ST_OFFLINE:
					state = online ? TN_ONLINE : ST_OFFLINE;
					break;
				default:
					state = online ? TN_ONLINE : TN_OFFLINE;
					break;
			}
			bool stateChanged = oldstate != state;
			oldstate = state;
			return stateChanged;
		}

		bool getState() { return state; }
		bool isOnline() { return state == ST_ONLINE; }
};

OnlineStatus boardCommsStatus(&boardOfflineCallback, &boardOnlineCallback);

//------------------------------------------------------------------------------
void setControllerPacketThrottle() {

	bool accelerating = encoderCounter > oldEncoderCounter;
	bool decelerating = encoderCounter < oldEncoderCounter;

	esk8.controllerPacket.throttle = mapEncoderToThrottleValue(encoderCounter);

	throttleChanged = accelerating || decelerating;
}

void packetAvailable_Callback(int test) {
	debug.print(COMMUNICATION, "packetAvailable_Callback() \n");
}

//--------------------------------------------------------------

#define COMMS_TIMEOUT_PERIOD 	1000

/**************************************************************
					SETUP
**************************************************************/
void setup() {

	Serial.begin(9600);

	debug.init();
	debug.addOption(STARTUP, "STARTUP");
	debug.addOption(DEBUG, "DEBUG");
	debug.addOption(HARDWARE, "HARDWARE");
	debug.addOption(COMMUNICATION, "COMMUNICATION");
	debug.addOption(ONLINE_STATUS, "ONLINE_STATUS");
	debug.addOption(TIMING, "TIMING");
	//debug.setFilter( STARTUP | COMMUNICATION | ONLINE_STATUS | TIMING );
	debug.setFilter( STARTUP | COMMUNICATION );

	setupRadio();

	Wire.begin();

	tft.begin();

  	tft.setRotation(1);
	tft.fillScreen(TFT_BLACK);            // Clear screen

	//img.setColorDepth(8); // Optionally set depth to 8 to halve RAM use
	img.createSprite(IWIDTH, IHEIGHT);
	img.fillSprite(TFT_BLUE);
	img.setFreeFont(FF18);                 // Select the font
  	img.setTextDatum(MC_DATUM);
	img.setTextColor(TFT_YELLOW, TFT_BLACK);
	img.drawString("Ready!", 10, 20, 2);
	
	WiFi.mode( WIFI_OFF );	// WIFI_MODE_NULL
    btStop();   // turn bluetooth module off

	debug.print(STARTUP, "%s \n", compile_date);
    debug.print(STARTUP, "esk8Project/Controller.ino \n");

	throttleChanged = true;	// initialise

	esk8.begin(esk8.RF24_CONTROLLER);

	bool encoderModuleOnline = setupEncoderModule(/* min */ -20, /* max */ 15);

	img.drawString( encoderModuleOnline 
		? "ENCODER_MODULE: connected" 
		: "ENCODER_MODULE: --------", 
		10, 20, 1);
	img.pushSprite(200, 100); delay(500);	

	sendMessage();

	runner.startNow();
	runner.addTask(tFlashLeds);
	runner.addTask(tSendControllerValues);
	tSendControllerValues.enable();

	boardCommsStatus.serviceState(false);

	xTaskCreatePinnedToCore (
		codeForEncoderTask,	// function
		"Task_Encoder",		// name
		10000,			// stack
		NULL,			// parameter
		1,				// priority
		NULL,	// handle
		0
	);				// port	
}
/**************************************************************
					LOOP
**************************************************************/
long nowms = 0;
long connectedNow = 0;
bool oldConnected = false;
#define BOARD_OFFLINE_PERIOD	1000
long rxCount = 0;
unsigned long last_sent_to_board = 50;
unsigned long last_updated_M5 =100;
int tx_interval = 200;
int update_M5_interval = 1000;

void loop() {

	bool connected = millis() - lastRxFromBoard < BOARD_OFFLINE_PERIOD;

	boardCommsStatus.serviceState(connected);

	runner.execute();

	if ( throttleChanged ) {
		throttleChanged = false;

		// if ( esk8.controllerPacket.throttle == 127 ) {
		// // 	char buf[100];
		// // 	sprintf(buf, "%0.1f", esk8.boardPacket.batteryVoltage);
		// }
		// else if (esk8.controllerPacket.throttle != 127) {
		// 	// maybe hide display?
		// 	updateDisplay(/*mode*/1, /*backlight*/0);
		// }
	}

	vTaskDelay( 10 );
}
/**************************************************************
					TASK 0
**************************************************************/
void codeForEncoderTask( void *parameter ) {

	#define TX_INTERVAL 200
	long nowMs = 0;
	
	for (;;) {

		encoderModule.update();

		m5ButtonA.serviceEvents();

		if (m5ButtonA.pressedForNumMs(2000)) {
			Serial.printf("Held down!\n");
		}

		vTaskDelay( 10 );
	}

	vTaskDelete(NULL);
}
//**************************************************************

bool sendToBoard() {

	radio.stopListening();

	bool sentOK = radio.write( &esk8.controllerPacket, sizeof(esk8.controllerPacket) );

	radio.startListening();

	debug.print(COMMUNICATION, "Sending: %d ", esk8.controllerPacket.throttle);

    bool timeout = false;                    
    // wait until response has arrived
    if ( !radio.available() ){                             // While nothing is received
		debug.print(COMMUNICATION, "NO_ACK \n");
	}
    else {
    	while ( radio.available() ) {
			radio.read( &esk8.boardPacket, sizeof(esk8.boardPacket) );
		}
		debug.print(COMMUNICATION, "Rx: %d \n", esk8.boardPacket.id);
    }
	
    return sentOK;
}
//--------------------------------------------------------------
int mapEncoderToThrottleValue(int raw) {
	int mappedThrottle = 0;
	int rawMiddle = 0;

	if (raw >= rawMiddle) {
		return map(raw, rawMiddle, encoderModule.encoderCounterMaxLimit, 127, 255);
	}
	return map(raw, encoderModule.encoderCounterMinLimit, rawMiddle, 0, 127);
}
//--------------------------------------------------------------
void setupRadio() {
    radio.begin();
    radio.setPALevel(RF24_PA_MIN);
    radio.setAutoAck(1);                    // Ensure autoACK is enabled
  	radio.enableAckPayload();               // Allow optional ack payloads

  	radio.openWritingPipe( esk8.listening_pipes[esk8.RF24_BOARD] );
  	radio.openReadingPipe( 1, esk8.talking_pipes[esk8.RF24_BOARD] );

    radio.printDetails();
}
//--------------------------------------------------------------
int setupEncoderModule(int minLimit, int maxLimit) {
		
	encoderModule.update();

	return  
		encoderModule.setPixelBrightness(10) == 0 &&
		encoderModule.setEncoderLimits(minLimit, maxLimit) == 0;
}
//--------------------------------------------------------------

void updateDisplay(int mode, int backlight) {
	#define LINE_1 20
	#define LINE_2 45
	#define LINE_3 70
	#define LINE_4 95

	if ( backlight == false ) {
		digitalWrite(TFT_BL, LOW);
		return;
	}

	switch (mode) {
		case 0:
			break;
		case 1:
		  	img.setTextDatum(MC_DATUM);
			img.setTextSize(1);
			img.fillScreen(TFT_DARKGREEN);
			img.setTextColor(TFT_YELLOW, TFT_BLACK);
			img.drawNumber( esk8.controllerPacket.throttle,  10, 10);		
			img.pushSprite(20, 20); delay(0);	

			digitalWrite(TFT_BL, HIGH);	// turn backlight off?

			break;
	}
}