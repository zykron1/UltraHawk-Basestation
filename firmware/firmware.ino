#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <string>

// Must be the same on UltraHawk
enum States {
	IDLE,
	TELEOP,
	AUTON,
};

typedef struct __attribute__((packed)) {
	size_t packet_number;
	float missionTime;
	States state;
	float gx, gy, gz, ax, ay, az; // imu data
	float roll, pitch, yaw; // orientation
	float x, y, z; // location
	float m1, m2, m3, m4;
} TelemetryPacket;

typedef struct __attribute__((packed)) {
	States state;
	float thrust, roll, pitch, yaw; // if teleop
	float xpos, ypos, zpos; // if auton
} Command;

TelemetryPacket myData;
String recievedCommand;
uint8_t broadcastAddress[] = {0x58, 0x8c, 0x81, 0xac, 0xbd, 0xa4};

Command cmd;

void parseCommand(char* line) {
    Serial.print("DEBUG Received: ");
    Serial.println(line);

    char* token = strtok(line, " ");
    
    if (token == NULL) return; 

    cmd.state = (States)atoi(token);

    float* values[] = { &cmd.thrust, &cmd.roll, &cmd.pitch, &cmd.yaw, &cmd.xpos, &cmd.ypos, &cmd.zpos };
    
    for (int i = 0; i < 7; i++) {
        token = strtok(NULL, " ");
        if (token != NULL) {
            *values[i] = atof(token);
        } else {
            Serial.println("Error: Incomplete packet received!");
            return; // Exit early if packet is malformed
        }
    }

    Serial.println("parsed!!");
}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
	memcpy(&myData, incomingData, sizeof(myData));
}

void setup() {
	delay(2000);
	WiFi.mode(WIFI_STA);
	esp_now_init();
	esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

	esp_now_peer_info_t peerInfo = {};
	memcpy(peerInfo.peer_addr, broadcastAddress, 6);
	peerInfo.channel = 0;  
	peerInfo.encrypt = false;
	esp_now_add_peer(&peerInfo);

	Serial.begin(9600);
}

const size_t MAX_LINE_LENGTH = 128;
char lineBuffer[MAX_LINE_LENGTH];
size_t bufferIndex = 0;

void loop() {
	Serial.printf("DP %zu t=%f %f %f %f state=%d motor=%f %f %f %f\n", 
		myData.packet_number,
		myData.missionTime,
		myData.roll,
		myData.pitch,
		myData.yaw,
		(int)myData.state,
		myData.m1,
		myData.m2,
		myData.m3,
		myData.m4
	);

    while (Serial.available()) {
        char c = Serial.read();
        
        if (c == '\n' || c == '\r') {
            if (bufferIndex > 0) {
                lineBuffer[bufferIndex] = '\0';                
                parseCommand(lineBuffer);
				esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)&cmd, sizeof(cmd));
				if (result == ESP_OK) {
					Serial.println("ESP NOW WORK");
				}
				bufferIndex = 0;
            }
        } else if (bufferIndex < MAX_LINE_LENGTH - 1) {
            lineBuffer[bufferIndex++] = c;
        }
    }
}
