#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <mutex>
#include "math/Vector3.h"

class WebRadar {
public:
    std::atomic<bool> running{false};
    std::atomic<bool> enabled{false};
    int port = 8080;
    std::string localIP;
    
    // Radar data (updated from main loop)
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
    
    void Start() {
        if (running) return;
        
        localIP = GetLocalIP();
        running = true;
        serverThread = std::thread(&WebRadar::ServerLoop, this);
        serverThread.detach();
        
        // Print instructions
        std::cout << "=== WEB RADAR STARTED ===" << std::endl;
        std::cout << "URL: http://" << localIP << ":" << port << std::endl;
        std::cout << "Share with friends on same WiFi!" << std::endl;
        std::cout << "=========================" << std::endl;
    }
    
    void Stop() {
        running = false;
        if (serverSocket >= 0) {
            close(serverSocket);
            serverSocket = -1;
        }
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
    
    std::string GetURL() const {
        return "http://" + localIP + ":" + std::to_string(port);
    }
    
private:
    std::thread serverThread;
    int serverSocket = -1;
    
    std::string GetLocalIP() {
        std::string ip = "127.0.0.1";
        
        // Try to get IP by creating a UDP socket
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) return ip;
        
        struct sockaddr_in serv;
        memset(&serv, 0, sizeof(serv));
        serv.sin_family = AF_INET;
        serv.sin_addr.s_addr = inet_addr("8.8.8.8");
        serv.sin_port = htons(53);
        
        if (connect(sock, (struct sockaddr*)&serv, sizeof(serv)) == 0) {
            struct sockaddr_in name;
            socklen_t namelen = sizeof(name);
            if (getsockname(sock, (struct sockaddr*)&name, &namelen) == 0) {
                char buf[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &name.sin_addr, buf, sizeof(buf));
                ip = buf;
            }
        }
        
        close(sock);
        return ip;
    }
    
    std::string GetRadarJSON() {
        std::lock_guard<std::mutex> lock(dataMutex);
        std::ostringstream json;
        json << "{";
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
    
    std::string GetRadarHTML() {
        return R"(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Web Radar</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#1a1a2e;display:flex;justify-content:center;align-items:center;min-height:100vh;font-family:Arial}
#radar{background:#0f0f1a;border:2px solid #4a4a6a;border-radius:8px}
.info{color:#888;text-align:center;margin-top:10px;font-size:14px}
.status{color:#0f0;font-size:12px}
</style>
</head>
<body>
<div>
<canvas id="radar" width="400" height="400"></canvas>
<div class="info">Web Radar <span class="status" id="status">Connecting...</span></div>
</div>
<script>
const canvas=document.getElementById('radar');
const ctx=canvas.getContext('2d');
const status=document.getElementById('status');
const size=400;

function draw(d){
    ctx.fillStyle='#0f0f1a';
    ctx.fillRect(0,0,size,size);
    ctx.strokeStyle='#4a4a6a';
    ctx.lineWidth=1;
    for(let i=1;i<4;i++){
        ctx.beginPath();
        ctx.moveTo(size*i/4,0);ctx.lineTo(size*i/4,size);
        ctx.moveTo(0,size*i/4);ctx.lineTo(size,size*i/4);
        ctx.stroke();
    }
    if(!d.valid){status.textContent='No map data';status.style.color='#f00';return}
    status.textContent='Connected';status.style.color='#0f0';
    function w2r(x,z){
        let rx=(x-d.camX)/d.camSize;
        let rz=(z-d.camZ)/d.camSize;
        return{x:size/2+rx*size/2,y:size/2-rz*size/2};
    }
    let lp=w2r(d.localX,d.localZ);
    ctx.fillStyle='#00ff00';
    ctx.beginPath();ctx.arc(lp.x,lp.y,8,0,Math.PI*2);ctx.fill();
    ctx.fillStyle='#ff0000';
    d.enemies.forEach(e=>{
        let ep=w2r(e.x,e.z);
        if(ep.x>=0&&ep.x<=size&&ep.y>=0&&ep.y<=size){
            ctx.beginPath();ctx.arc(ep.x,ep.y,8,0,Math.PI*2);ctx.fill();
        }
    });
}

function update(){
    fetch('/data').then(r=>r.json()).then(d=>{draw(d);setTimeout(update,100)})
    .catch(()=>{status.textContent='Disconnected';status.style.color='#f00';setTimeout(update,1000)});
}
update();
</script>
</body>
</html>)";
    }
    
    void HandleClient(int client) {
        char buffer[1024] = {0};
        read(client, buffer, sizeof(buffer) - 1);
        
        std::string request(buffer);
        std::string response;
        std::string body;
        std::string contentType;
        
        if (request.find("GET /data") != std::string::npos) {
            body = GetRadarJSON();
            contentType = "application/json";
        } else {
            body = GetRadarHTML();
            contentType = "text/html; charset=utf-8";
        }
        
        std::ostringstream resp;
        resp << "HTTP/1.1 200 OK\r\n";
        resp << "Content-Type: " << contentType << "\r\n";
        resp << "Content-Length: " << body.length() << "\r\n";
        resp << "Access-Control-Allow-Origin: *\r\n";
        resp << "Connection: close\r\n\r\n";
        resp << body;
        
        response = resp.str();
        write(client, response.c_str(), response.length());
        close(client);
    }
    
    void ServerLoop() {
        std::cout << "[WebRadar] Starting server on port " << port << std::endl;
        
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket < 0) {
            std::cout << "[WebRadar] Failed to create socket" << std::endl;
            running = false;
            return;
        }
        
        int opt = 1;
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        
        if (bind(serverSocket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cout << "[WebRadar] Failed to bind port " << port << std::endl;
            close(serverSocket);
            serverSocket = -1;
            running = false;
            return;
        }
        
        if (listen(serverSocket, 5) < 0) {
            std::cout << "[WebRadar] Failed to listen" << std::endl;
            close(serverSocket);
            serverSocket = -1;
            running = false;
            return;
        }
        
        std::cout << "[WebRadar] Server listening on 0.0.0.0:" << port << std::endl;
        
        while (running) {
            struct sockaddr_in clientAddr;
            socklen_t clientLen = sizeof(clientAddr);
            int client = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
            if (client >= 0) {
                std::cout << "[WebRadar] Client connected" << std::endl;
                HandleClient(client);
            }
        }
        
        std::cout << "[WebRadar] Server stopped" << std::endl;
    }
};

inline WebRadar webRadar;
