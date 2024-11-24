// NetworkManager.cpp
#include "NetworkManager.h"
//#include "midiManager.h"

NetworkManager::NetworkManager(ConfigManager& configManager)
    : config(configManager) {}  // Initialize the config reference


void NetworkManager::DNSprocessNextRequest(void* parameter) {
    for (;;) {
        dnsServer.processNextRequest();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

bool NetworkManager::checkSSID(const char* ssidToFind) {
    if (ssidToFind == nullptr || *ssidToFind == '\0') {
        return false;
    }
    int scanAttempts = 0;
    while (scanAttempts < 3) {
        int networkCount = WiFi.scanNetworks();
        for (size_t i = 0; i < networkCount; ++i) {
            if (strcmp(WiFi.SSID(i).c_str(), ssidToFind) == 0) {
                return true;
            }
        }
        delay(500);
        scanAttempts++;
    }
    return false;
}

void NetworkManager::WiFiEvent(WiFiEvent_t event) {
  switch (event)
  {
    // WiFi event cases
  case ARDUINO_EVENT_WIFI_READY:
    Serial.printf("WiFi Ready\n");
    break;
  case ARDUINO_EVENT_WIFI_SCAN_DONE:
    Serial.printf("WiFi Scan Done\n");
    break;
  case ARDUINO_EVENT_WIFI_STA_START:
    Serial.printf("WiFi STA Started\n");
    break;
  case ARDUINO_EVENT_WIFI_STA_STOP:
    Serial.printf("WiFi STA Stopped\n");
    break;
  case ARDUINO_EVENT_WIFI_STA_CONNECTED:
    Serial.printf("WiFi Connected\n");
    break;
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    Serial.printf("WiFi Disconnected\n");
    break;
  case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
    Serial.printf("WiFi Auth Mode Changed\n");
    break;
  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    Serial.printf("WiFi Got IP: %s\n", WiFi.localIP().toString().c_str());
    config.network.wifi_ip = WiFi.localIP();
    break;
  case ARDUINO_EVENT_WIFI_STA_LOST_IP:
    Serial.printf("WiFi Lost IP\n");
    break;
  case ARDUINO_EVENT_WIFI_AP_START:
    Serial.printf("WiFi AP Started\n");
    break;
  case ARDUINO_EVENT_WIFI_AP_STOP:
    Serial.printf("WiFi AP Stopped\n");
    break;
  case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
    Serial.printf("Station Connected to WiFi AP\n");
    break;
  case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
    Serial.printf("Station Disconnected from WiFi AP\n");
    break;
  case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
    Serial.printf("Station IP Assigned in WiFi AP Mode\n");
    config.network.wifi_ip = WiFi.softAPIP();
    break;
  case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED:
    Serial.printf("Probe Request Received in WiFi AP Mode\n");
    break;
    // [You can add other WiFi events here as needed]
  default:
    break;
  }
}

String NetworkManager::scanWifi() {
  int numNetworks = WiFi.scanNetworks();
  if (numNetworks == WIFI_SCAN_FAILED)
  {
    Serial.printf("Wi-Fi scan failed!\n");
    return "{}";
  }
  else if (numNetworks == 0)
  {
    Serial.printf("No networks found\n");
    return "[]";
  }

  JsonDocument jsonDoc; // Adjust the size based on the expected number of networks
  JsonArray networks = jsonDoc.to<JsonArray>();

  for (size_t i = 0; i < numNetworks; ++i)
  {
    JsonObject network = networks.add<JsonObject>();
    network["ssid"] = WiFi.SSID(i);
    network["rssi"] = WiFi.RSSI(i);
    network["channel"] = WiFi.channel(i);
    network["encryption"] = WiFi.encryptionType(i);
  }

  String jsonResult;
  serializeJson(jsonDoc, jsonResult);
  Serial.printf("Scanned Wi-Fi Networks:%s\n", jsonResult.c_str());

  return jsonResult;
}

void NetworkManager::prepWifi() {
    if (WiFi.getMode() == WIFI_STA) {
        WiFi.disconnect();
    }
    if (WiFi.getMode() == WIFI_AP) {
        WiFi.softAPdisconnect();
        WiFi.mode(WIFI_STA);
    }
    WiFi.mode(WIFI_STA);
}

bool NetworkManager::connectSTA() {
    if (checkSSID(config.network.ssid)) {
        if (config.network.useStaticWiFi) {
            WiFi.config(config.network.Swifi_ip, config.network.Swifi_gw, config.network.Swifi_subnet, config.network.Swifi_dns);
        }
        WiFi.begin(config.network.ssid, config.network.password);
        Serial.printf("Connecting to %s\n", config.network.ssid);

        unsigned long startTime = millis();
        while (millis() - startTime < 10000) {
            if (WiFi.isConnected()) {
                Serial.printf("Successfully connected to %s\n", config.network.ssid);
                return true;
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        Serial.printf("Failed to connect to %s, switching to AP mode\n", config.network.ssid);
    } else {
        Serial.printf("No Saved WiFi SSID, starting AP mode\n");
    }
    return false;
}

void NetworkManager::connectAP() {
    if (WiFi.getMode() == WIFI_STA) {
        WiFi.mode(WIFI_AP);
    }
    if (config.network.useStaticWiFi) {
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    }
    WiFi.softAP("ALMALOOX", softAP_password);
}


void NetworkManager::init() {

    WiFi.onEvent(std::bind(&NetworkManager::WiFiEvent, this, std::placeholders::_1));


  WiFi.mode(WIFI_STA);
  WiFi.setHostname("ALMALOOX");

  if (MDNS.begin("ALMALOOX")) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("MDNS: started");

  }
  else {
    Serial.printf("MDNS: failed to start\n");
  }

  //Create the DNS task but immediately suspend until we're in WiFi AP mode
  //dnsServer.start(DNS_PORT, "*", apIP);
  //xTaskCreate(DNSprocessNextRequest, "DNSprocessNextRequest", 4096, NULL, 1, &DNSprocessNextRequestHandle);
  //vTaskSuspend(DNSprocessNextRequestHandle);
  //dnsServer.stop();




}

