#ifndef CONFIGDATA_H
#define CONFIGDATA_H

#include <ArduinoJson.h>
#include <vector>

// Define the ledData struct
struct ledData {
    uint8_t warmPin = 0;
    uint8_t coldPin = 0;
    uint32_t warmCycle = 1024;
    uint32_t coldCycle = 1024;
    uint8_t coldChannel = 0;
    uint8_t warmChannel = 0;
};

// Define the ConfigData class
class ConfigData {
public:
    char ssid[30] = "";
    char password[30] = "";
    char devicename[20] = "";

    IPAddress wifi_ip;
    IPAddress wifi_gw;
    IPAddress wifi_dns;
    IPAddress wifi_subnet;

    IPAddress Swifi_ip;
    IPAddress Swifi_gw;
    IPAddress Swifi_dns;
    IPAddress Swifi_subnet;

    bool useStaticWiFi = false;
    bool useStaticEth = false;
    bool ModulErrorLittleFS = false;

    bool connectedToAP = false;

    ConfigData() = default;

    void make(JsonDocument& doc);
    JsonDocument get();
    void save(const char* filePath);
    void load(const char* filePath);

    // Function to get a pointer to ledData at a specific index
    ledData* getLedData(size_t index);

private:
    int calculateState(bool in, bool out);
    std::vector<ledData> lampdata;
};

extern ConfigData configData;

#endif // CONFIGDATA_H
