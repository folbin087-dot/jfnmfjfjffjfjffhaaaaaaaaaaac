#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <random>
#include "math/Vector3.h"

// Cloud radar using public MQTT broker (EMQX) - NO SSL, simple TCP
// Viewer connects via WebSocket on browser
class CloudRadar {
public:
    std::atomic<bool> running{false};
    std::atomic<bool> connected{false};
    
    std::string roomCode = "";  // 6-char code for sharing
    
    struct RadarData {
        Vector3 localPosition;
        Vector3 cameraPosition;
        float cameraSize = 50.0f;
        bool mapDataValid = false;
        std::string mapName = "";
        std::string localName = "";
        Vector3 enemyPositions[16];
        std::string enemyNames[16];
        int enemyCount = 0;
        Vector3 teamPositions[16];
        std::string teamNames[16];
        int teamCount = 0;
        Vector3 grenadePositions[16] = {};
        int grenadeTypes[16] = {0};
        int grenadeCount = 0;
        
        // Planted bomb
        Vector3 bombPosition;
        float bombTimer = 0.0f;
        bool bombPlanted = false;
    };
    
    RadarData data;
    std::mutex dataMutex;
    
    void GenerateRoomCode() {
        static const char chars[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);
        
        roomCode = "";
        for (int i = 0; i < 6; i++) {
            roomCode += chars[dis(gen)];
        }
    }
    
    std::string GetRoomCode() const { return roomCode; }
    
    void Start() {
        if (running) return;
        
        if (roomCode.empty()) {
            GenerateRoomCode();
        }
        
        running = true;
        mqttThread = std::thread(&CloudRadar::MqttLoop, this);
        mqttThread.detach();
        
        std::cout << "=== CLOUD RADAR STARTED ===" << std::endl;
        std::cout << "Room Code: " << roomCode << std::endl;
        std::cout << "Share this code with friends!" << std::endl;
        std::cout << "============================" << std::endl;
    }
    
    void Stop() {
        running = false;
        connected = false;
        if (sock >= 0) {
            close(sock);
            sock = -1;
        }
    }
    
    void UpdateData(const Vector3& localPos, const Vector3& camPos, float camSize,
                    bool valid, const std::string& mapName, const std::string& localName,
                    const Vector3* enemies, const std::string* enemyNames, int enemyCount,
                    const Vector3* teammates, const std::string* teamNames, int teamCount,
                    const Vector3* grenades = nullptr, const int* grenadeTypes = nullptr, int grenadeCount = 0,
                    bool bombPlanted = false, const Vector3& bombPos = Vector3(), float bombTimer = 0.0f) {
        std::lock_guard<std::mutex> lock(dataMutex);
        data.localPosition = localPos;
        data.cameraPosition = camPos;
        data.cameraSize = camSize;
        data.mapDataValid = valid;
        data.mapName = mapName;
        data.localName = localName;
        data.enemyCount = (enemyCount > 16) ? 16 : enemyCount;
        for (int i = 0; i < data.enemyCount; i++) {
            data.enemyPositions[i] = enemies[i];
            data.enemyNames[i] = enemyNames[i];
        }
        data.teamCount = (teamCount > 16) ? 16 : teamCount;
        for (int i = 0; i < data.teamCount; i++) {
            data.teamPositions[i] = teammates[i];
            data.teamNames[i] = teamNames[i];
        }
        data.grenadeCount = (grenadeCount > 16) ? 16 : grenadeCount;
        for (int i = 0; i < data.grenadeCount; i++) {
            if (grenades) data.grenadePositions[i] = grenades[i];
            if (grenadeTypes) data.grenadeTypes[i] = grenadeTypes[i];
        }
        data.bombPlanted = bombPlanted;
        data.bombPosition = bombPos;
        data.bombTimer = bombTimer;
    }

private:
    std::thread mqttThread;
    int sock = -1;
    
    std::string BuildCompactJSON() {
        std::lock_guard<std::mutex> lock(dataMutex);
        std::ostringstream j;
        j << "{\"v\":" << (data.mapDataValid ? 1 : 0);
        j << ",\"m\":\"" << data.mapName << "\"";
        j << ",\"cx\":" << (int)data.cameraPosition.x;
        j << ",\"cz\":" << (int)data.cameraPosition.z;
        j << ",\"cs\":" << (int)data.cameraSize;
        j << ",\"lx\":" << std::fixed << std::setprecision(1) << data.localPosition.x;
        j << ",\"lz\":" << data.localPosition.z;
        j << ",\"ln\":\"" << data.localName << "\"";
        j << ",\"e\":[";
        for (int i = 0; i < data.enemyCount; i++) {
            if (i > 0) j << ",";
            j << "[" << std::fixed << std::setprecision(1) << data.enemyPositions[i].x;
            j << "," << data.enemyPositions[i].z;
            j << ",\"" << data.enemyNames[i] << "\"]";
        }
        j << "],\"t\":[";
        for (int i = 0; i < data.teamCount; i++) {
            if (i > 0) j << ",";
            j << "[" << std::fixed << std::setprecision(1) << data.teamPositions[i].x;
            j << "," << data.teamPositions[i].z;
            j << ",\"" << data.teamNames[i] << "\"]";
        }
        j << "],\"g\":[";
        for (int i = 0; i < data.grenadeCount; i++) {
            if (i > 0) j << ",";
            j << "[" << std::fixed << std::setprecision(1) << data.grenadePositions[i].x;
            j << "," << data.grenadePositions[i].z;
            j << "," << std::setprecision(0) << data.grenadeTypes[i] << "]";
        }
        j << "]";
        // Planted bomb
        if (data.bombPlanted) {
            j << ",\"b\":[" << std::fixed << std::setprecision(1) << data.bombPosition.x;
            j << "," << data.bombPosition.z;
            j << "," << data.bombTimer << "]";
        }
        j << "}";
        return j.str();
    }
    
