
#include <esp_now.h>
#include <WiFi.h>
#include <HUDLibrary.h>

// Global copy of slave
#define NUMSLAVES 20
esp_now_peer_info_t slave;
int slaveCount = 0;

#define CHANNEL 3
#define PRINTSCANRESULTS 0


HUDLib hud(/*debug*/ true);

// Init ESP Now with fallback
void InitESPNow() {
	WiFi.disconnect();
	if (esp_now_init() == ESP_OK) {
		Serial.println("ESPNow Init Success");
	}
	else {
		Serial.println("ESPNow Init Failed");
		ESP.restart();
	}
}

// Scan for slaves in AP mode
bool ScanForSlave() {

	int8_t scanResults = WiFi.scanNetworks(); // scans for devices in AP mode
	slaveCount = 0;

	if (scanResults == 0) {
		Serial.println("No WiFi devices in AP Mode found");
	} else {
		Serial.print("Found "); Serial.print(scanResults); Serial.println(" devices ");
		for (int i = 0; i < scanResults; ++i) {
			// Print SSID and RSSI for each device found
			String SSID = WiFi.SSID(i);
			int32_t RSSI = WiFi.RSSI(i);
			String BSSIDstr = WiFi.BSSIDstr(i);

			delay(10);
			// Check if the current device starts with `HUD_SSID`
			if ( SSID.indexOf("HUD_SSID") == 0 ) {
				Serial.printf("%d: %s [%s] (RSSI: %d) \n", i+1, SSID.c_str(), BSSIDstr.c_str(), RSSI);
				int mac[6];

				if ( 6 == sscanf(BSSIDstr.c_str(), "%x:%x:%x:%x:%x:%x%c",  &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5] ) ) {
					for (int ii = 0; ii < 6; ++ii ) {
						slave.peer_addr[ii] = (uint8_t) mac[ii];
					}
				}
				slave.channel = CHANNEL; // pick a channel
				slave.encrypt = 0; // no encryption
				slaveCount++;
			}
		}
	}

	WiFi.scanDelete();	// clean up ram

	return slaveCount == 1;
}

// Check if the slave is already paired with the master.
// If not, pair the slave with master
bool manageSlave() {

	bool success = false;
	esp_err_t addStatus;

	if (slaveCount > 0) {
		for (int i = 0; i < slaveCount; i++) {
			const esp_now_peer_info_t *peer = &slave;
			const uint8_t *peer_addr = slave.peer_addr;

			addStatus = esp_now_add_peer(peer);
			if (addStatus == ESP_OK) {
				// Pair success
				Serial.println("Pair success");
			} else if (addStatus == ESP_ERR_ESPNOW_NOT_INIT) {
				// How did we get so far!!
				Serial.println("ESPNOW Not Init");
			} else if (addStatus == ESP_ERR_ESPNOW_ARG) {
				Serial.println("Add Peer - Invalid Argument");	
			} else if (addStatus == ESP_ERR_ESPNOW_FULL) {
				Serial.println("Peer list full");
			} else if (addStatus == ESP_ERR_ESPNOW_NO_MEM) {
				Serial.println("Out of memory");
			} else if (addStatus == ESP_ERR_ESPNOW_EXIST) {
				Serial.println("Peer Exists");
			} else {
				Serial.println("Not sure what happened");
			}
			delay(100);
		}
	} else {
		// No slave found to process
		Serial.println("No Slave found to process");
	}
	return addStatus == ESP_OK;
}
//--------------------------------------------------
bool sendDataOk() {

	long m = millis();

	hud.data.id++;
	hud.data.controllerLedState = hud.Ok;
	hud.data.boardLedState = hud.Error;
	hud.data.vescLedState = hud.FlashingError;

	const uint8_t *peer_addr = slave.peer_addr;
	Serial.printf("Sending: %d\n", hud.data.id);
	
	uint8_t bs[sizeof(hud.data)];
	memcpy(bs, &hud.data, sizeof(hud.data));
	esp_err_t result = esp_now_send(peer_addr, bs, sizeof(hud.data));

	return result == ESP_OK;
}
//--------------------------------------------------
// callback when data is sent from Master to Slave
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
	char macStr[18];
	snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
	       mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
	Serial.printf("Last Packet Send Status: %s\n", status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

//--------------------------------------------------

void setup() {

	Serial.begin(9600);
	//Set device in STA mode to begin with
	WiFi.mode(WIFI_STA);
	Serial.println("ESPNow/Multi-Slave/Master Example");
	// This is the mac address of the Master in Station Mode
	Serial.print("STA MAC: "); Serial.println(WiFi.macAddress());

	hud.data.id = 0;

	// Init ESPNow with a fallback logic
	InitESPNow();

	esp_now_register_send_cb(OnDataSent);

	while (slaveCount == 0) {
		ScanForSlave();
		manageSlave();
	}
}

void loop() {

    if ( sendDataOk() ) {

    }

	// wait for 3seconds to run the logic again
	delay(1000);
}