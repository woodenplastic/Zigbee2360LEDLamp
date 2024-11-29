#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <WiFi.h>

#ifdef ETH_ENABLED
#include <ETH.h>
#endif

#include <ArduinoJson.h>
#include <IPAddress.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
extern WiFiUDP Udp;

#include "configData.h"

#define NET_TIMEOUT_MS 20000
#define WIFI_RECOVER_TIME_MS 30000

// WiFi event handler
void WiFiEventHandler(WiFiEvent_t event)
{
    switch (event)
    {
#ifdef ETH_ENABLED
    case ARDUINO_EVENT_ETH_START:
        Serial.printf("[NETWORK] ETH Started\n");
        ETH.setHostname(config.hardware.devicename);
        break;
    case ARDUINO_EVENT_ETH_CONNECTED:
        break;
    case ARDUINO_EVENT_ETH_GOT_IP:
        Serial.print("ETH MAC: ");
        Serial.println(ETH.macAddress());
        Serial.print("IPv4: ");
        Serial.println(ETH.localIP());
        if (ETH.fullDuplex())
        {
            Serial.print("FULL_DUPLEX");
        }
        Serial.print(" ");
        Serial.print(ETH.linkSpeed());
        Serial.println("Mbps");

    case ARDUINO_EVENT_ETH_DISCONNECTED:
        Serial.printf("[NETWORK] ETH Disconnected\n");
        break;
    case ARDUINO_EVENT_ETH_STOP:
        Serial.printf("[NETWORK] ETH Stopped\n");
        break;
#endif

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

class NetworkManager
{
public:
    // Static method to get the singleton instance
    static NetworkManager &getInstance()
    {
        static NetworkManager instance;
        return instance;
    }

    void init()
    {

        WiFi.onEvent(WiFiEventHandler);

#ifdef ETH_ENABLED
        if (config.network.useStaticEth)
        {
            ETH.config(config.network.eth_ip, config.network.eth_gw, config.network.eth_subnet, config.network.eth_dns);
        }

        ETH.begin();
#endif

        WiFi.mode(WIFI_STA);
        WiFi.setHostname(config.hardware.devicename);

        Udp.begin(udpPort);
        // Udp2.begin(udp2Port);

        if (MDNS.begin(config.hardware.devicename))
        {
            MDNS.addService("apple-midi", "udp", 5004);
            MDNS.addService("http", "tcp", 80);
            Serial.printf("[NETWORK] MDNS: started\n");
        }
        else
        {
            Serial.printf("[NETWORK] MDNS: failed to start\n");
        }

        // Create the DNS task but immediately suspend until we're in WiFi AP mode
        // dnsServer.start(DNS_PORT, "*", apIP);
        // xTaskCreate(DNSprocessNextRequest, "DNSprocessNextRequest", 4096, NULL, 1, &DNSprocessNextRequestHandle);
        // vTaskSuspend(DNSprocessNextRequestHandle);
        // dnsServer.stop();

        // Create event group and task
        networkEventGroup = xEventGroupCreate();
        xTaskCreate(checkConnectionsTask, "checkConnectionsTask", 4096, this, 1, &checkNetworkHandle);

        // Set the initial bit to trigger the task
        xEventGroupSetBits(networkEventGroup, MANUAL_TRIGGER_BIT);
    }

    void DNSprocessNextRequest()
    {
        dnsServer.processNextRequest();
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    String scanWifi()
    {
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

    void freshConnect()
    {
        if (networkEventGroup != nullptr)
        {
            if (apConnected())
                WiFi.softAPdisconnect();
            xEventGroupSetBits(networkEventGroup, MANUAL_TRIGGER_BIT);
        }
    }

    const IPAddress broadcastIp = IPAddress(255, 255, 255, 255);

    bool wifiConnected()
    {
        return WiFi.isConnected();
    }

    bool isEthConnected = false;
#ifdef ETH_ENABLED
    bool ethConnected()
    {
        return ETH.localIP() != IPAddress(0, 0, 0, 0);
    }
#endif

    bool apConnected()
    {
        return WiFi.softAPgetStationNum() > 0;
    }

    IPAddress getIP()
    {

#ifdef ETH_ENABLED
        if (isEthConnected)
        {
            return ETH.localIP();
        }
#endif

        if (wifiConnected())
        {
            return WiFi.localIP();
        }
        else
        {
            return apIP;
        }
    }

private:
    // Constructor and destructor are private to enforce singleton
    NetworkManager() = default;
    ~NetworkManager()
    {
        stopTask();
    }

    // Delete copy and move constructors
    NetworkManager(const NetworkManager &) = delete;
    NetworkManager &operator=(const NetworkManager &) = delete;

    DNSServer dnsServer;

    TaskHandle_t checkNetworkHandle = nullptr;
    EventGroupHandle_t networkEventGroup = nullptr;
    const int MANUAL_TRIGGER_BIT = BIT0;

    // Private methods

    void check()
    {
#ifdef ETH_ENABLED
        isEthConnected = ethConnected(); // Check Ethernet connectivity if enabled
#endif

        if (!isEthConnected && !apConnected() && !wifiConnected())
        {
            prepWifi();

            if (!connectSTA())
            {
                connectAP();
            }
        }
    }

    void prepWifi()
    {
        if (WiFi.getMode() == WIFI_STA)
        {
            WiFi.disconnect();
        }
        if (WiFi.getMode() == WIFI_AP)
        {
            WiFi.softAPdisconnect();
            WiFi.mode(WIFI_STA);
        }
        WiFi.mode(WIFI_STA);
    }

    bool connectSTA()
    {
        if (checkSSID(config.network.ssid))
        {
            if (config.network.useStaticWiFi)
            {
                WiFi.config(config.network.Swifi_ip, config.network.Swifi_gw, config.network.Swifi_subnet, config.network.Swifi_dns);
            }
            WiFi.begin(config.network.ssid, config.network.password);
            Serial.printf("[NETWORK] Connecting to %s\n", config.network.ssid);

            unsigned long startTime = millis();
            while (millis() - startTime < 10000)
            {
                if (WiFi.isConnected())
                {
                    Serial.printf("[NETWORK] Successfully connected to %s\n", config.network.ssid);
                    return true;
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            Serial.printf("[NETWORK] Failed to connect to %s, switching to AP mode\n", config.network.ssid);
        }
        else
        {
            Serial.printf("[NETWORK] No Saved WiFi SSID, starting AP mode\n");
        }
        return false;
    }

    void connectAP()
    {
        if (WiFi.getMode() == WIFI_STA)
        {
            WiFi.mode(WIFI_AP);
        }
        if (config.network.useStaticWiFi)
        {
            WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
        }
        WiFi.softAP(config.hardware.devicename, softAP_password);
    }

    bool checkSSID(const char *ssidToFind)
    {
        if (ssidToFind == nullptr || *ssidToFind == '\0')
        {
            return false;
        }
        int scanAttempts = 0;
        while (scanAttempts < 3)
        {
            int networkCount = WiFi.scanNetworks();
            for (size_t i = 0; i < networkCount; ++i)
            {
                if (strcmp(WiFi.SSID(i).c_str(), ssidToFind) == 0)
                {
                    return true;
                }
            }
            delay(500);
            scanAttempts++;
        }
        return false;
    }

    static void checkConnectionsTask(void *parameter)
    {
        NetworkManager *self = static_cast<NetworkManager *>(parameter);
        for (;;)
        {
            EventBits_t bits = xEventGroupWaitBits(
                self->networkEventGroup,
                self->MANUAL_TRIGGER_BIT,
                pdTRUE,
                pdFALSE,
                pdMS_TO_TICKS(30000));

            if (bits & self->MANUAL_TRIGGER_BIT)
            {
                self->check(); // Call the internal check method
            }

            vTaskDelay(pdMS_TO_TICKS(1000)); // Adjust delay as needed
        }
    }

    void stopTask()
    {
        if (checkNetworkHandle != nullptr)
        {
            vTaskDelete(checkNetworkHandle);
            checkNetworkHandle = nullptr;
        }
        if (networkEventGroup != nullptr)
        {
            vEventGroupDelete(networkEventGroup);
            networkEventGroup = nullptr;
        }
    }

    const unsigned int udpPort = 53000;
    const unsigned int udp2Port = 53001;
    const char *softAP_password = "12345678";
    const int maxScanAttempts = 1;
    const int scanInterval = 3000;
    const uint16_t DNS_PORT = 53;
    const IPAddress apIP = IPAddress(192, 168, 4, 1);
    const IPAddress netMsk = IPAddress(255, 255, 255, 0);
    const IPAddress subnet = IPAddress(255, 255, 255, 0);
};

#endif // NETWORK_MANAGER_H
