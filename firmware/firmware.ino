#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// Must be the same on UltraHawk
typedef struct {
	size_t packet_number;
	float missionTime;
	float gx, gy, gz, ax, ay, az; // imu data
	float roll, pitch, yaw; // orientation
	float x, y, z; // location
} TelemetryPacket;

TelemetryPacket myData;

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
	memcpy(&myData, incomingData, sizeof(myData));
}

void setup() {
	delay(2000);
	WiFi.mode(WIFI_STA);
	esp_now_init();
	esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

	Serial.begin(9600);
}

void loop() {
	Serial.printf("DP %zu t=%f %f %f %f \n", myData.packet_number, myData.missionTime, myData.roll, myData.pitch, myData.yaw);
}