    void MqttLoop() {
        while (running) {
            if (!MqttConnect()) {
                std::cout << "[CloudRadar] MQTT connect failed, retry in 3s" << std::endl;
                sleep(3);
                continue;
            }
            
            connected = true;
            std::cout << "[CloudRadar] Connected to HiveMQ!" << std::endl;
            
            std::string topic = "radar/" + roomCode;
            int pingCounter = 0;
            
            while (running && connected) {
                std::string payload = BuildCompactJSON();
                if (!MqttPublish(topic, payload)) {
                    std::cout << "[CloudRadar] Publish failed" << std::endl;
                    connected = false;
                    break;
                }
                
                // Send PINGREQ every ~20 seconds (400 iterations * 50ms)
                pingCounter++;
                if (pingCounter >= 400) {
                    pingCounter = 0;
                    if (!MqttPing()) {
                        std::cout << "[CloudRadar] Ping failed" << std::endl;
                        connected = false;
                        break;
                    }
                }
                
                usleep(50000); // 50ms = 20 updates per second
            }
            
            close(sock);
            sock = -1;
        }
    }
    
    bool MqttPing() {
        if (sock < 0) return false;
        
        // PINGREQ packet: 0xC0 0x00
        uint8_t ping[2] = {0xC0, 0x00};
        int sent = send(sock, ping, 2, 0);
        if (sent <= 0) return false;
        
        // Read PINGRESP (non-blocking, just clear buffer)
        uint8_t resp[2];
        recv(sock, resp, 2, MSG_DONTWAIT);
        
        return true;
    }
    
    bool MqttConnect() {
        // HiveMQ public broker - plain TCP on port 1883
        const char* host = "broker.hivemq.com";
        
        struct hostent* he = gethostbyname(host);
        if (!he) {
            std::cout << "[CloudRadar] DNS failed for " << host << std::endl;
            return false;
        }
        
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) return false;
        
        // Set short socket timeout for faster failure detection
        struct timeval tv;
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        
        // Disable Nagle's algorithm for lower latency
        int nodelay = 1;
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
        
        // Enable TCP keepalive
        int keepalive = 1;
        int keepidle = 10;
        int keepintvl = 5;
        int keepcnt = 3;
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));
        
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(1883);
        memcpy(&addr.sin_addr, he->h_addr, he->h_length);
        
        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(sock);
            sock = -1;
            return false;
        }
        
        // MQTT CONNECT packet
        std::string clientId = "radar_" + roomCode;
        
        uint8_t connectPacket[256];
        int pos = 0;
        
        // Fixed header
        connectPacket[pos++] = 0x10; // CONNECT
        int remainingPos = pos++;    // Length placeholder
        
        // Protocol name
        connectPacket[pos++] = 0x00;
        connectPacket[pos++] = 0x04;
        connectPacket[pos++] = 'M';
        connectPacket[pos++] = 'Q';
        connectPacket[pos++] = 'T';
        connectPacket[pos++] = 'T';
        
        // Protocol level (4 = 3.1.1)
        connectPacket[pos++] = 0x04;
        
        // Connect flags (clean session)
        connectPacket[pos++] = 0x02;
        
        // Keep alive (30 seconds) - shorter for more reliable connection
        connectPacket[pos++] = 0x00;
        connectPacket[pos++] = 0x1E;
        
        // Client ID
        connectPacket[pos++] = 0x00;
        connectPacket[pos++] = clientId.length();
        memcpy(&connectPacket[pos], clientId.c_str(), clientId.length());
        pos += clientId.length();
        
        // Set remaining length
        connectPacket[remainingPos] = pos - 2;
        
        if (send(sock, connectPacket, pos, 0) <= 0) return false;
        
        // Read CONNACK
        uint8_t connack[4];
        if (recv(sock, connack, 4, 0) < 4) return false;
        
        if (connack[0] != 0x20 || connack[3] != 0x00) {
            std::cout << "[CloudRadar] CONNACK error: " << (int)connack[3] << std::endl;
            return false;
        }
        
        return true;
    }
    
    bool MqttPublish(const std::string& topic, const std::string& payload) {
        if (sock < 0) return false;
        
        std::vector<uint8_t> packet;
        
        // Fixed header - PUBLISH, QoS 0
        packet.push_back(0x30);
        
        // Remaining length
        int remainingLen = 2 + topic.length() + payload.length();
        if (remainingLen < 128) {
            packet.push_back(remainingLen);
        } else {
            packet.push_back((remainingLen % 128) | 0x80);
            packet.push_back(remainingLen / 128);
        }
        
        // Topic
        packet.push_back(0x00);
        packet.push_back(topic.length());
        for (char c : topic) packet.push_back(c);
        
        // Payload
        for (char c : payload) packet.push_back(c);
        
        int sent = send(sock, packet.data(), packet.size(), 0);
        return sent > 0;
    }
};

inline CloudRadar cloudRadar;
