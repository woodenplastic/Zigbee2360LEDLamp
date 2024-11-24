#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <IPAddress.h>
#include <DNSServer.h>
#include <ESPmDNS.h>

#define NET_TIMEOUT_MS 20000 
#define WIFI_RECOVER_TIME_MS 30000 

#include "configData.h"

class NetworkManager {
public:
    NetworkManager(ConfigManager& configManager); // Constructor to accept config
    void init();
    void DNSprocessNextRequest(void* parameter);
    bool checkSSID(const char* ssidToFind);
    void WiFiEvent(WiFiEvent_t event);
    String scanWifi();
    void prepWifi();
    bool connectSTA();
    void connectAP();

    const unsigned int udpPort = 53000;
    const unsigned int udp2Port = 53001;

    const IPAddress broadcastIp = IPAddress(255, 255, 255, 255);
    const IPAddress apIP = IPAddress(192, 168, 4, 1);
    const IPAddress netMsk = IPAddress(255, 255, 255, 0);
    const IPAddress subnet = IPAddress(255, 255, 255, 0);
    const char* softAP_password = "12345678";
    const int scanInterval = 3000;
    const uint16_t DNS_PORT = 53;

private:
    DNSServer dnsServer;
    ConfigManager& config; // Reference to ConfigManager instance
};

#endif // NETWORK_MANAGER_H
