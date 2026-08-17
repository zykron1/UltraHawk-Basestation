#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <string.h>

enum States : uint8_t { IDLE, TELEOP, AUTON };

typedef struct __attribute__((packed)) {
    uint32_t packet_number;
    float missionTime;
    uint8_t state;
    float gx, gy, gz;
    float ax, ay, az;
    float roll, pitch, yaw;
    float x, y, z;
    float m1, m2, m3, m4;
} TelemetryPacket;

typedef struct __attribute__((packed)) {
    uint8_t state;
    float thrust, roll, pitch, yaw;
    float xpos, ypos, zpos;
} Command;

// This must be the drone's MAC address.
uint8_t droneAddress[] = {0x58, 0x8c, 0x81, 0xac, 0xbd, 0xa4};

TelemetryPacket latestTelemetry = {};
Command command = {IDLE, 0, 0, 0, 0, 0, 0, 0};

portMUX_TYPE telemetryMux = portMUX_INITIALIZER_UNLOCKED;

bool commandValid = false;
uint32_t lastCommandSend = 0;
uint32_t lastTelemetryPrint = 0;

const uint32_t COMMAND_SEND_PERIOD_MS = 50;
const uint32_t TELEMETRY_PRINT_PERIOD_MS = 100;

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (len != sizeof(TelemetryPacket)) {
        return;
    }

    TelemetryPacket received;
    memcpy(&received, incomingData, sizeof(received));

    if (received.state > AUTON) {
        return;
    }

    portENTER_CRITICAL(&telemetryMux);
    latestTelemetry = received;
    portEXIT_CRITICAL(&telemetryMux);
}

bool parseCommand(char *line) {
    char *token = strtok(line, " ");
    if (token == nullptr) {
        return false;
    }

    int state = atoi(token);
    if (state < IDLE || state > AUTON) {
        Serial.println("Invalid state");
        return false;
    }

    float values[7] = {};
    int valueCount = 0;

    while ((token = strtok(nullptr, " ")) != nullptr && valueCount < 7) {
        values[valueCount++] = atof(token);
    }

    Command parsed = {IDLE, 0, 0, 0, 0, 0, 0, 0};
    parsed.state = static_cast<uint8_t>(state);

    if (state == IDLE) {
        command = parsed;
        return true;
    }

    if (state == TELEOP) {
        if (valueCount < 4) {
            Serial.println("TELEOP requires: state thrust roll pitch yaw");
            return false;
        }

        parsed.thrust = values[0];
        parsed.roll = values[1];
        parsed.pitch = values[2];
        parsed.yaw = values[3];
    }

    if (state == AUTON) {
        if (valueCount >= 7) {
            // Original format: state thrust roll pitch yaw xpos ypos zpos
            parsed.xpos = values[4];
            parsed.ypos = values[5];
            parsed.zpos = values[6];
        } else if (valueCount >= 3) {
            // Short format: state xpos ypos zpos
            parsed.xpos = values[0];
            parsed.ypos = values[1];
            parsed.zpos = values[2];
        } else {
            Serial.println("AUTON requires: state xpos ypos zpos");
            return false;
        }
    }

    command = parsed;
    return true;
}

void sendCommand() {
    if (!commandValid) {
        return;
    }

    esp_err_t result = esp_now_send(
        droneAddress, reinterpret_cast<uint8_t *>(&command), sizeof(command));

    if (result != ESP_OK) {
        Serial.printf("Command send failed: %d\n", result);
    }
}

void setup() {
    Serial.begin(115200);

    WiFi.mode(WIFI_STA);

    Serial.print("Ground station MAC: ");
    Serial.println(WiFi.macAddress());

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
        while (true) {
            delay(1000);
        }
    }

    esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, droneAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (!esp_now_is_peer_exist(droneAddress)) {
        esp_err_t result = esp_now_add_peer(&peerInfo);
        if (result != ESP_OK) {
            Serial.printf("Peer add failed: %d\n", result);
        }
    }

    Serial.println("Ground station ready");
}

const size_t MAX_LINE_LENGTH = 128;
char lineBuffer[MAX_LINE_LENGTH];
size_t bufferIndex = 0;

void loop() {
    uint32_t now = millis();

    if (now - lastTelemetryPrint >= TELEMETRY_PRINT_PERIOD_MS) {
        lastTelemetryPrint = now;

        TelemetryPacket telemetry;
        portENTER_CRITICAL(&telemetryMux);
        telemetry = latestTelemetry;
        portEXIT_CRITICAL(&telemetryMux);

        Serial.printf("DP %lu t=%.3f roll=%.2f pitch=%.2f yaw=%.2f "
                      "state=%u motor=%.1f %.1f %.1f %.1f\n",
                      static_cast<unsigned long>(telemetry.packet_number),
                      telemetry.missionTime, telemetry.roll, telemetry.pitch,
                      telemetry.yaw, telemetry.state, telemetry.m1,
                      telemetry.m2, telemetry.m3, telemetry.m4);
    }

    while (Serial.available() > 0) {
        char c = Serial.read();

        if (c == '\n' || c == '\r') {
            if (bufferIndex > 0) {
                lineBuffer[bufferIndex] = '\0';

                if (parseCommand(lineBuffer)) {
                    commandValid = true;
                    sendCommand();
                }

                bufferIndex = 0;
            }
        } else if (bufferIndex < MAX_LINE_LENGTH - 1) {
            lineBuffer[bufferIndex++] = c;
        }
    }

    if (now - lastCommandSend >= COMMAND_SEND_PERIOD_MS) {
        lastCommandSend = now;
        sendCommand();
    }
}
