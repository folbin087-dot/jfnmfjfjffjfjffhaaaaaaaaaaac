#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <random>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "math/Vector3.h"

// Simple WebSocket client for PieSocket relay
class WebSocketRadar {
public:
    std::atomic<bool> running{false};
    std::atomic<bool> connected{false};
    
    // PieSocket config - user needs to set these
    std::string apiKey = "";
    std::string clusterId = "s1";  // default cluster
    std::string channelId = "";    // random generated
    
    struct RadarData {
        Vector3 localPosition;
        Vector3 cameraPosition;
        float cameraSize = 50.0f;
        bool mapDataValid = false;
        Vector3 enemyPositions[16];
        int enemyCount = 0;
    };
    
    RadarData data;
    std::mutex dataMutex;
    
    void GenerateChannelId() {
        static const char alphanum[] = "0123456789abcdef";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        
        channelId = "radar_";
        for (int i = 0; i < 8; i++) {
            channelId += alphanum[dis(gen)];
        }
    }
    
    std::string GetShareURL() const {
        // Returns URL to the viewer page
        return "https://piesocket.com/widget/" + apiKey + "/" + channelId;
    }
    
    std::string GetViewerHTML() const {
        std::ostringstream html;
        html << R"(<!DOCTYPE html>
<html><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Web Radar</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#1a1a2e;display:flex;flex-direction:column;justify-content:center;align-items:center;min-height:100vh;font-family:Arial}
#radar{background:#0f0f1a;border:2px solid #4a4a6a;border-radius:8px}
.info{color:#888;text-align:center;margin-top:10px;font-size:14px}
.status{font-size:12px}
.connected{color:#0f0}.disconnected{color:#f00}
</style>
</head><body>
<canvas id="radar" width="400" height="400"></canvas>
<div class="info">Web Radar <span class="status disconnected" id="status">Connecting...</span></div>
<script>
const canvas=document.getElementById('radar');
const ctx=canvas.getContext('2d');
const status=document.getElementById('status');
const size=400;

const ws=new WebSocket('wss://)" << clusterId << R"(.piesocket.com/v3/)" << channelId << R"(?api_key=)" << apiKey << R"(');

ws.onopen=()=>{status.textContent='Connected';status.className='status connected';};
ws.onclose=()=>{status.textContent='Disconnected';status.className='status disconnected';};
ws.onerror=()=>{status.textContent='Error';status.className='status disconnected';};

ws.onmessage=(e)=>{
    try{
        const d=JSON.parse(e.data);
        if(d.type==='radar')draw(d);
    }catch(err){}
};

function draw(d){
    ctx.fillStyle='#0f0f1a';ctx.fillRect(0,0,size,size);
    ctx.strokeStyle='#4a4a6a';ctx.lineWidth=1;
    for(let i=1;i<4;i++){
        ctx.beginPath();ctx.moveTo(size*i/4,0);ctx.lineTo(size*i/4,size);
        ctx.moveTo(0,size*i/4);ctx.lineTo(size,size*i/4);ctx.stroke();
    }
    if(!d.valid)return;
    function w2r(x,z){
        let rx=(x-d.camX)/d.camSize;
        let rz=(z-d.camZ)/d.camSize;
        return{x:size/2+rx*size/2,y:size/2-rz*size/2};
    }
    let lp=w2r(d.localX,d.localZ);
    ctx.fillStyle='#00ff00';ctx.beginPath();ctx.arc(lp.x,lp.y,8,0,Math.PI*2);ctx.fill();
    ctx.fillStyle='#ff0000';
    if(d.enemies)d.enemies.forEach(e=>{
        let ep=w2r(e.x,e.z);
        if(ep.x>=0&&ep.x<=size&&ep.y>=0&&ep.y<=size){
            ctx.beginPath();ctx.arc(ep.x,ep.y,8,0,Math.PI*2);ctx.fill();
        }
    });
}
</script></body></html>)";
        return html.str();
    }
    
    void Start() {
        if (running) return;
        if (apiKey.empty()) {
            std::cout << "[WebSocketRadar] API Key not set!" << std::endl;
            return;
        }
        
        if (channelId.empty()) {
            GenerateChannelId();
        }
        
        running = true;
        wsThread = std::thread(&WebSocketRadar::ConnectLoop, this);
        wsThread.detach();
        
        std::cout << "=== WEBSOCKET RADAR ===" << std::endl;
        std::cout << "Channel: " << channelId << std::endl;
        std::cout << "Share the viewer HTML with friends!" << std::endl;
        std::cout << "=======================" << std::endl;
    }
    
    void Stop() {
        running = false;
        connected = false;
    }
    
    void UpdateData(const Vector3& localPos, const Vector3& camPos, float camSize,
                    bool valid, const Vector3* enemies, int count) {
        std::lock_guard<std::mutex> lock(dataMutex);
        data.localPosition = localPos;
        data.cameraPosition = camPos;
        data.cameraSize = camSize;
        data.mapDataValid = valid;
        data.enemyCount = (count > 16) ? 16 : count;
        for (int i = 0; i < data.enemyCount; i++) {
            data.enemyPositions[i] = enemies[i];
        }
    }

private:
    std::thread wsThread;
    SSL_CTX* sslCtx = nullptr;
    SSL* ssl = nullptr;
    int sock = -1;
    
    std::string BuildJSON() {
        std::lock_guard<std::mutex> lock(dataMutex);
        std::ostringstream json;
        json << "{\"type\":\"radar\",";
        json << "\"valid\":" << (data.mapDataValid ? "true" : "false") << ",";
        json << "\"camX\":" << data.cameraPosition.x << ",";
        json << "\"camZ\":" << data.cameraPosition.z << ",";
        json << "\"camSize\":" << data.cameraSize << ",";
        json << "\"localX\":" << data.localPosition.x << ",";
        json << "\"localZ\":" << data.localPosition.z << ",";
        json << "\"enemies\":[";
        for (int i = 0; i < data.enemyCount; i++) {
            if (i > 0) json << ",";
            json << "{\"x\":" << data.enemyPositions[i].x
                 << ",\"z\":" << data.enemyPositions[i].z << "}";
        }
        json << "]}";
        return json.str();
    }
    
    void ConnectLoop() {
        while (running) {
            if (!Connect()) {
                std::cout << "[WebSocketRadar] Connection failed, retrying in 5s..." << std::endl;
                sleep(5);
                continue;
            }
            
            connected = true;
            std::cout << "[WebSocketRadar] Connected!" << std::endl;
            
            while (running && connected) {
                std::string json = BuildJSON();
                if (!SendFrame(json)) {
                    connected = false;
                    break;
                }
                usleep(100000); // 100ms update rate
            }
            
            Disconnect();
        }
    }
    
    bool Connect() {
        // Initialize SSL
        SSL_library_init();
        SSL_load_error_strings();
        sslCtx = SSL_CTX_new(TLS_client_method());
        if (!sslCtx) return false;
        
        // Resolve hostname
        std::string host = clusterId + ".piesocket.com";
        struct hostent* he = gethostbyname(host.c_str());
        if (!he) return false;
        
        // Create socket
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) return false;
        
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(443);
        memcpy(&addr.sin_addr, he->h_addr, he->h_length);
        
        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(sock);
            return false;
        }
        
