#include <esp_now.h>
#include <WiFi.h>
#include <HUDLibrary.h>

esp_now_peer_info_t slave;

#define CHANNEL 3
#define PRINTSCANRESULTS 0

HUDLib hud(/*debug*/ true);

int slaveCount = 0;

// Init ESP Now with fallback
void InitESPNow() {
	WiFi.disconnect();
	if (esp_now_init() == ESP_OK) {
		debug.print(STARTUP, "ESPNow Init Success \n");
	}
	else {
		debug.print(STARTUP, "ESPNow Init Failed \n");
		ESP.restart();
	}
}

// Scan for slaves in AP mode
bool ScanForSlave() {

	int8_t scanResults = WiFi.scanNetworks(); // scans for devices in AP mode
	slaveCount = 0;

	if (scanResults == 0) {
		debug.print(DEBUG, "No WiFi devices in AP Mode found\n");
	} else {
		debug.print(DEBUG, "Found %d devices!\n", scanResults);
		for (int i = 0; i < scanResults; ++i) {
			// Print SSID and RSSI for each device found
			String SSID = WiFi.SSID(i);
			int32_t RSSI = WiFi.RSSI(i);
			String BSSIDstr = WiFi.BSSIDstr(i);

			vTaskDelay( 10 );
			// Check if the current device starts with `HUD_SSID`
			if ( SSID.indexOf("HUD_SSID") == 0 ) {
				debug.print(DEBUG, "%d: %s [%s] (RSSI: %d) \n", i+1, SSID.c_str(), BSSIDstr.c_str(), RSSI);
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
				debug.print(DEBUG, "Pair success\n");
			} else if (addStatus == ESP_ERR_ESPNOW_NOT_INIT) {
				// How did we get so far!!
				debug.print(DEBUG, "ESPNOW Not Init\n");
			} else if (addStatus == ESP_ERR_ESPNOW_ARG) {
				debug.print(DEBUG, "Add Peer - Invalid Argument\n");	
			} else if (addStatus == ESP_ERR_ESPNOW_FULL) {
				debug.print(DEBUG, "Peer list full\n");
			} else if (addStatus == ESP_ERR_ESPNOW_NO_MEM) {
				debug.print(DEBUG, "Out of memory\n");
			} else if (addStatus == ESP_ERR_ESPNOW_EXIST) {
				debug.print(DEBUG, "Peer Exists\n");
			} else {
				debug.print(DEBUG, "Not sure what happened\n");
			}
			vTaskDelay( 100 );
		}
	} else {
		// No slave found to process
		debug.print(DEBUG, "No Slave found to process\n");
	}
	return addStatus == ESP_OK;
}
//--------------------------------------------------
bool sendDataToHUD_Ok() {

	long m = millis();

	hud.data.id++;
	hud.data.controllerLedState = m/1000 % 3 == 0 ? hud.Error : hud.FlashingError;
	hud.data.boardLedState = m/1000 % 2 == 0 ? hud.Error : hud.Ok;
	hud.data.vescLedState = hud.FlashingError;

	const uint8_t *peer_addr = slave.peer_addr;
	
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
	// Serial.printf("Last Packet Send Status: %s\n", status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}
//--------------------------------------------------
void setupEspNow() {

	//Set device in STA mode to begin with
	WiFi.mode(WIFI_STA);

	// This is the mac address of the Master in Station Mode
	debug.print(STARTUP, "STA MAC: %s \n", WiFi.macAddress());

	hud.data.id = 0;

	// Init ESPNow with a fallback logic
	InitESPNow();

	esp_now_register_send_cb(OnDataSent);

	ScanForSlave();
	manageSlave();
}
