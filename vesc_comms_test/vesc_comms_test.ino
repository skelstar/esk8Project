#include "vesc_comm.h";

uint8_t vesc_packet[PACKET_MAX_LENGTH];

#define DEBUG

void setup() {
  Serial.begin(9600);

  vesc_comm_init(115200);
}

void loop() {
  uint8_t bytes_read = vesc_comm_fetch_packet(vesc_packet);
  delay(200);
}