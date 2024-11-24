#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H


#include <WiFi.h>
#include <ArduinoJson.h>
#include <IPAddress.h>
#include <DNSServer.h>
#include <ESPmDNS.h>

#include "configData.h"

#define NET_TIMEOUT_MS 20000 
#define WIFI_RECOVER_TIME_MS 30000 



class NetworkManager {
public:
    // Static method to get the singleton instance
    static NetworkManager& getInstance() {
        static NetworkManager instance;
        return instance;
    }

    void check() {
        if (WiFi.softAPgetStationNum() == 0 && !WiFi.isConnected()) {
            prepWifi();

            if (!connectSTA()) {
                connectAP();
            }
        }
    }

    void init() {

        WiFi.onEvent(WiFiEventHandler);

        WiFi.mode(WIFI_STA);
        WiFi.setHostname(config.hardware.devicename);

        if (MDNS.begin(config.hardware.devicename)) {
            MDNS.addService("http", "tcp", 80);
            Serial.printf("[NETWORK] MDNS: started\n");

        }
        else {
            Serial.printf("[NETWORK] MDNS: failed to start\n");
        }

        //Create the DNS task but immediately suspend until we're in WiFi AP mode
        //dnsServer.start(DNS_PORT, "*", apIP);
        //xTaskCreate(DNSprocessNextRequest, "DNSprocessNextRequest", 4096, NULL, 1, &DNSprocessNextRequestHandle);
        //vTaskSuspend(DNSprocessNextRequestHandle);
        //dnsServer.stop();


    }


    void DNSprocessNextRequest(void* parameter) {
        for (;;) {
            dnsServer.processNextRequest();
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    String scanWifi() {
        int numNetworks = WiFi.scanNetworks();
        if (numNetworks == WIFI_SCAN_FAILED)
        {
            Serial.printf("[NETWORK] Wi-Fi scan failed!\n");
            return "{}";
        }
        else if (numNetworks == 0)
        {
            Serial.printf("[NETWORK] No networks found\n");
            return "[]";
        }

        JsonDocument doc;
        JsonArray networks = doc.to<JsonArray>();

        for (size_t i = 0; i < numNetworks; ++i)
        {
            JsonObject network = networks.add<JsonObject>();
            network["ssid"] = WiFi.SSID(i);
            network["rssi"] = WiFi.RSSI(i);
            network["channel"] = WiFi.channel(i);
            network["encryption"] = WiFi.encryptionType(i);
        }

        String buf;
        serializeJson(doc, buf);
        Serial.printf("[NETWORK] Scanned Wi-Fi Networks:%s\n", buf.c_str());

        return buf;
    }



    const unsigned int udpPort = 53000;
    const unsigned int udp2Port = 53001;
    const IPAddress broadcastIp = IPAddress(255, 255, 255, 255);
    const IPAddress apIP = IPAddress(192, 168, 4, 1);
    const IPAddress netMsk = IPAddress(255, 255, 255, 0);
    const IPAddress subnet = IPAddress(255, 255, 255, 0);
    const char* softAP_password = "12345678";
    const int maxScanAttempts = 1;
    const int scanInterval = 3000;
    const uint16_t DNS_PORT = 53;

private:

    // Constructor and destructor are private to enforce singleton
    NetworkManager() = default;
    ~NetworkManager() = default;

    // Delete copy and move constructors
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    DNSServer dnsServer;

    // Private methods
    void prepWifi() {
        if (WiFi.getMode() == WIFI_STA) {
            WiFi.disconnect();
        }
        if (WiFi.getMode() == WIFI_AP) {
            WiFi.softAPdisconnect();
            WiFi.mode(WIFI_STA);
        }
        WiFi.mode(WIFI_STA);
    }

    bool connectSTA() {
        if (checkSSID(config.network.ssid)) {
            if (config.network.useStaticWiFi) {
                WiFi.config(config.network.Swifi_ip, config.network.Swifi_gw, config.network.Swifi_subnet, config.network.Swifi_dns);
            }
            WiFi.begin(config.network.ssid, config.network.password);
            Serial.printf("[NETWORK] Connecting to %s\n", config.network.ssid);

            unsigned long startTime = millis();
            while (millis() - startTime < 10000) {
                if (WiFi.isConnected()) {
                    Serial.printf("[NETWORK] Successfully connected to %s\n", config.network.ssid);
                    return true;
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            Serial.printf("[NETWORK] Failed to connect to %s, switching to AP mode\n", config.network.ssid);
        }
        else {
            Serial.printf("[NETWORK] No Saved WiFi SSID, starting AP mode\n");
        }
        return false;
    }

    void connectAP() {
        if (WiFi.getMode() == WIFI_STA) {
            WiFi.mode(WIFI_AP);
        }
        if (config.network.useStaticWiFi) {
            WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
        }
        WiFi.softAP(config.hardware.devicename, softAP_password);
    }

    bool checkSSID(const char* ssidToFind) {
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

    // WiFi event handler
    static void WiFiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info) {
        switch (event)
        {
            // WiFi event cases
        case ARDUINO_EVENT_WIFI_READY:
            Serial.printf("[NETWORK] WiFi Ready\n");
            break;
        case ARDUINO_EVENT_WIFI_SCAN_DONE:
            Serial.printf("[NETWORK] WiFi Scan Done\n");
            break;
        case ARDUINO_EVENT_WIFI_STA_START:
            Serial.printf("[NETWORK] WiFi STA Started\n");
            break;
        case ARDUINO_EVENT_WIFI_STA_STOP:
            Serial.printf("[NETWORK] WiFi STA Stopped\n");
            break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.printf("[NETWORK] WiFi Connected\n");
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.printf("[NETWORK] WiFi Disconnected\n");
            break;
        case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
            Serial.printf("[NETWORK] WiFi Auth Mode Changed\n");
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.printf("[NETWORK] WiFi Got IP: %s\n", WiFi.localIP().toString().c_str());
            config.network.wifi_ip = WiFi.localIP();
            break;
        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
            Serial.printf("[NETWORK] WiFi Lost IP\n");
            break;
        case ARDUINO_EVENT_WIFI_AP_START:
            Serial.printf("[NETWORK] WiFi AP Started\n");
            break;
        case ARDUINO_EVENT_WIFI_AP_STOP:
            Serial.printf("[NETWORK] WiFi AP Stopped\n");
            break;
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            Serial.printf("[NETWORK] Station Connected to WiFi AP\n");
            break;
        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            Serial.printf("[NETWORK] Station Disconnected from WiFi AP\n");
            break;
        case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
            Serial.printf("[NETWORK] Station IP Assigned in WiFi AP Mode\n");
            config.network.wifi_ip = WiFi.softAPIP();
            break;
        case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED:
            Serial.printf("[NETWORK] Probe Request Received in WiFi AP Mode\n");
            break;
            // [You can add other WiFi events here as needed]
        default:
            break;
        }
    }


};

#endif // NETWORK_MANAGER_H

















