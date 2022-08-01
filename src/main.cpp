#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <esp_sntp.h>
#include <time.h>
#include <EEPROM.h>
#include "VVeggies.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

void notifyClients(AsyncWebSocketClient *client);
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void initWebSocket();
void onStateChange(void);

VVeggies v(onStateChange);

void onStateChange() {
  notifyClients(NULL);
}

void notifyClients(AsyncWebSocketClient *client) {
  if (client) {
    client->text(v.getStateJson());
  } else {
    ws.textAll(v.getStateJson());
  }
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  char buffer[256] = {};
  char * tokData[2];
  if (len > 256) return;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0; // Null Terminates String
    strcpy(buffer,(const char *)data);
    tokData[0] = strtok(buffer,":");
    tokData[1] = strtok(NULL, ":");
    if (tokData[0]) {
      v.processCommand(tokData[0],tokData[1]);
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      notifyClients(client);
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup()
{
    Serial.begin(115200);
    EEPROM.begin(512);
    // Set WiFi to station mode and disconnect from an AP if it was previously connected
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.enableLongRange(true);
    
    bool networkFound = false;
    int netId = 0;
    while (!networkFound) {
      Serial.println("Scan start");
      int n = WiFi.scanNetworks();
      Serial.println("Looking for Known Network");
      if (n == 0) {
          Serial.println("No Networks found");
      } else {
          Serial.print(n);
          Serial.println(" Networks found");
          for (int i = 0; i < n; ++i) {
              Serial.println(WiFi.SSID(i));
              if (!WiFi.SSID(i).compareTo("Vella Home")) {
                networkFound = true;
                netId = i;
                Serial.println("^^^^^^^^^^ - Known network found!");
              }
          }
      }
    }

    // netId has the network attempt connection
    do {
      Serial.println("Attemping to Connect...");
      WiFi.begin("Vella Home","Alexandria11");
    } while (WiFi.waitForConnectResult() != WL_CONNECTED);
    Serial.println("Connected!");
    Serial.println(String("Local IP: ") + WiFi.localIP().toString());
    Serial.println(String("Local DNS: ") + WiFi.dnsIP().toString());

    Serial.println("Setting up Time Server & Starting Web Socket...");
    initWebSocket();
    configTime(-18000,3600,"pool.ntp.org");

    server.on("/",HTTP_GET,[&](AsyncWebServerRequest * request) {
      request->redirect(String("http://localhost:8080/?vv=") + WiFi.localIP().toString());
    });

    server.begin();
  
    tm time;
    do {
      delay(1000);
      Serial.println("Getting Time...");
    } while (!getLocalTime(&time));

    char buffer[200];
    strftime(buffer,200,"%A - %m/%d/%Y %I:%M:%S %p", &time);
    Serial.println(buffer);

    v.updateInternalStateFromEEPROM();

    Serial.println("Setup complete. Handing off to main loop...");
}

void loop()
{
    v.doTasks();
    ws.cleanupClients();
}
