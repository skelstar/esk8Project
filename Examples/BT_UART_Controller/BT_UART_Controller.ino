/**
 * A BLE client example that is rich in capabilities.
 */

#include "BLEDevice.h"
#include <ESP8266VESC.h>
#include "VescUart.h"

ESP8266VESC esp8266VESC = ESP8266VESC();

static BLEUUID serviceUUID("0000ffe0-0000-1000-8000-00805f9b34fb");
static BLEUUID    charUUID("0000ffe1-0000-1000-8000-00805f9b34fb");

static BLEAddress *pServerAddress;
static boolean doConnect = false;
static boolean connected = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
//--------------------------------------------------------------
static void notifyCallback(
	BLERemoteCharacteristic* pBLERemoteCharacteristic,
	uint8_t* pData,
	size_t length,
	bool isNotify) {
	    Serial.println("----------- N O T I F Y -----------");
		esp8266VESC.receivePacket(pData, length);
}

//--------------------------------------------------------------
bool connectToServer(BLEAddress pAddress) {
    Serial.print("Forming a connection to ");
    Serial.println(pAddress.toString().c_str());
    
    BLEClient*  pClient  = BLEDevice::createClient();
    Serial.println(" - Created client");

    // Connect to the remove BLE Server.
    pClient->connect(pAddress);
    Serial.println(" - Connected to server");

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
		Serial.print("Failed to find our service UUID: ");
		Serial.println(serviceUUID.toString().c_str());
		return false;
    }
    Serial.println(" - Found our service");

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
		Serial.print("Failed to find our characteristic UUID: ");
		Serial.println(charUUID.toString().c_str());
		return false;
    }
    Serial.println(" - Found our characteristic");

    // Read the value of the characteristic.
    std::string value = pRemoteCharacteristic->readValue();
    Serial.print("The characteristic value was: ");
    Serial.println(value.c_str());

    pRemoteCharacteristic->registerForNotify(notifyCallback);
}
//--------------------------------------------------------------
/*Scan for BLE servers and find the first one that advertises the service we are looking for. */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
	/* Called for each advertising BLE server. */
  	void onResult(BLEAdvertisedDevice advertisedDevice) {
		Serial.print("BLE Advertised Device found: ");
		Serial.println(advertisedDevice.toString().c_str());

		// We have found a device, let us now see if it contains the service we are looking for.
		if (advertisedDevice.haveServiceUUID() && advertisedDevice.getServiceUUID().equals(serviceUUID)) {
			Serial.print("Found our device!  address: "); 
			advertisedDevice.getScan()->stop();

			pServerAddress = new BLEAddress(advertisedDevice.getAddress());
			doConnect = true;
		} // Found our server
  	}
};
//--------------------------------------------------------------
void setup() {
	Serial.begin(9600);
	Serial.println("Starting Arduino BLE Client application...");
	BLEDevice::init("ESP32 BLE Client");

	// Retrieve a Scanner and set the callback we want to use to be informed when we
	// have detected a new device.  Specify that we want active scanning and start the
	// scan to run for 30 seconds.
	BLEScan* pBLEScan = BLEDevice::getScan();
	pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
	pBLEScan->setActiveScan(true);
	pBLEScan->start(30);
} // End of setup.

long nowms = 0;
long accelDEcelMs = 0;

#define ACCEL_DECEL_STATE_IDLE	0
#define ACCEL_DECEL_STATE_ACCEL	1
#define ACCEL_DECEL_STATE_DECEL	2

int accelDecelState = 0;

void loop() {
	// If the flag "doConnect" is true then we have scanned for and found the desired
	// BLE Server with which we wish to connect.  Now we connect to it.  Once we are 
	// connected we set the connected flag to be true.
	if (doConnect == true) {
		if (connectToServer(*pServerAddress)) {
			Serial.println("We are now connected to the BLE Server.");
			connected = true;
		} else {
			Serial.println("We have failed to connect to the server; there is nothin more we will do.");
		}
		doConnect = false;
	}

	if (connected) {

		if (millis() - nowms > 5000) {
			nowms = millis();
			//Serial.println("======================================================");
			//sendGetValuesRequest();
		}

		if (accelDecelState == ACCEL_DECEL_STATE_IDLE || accelDecelState == ACCEL_DECEL_STATE_DECEL) {
			if (millis()-accelDEcelMs > 3000) {
				accelDEcelMs = millis();
				accelDecelState = ACCEL_DECEL_STATE_ACCEL;
				sendNunchukValues(127+15);
			}
		}
		else {
			if (millis()-accelDEcelMs > 1000) {
				accelDEcelMs = millis();
				accelDecelState = ACCEL_DECEL_STATE_DECEL;
				sendNunchukValues(127);
			}
		}

	}

	delay(10); // Delay a second between loops.
} // End of loop

//--------------------------------------------------------------
void sendGetValuesRequest() {

	esp8266VESC.composeGetValuesRequest();

	uint8_t* payload = esp8266VESC.getCommandPayload();
	uint8_t payloadLength = esp8266VESC.getCommandPayloadLength();

	pRemoteCharacteristic->writeValue(payload, payloadLength);
}
//--------------------------------------------------------------
void sendNunchukValues(int throttle) {

	Serial.printf("sendNunchukValues(int %d) \n", throttle);
	esp8266VESC.composeSendNunchukValuesRequest(127, throttle, 0, 0);

	uint8_t* payload = esp8266VESC.getCommandPayload();
	uint8_t payloadLength = esp8266VESC.getCommandPayloadLength();

	pRemoteCharacteristic->writeValue(payload, payloadLength);
}