        // SSL handshake
        ssl = SSL_new(sslCtx);
        SSL_set_fd(ssl, sock);
        SSL_set_tlsext_host_name(ssl, host.c_str());
        
        if (SSL_connect(ssl) <= 0) {
            SSL_free(ssl);
            close(sock);
            return false;
        }
        
        // WebSocket handshake
        std::string path = "/v3/" + channelId + "?api_key=" + apiKey;
        std::string key = "dGhlIHNhbXBsZSBub25jZQ=="; // base64 random
        
        std::ostringstream req;
        req << "GET " << path << " HTTP/1.1\r\n";
        req << "Host: " << host << "\r\n";
        req << "Upgrade: websocket\r\n";
        req << "Connection: Upgrade\r\n";
        req << "Sec-WebSocket-Key: " << key << "\r\n";
        req << "Sec-WebSocket-Version: 13\r\n\r\n";
        
        std::string request = req.str();
        SSL_write(ssl, request.c_str(), request.length());
        
        char response[1024];
        int len = SSL_read(ssl, response, sizeof(response) - 1);
        if (len <= 0) return false;
        response[len] = 0;
        
        if (strstr(response, "101") == nullptr) {
            std::cout << "[WebSocketRadar] Handshake failed" << std::endl;
            return false;
        }
        
        return true;
    }
    
    bool SendFrame(const std::string& data) {
        if (!ssl) return false;
        
        size_t len = data.length();
        std::vector<uint8_t> frame;
        
        // Text frame, FIN bit set
        frame.push_back(0x81);
        
        // Masked, length
        if (len < 126) {
            frame.push_back(0x80 | len);
        } else {
            frame.push_back(0x80 | 126);
            frame.push_back((len >> 8) & 0xFF);
            frame.push_back(len & 0xFF);
        }
        
        // Mask key
        uint8_t mask[4] = {0x12, 0x34, 0x56, 0x78};
        frame.insert(frame.end(), mask, mask + 4);
        
        // Masked payload
        for (size_t i = 0; i < len; i++) {
            frame.push_back(data[i] ^ mask[i % 4]);
        }
        
        int sent = SSL_write(ssl, frame.data(), frame.size());
        return sent > 0;
    }
    
    void Disconnect() {
        if (ssl) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
            ssl = nullptr;
        }
        if (sock >= 0) {
            close(sock);
            sock = -1;
        }
        if (sslCtx) {
            SSL_CTX_free(sslCtx);
            sslCtx = nullptr;
        }
        connected = false;
    }
};

inline WebSocketRadar wsRadar;
