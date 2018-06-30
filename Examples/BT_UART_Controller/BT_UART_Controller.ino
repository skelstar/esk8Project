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
		//Serial.printf("Notify callback for characteristic \n");
	    Serial.println("-------------------------------------------");
	    Serial.printf("notifyCallback: length=%d \n", length);
	    Serial.printf("sizeof(pData) %u \n", sizeof(pData));
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

	// If we are connected to a peer BLE Server, update the characteristic each time we are reached
	// // with the current time since boot.
	// if (connected) {
	// 	String newValue = "Time since boot: " + String(millis()/1000);
	// 	Serial.println("Setting new characteristic value to \"" + newValue + "\"");

	// 	// Set the characteristic's value to be the array of bytes that is actually a string.
	// 	pRemoteCharacteristic->writeValue(newValue.c_str(), newValue.length());
	// }

	if (connected && millis() - nowms > 5000) {
		nowms = millis();
		Serial.println("======================================================");
		getVescValues();
	}

	delay(100); // Delay a second between loops.
} // End of loop


void getVescValues() {

	// struct VESCValues
	// {
	//		float temperatureMosfet1 = 0.0f;
	//		float temperatureMosfet2 = 0.0f;
	//		float temperatureMosfet3 = 0.0f;
	//		float temperatureMosfet4 = 0.0f;
	//		float temperatureMosfet5 = 0.0f;
	//		float temperatureMosfet6 = 0.0f;
	//		float temperaturePCB = 0.0f;
	//		float avgMotorCurrent = 0.0f;
	//		float avgInputCurrent = 0.0f;
	//		float dutyCycleNow = 0.0f;
	//		int32_t rpm = 0;
	//		float inputVoltage = 0.0f;
	//		float ampHours = 0.0f;
	//		float ampHoursCharged = 0.0f;
	//		float wattHours = 0.0f;
	//		float wattHoursCharged = 0.0f;
	//		int32_t tachometer = 0;
	//		int32_t tachometerAbs = 0;
	//		mc_fault_code faultCode = FAULT_CODE_NONE;
	// };

    VESCValues vescValues;

	esp8266VESC.requestVescValues(vescValues);

	uint8_t* payload = esp8266VESC.getCommandPayload();
	uint8_t payloadLength = esp8266VESC.getCommandPayloadLength();

	// Serial.printf("sizeof packet = %d\n", payloadLength);

	pRemoteCharacteristic->writeValue(payload, payloadLength);
		// get the packet that should be sent
		//esp8266VESC.requestVESCValues(vescValues);

		// vescConnected = true;
		// esk8.boardPacket.batteryVoltage = vescValues.inputVoltage;
		// Serial.println("Average motor current = " + String(vescValues.avgMotorCurrent) + "A");
		// Serial.println("Average battery current = " + String(vescValues.avgInputCurrent) + "A");
		// Serial.println("Duty cycle = " + String(vescValues.dutyCycleNow) + "%");
	    
		//Serial.println("rpm = " + String(vescValues.rpm) + "rpm");

		// Serial.println("Battery voltage = " + String(vescValues.inputVoltage) + "V");

		// Serial.println("Drawn energy (mAh) = " + String(vescValues.ampHours) + "mAh");
		// Serial.println("Charged energy (mAh) = " + String(vescValues.ampHoursCharged) + "mAh");

		// Serial.println("Drawn energy (Wh) = " + String(vescValues.wattHours) + "Wh");
		// Serial.println("Charged energy (Wh) = " + String(vescValues.wattHoursCharged) + "Wh");

		// String(vescValues.ampHours).toCharArray(myVescValues.ampHours, 7);
		// String(vescValues.rpm).toCharArray(myVescValues.rpm, 7);
			// String(vescValues.inputVoltage).toCharArray(myVescValues.battery, 7);
		// myVescValues.battery = vescValues.inputVoltage;
		// String(vescValues.avgMotorCurrent).toCharArray(myVescValues.avgMotorCurrent, 7);
		// String(vescValues.dutyCycleNow).toCharArray(myVescValues.dutyCycleNow, 7);
	// }
	// else
	// {
	// 	vescConnected = false;
	// 	//debug.print(VESC_COMMS, "The VESC values could not be read!\n");
	// }
	//return vescConnected;
}